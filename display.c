/*
 * Allwinner A64 Display Engine controller
 * 
 * Hacked together by Dave MacFarlane
 * Based off of https://lupyuen.github.io/articles/dsi#appendix-sequence-of-steps-for-pinephone-display-driver as a reference
 */
#define DEBUG(x) { /* iprint(x); */}
#include "u.h"
#include "../port/lib.h"

#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ccu.h"

#define Image IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"

#define R_PWM_CTRL_REG 0x0
#define	R_PWM_CH0_PERIOD 0x04

#define PIO_CFG_MASK(n) ~(7<<n)

#define PIO_PH_CFG01 0x100
#define	PIO_PH_DATA 0x10c

#define	RPIO_CFG10	0x04	/* Port L Configure Register 1 */

#define MIXER(i, n) (( (i+1)*0x100000)+ n)
#define VIDEO_SCALER_CH0(n) (MIXER(0, 0x20000) + n)
#define BLD(x) (MIXER(0, 0x1000) + x)
#define OVL_UI(i, reg) (MIXER(0, 0x2000 + 0x1000*i) + reg)
#define UI_SCALER(i, n) MIXER(0, 0x40000 + (0x10000*i) + n)
#define WIDTH 720
#define HEIGHT 1440

enum {
	SCLK_GATE = 0x0,
	HCLK_GATE = 0x4,
	AHB_RESET = 0x8,
	DE2TCON_MUX = 0x10,
};

/* Mixer global registers */
enum {
	GLB_CTL = 0x0,
	GLB_STS = 0x4,
	GLB_DBUFFER = 0x8,
	GLB_SIZE = 0xc,
};

/* BLD registers */
enum {
	BLD_FILLCOLOUR_CTL = 0x0,
	BLD_FILL_COLOUR = 0x4,
	BLD_CH_ISIZE = 0x8,
	BLD_CH_OFFSET = 0xc,
	BLD_CH_RTCTL = 0x80,
	BLD_PREMUL_CTL = 0x84,
	BLD_BK_COLOUR = 0x88,
	BLD_SIZE = 0x8c,
	BLD_CTL = 0x90,
};

/* Overlay registers */
enum {
	OVL_UI_ATTR_CTRL = 0x00,
	OVL_UI_MBSIZE = 0x4,
	OVL_UI_COORD = 0x8,
	OVL_UI_PITCH = 0xc,
	OVL_UI_TOP_LADD = 0x10,
	OVL_UI_FILL_COLOR = 0x18,
	OVL_UI_SIZE = 0x88,
};

/* Post processing registers */
enum {
	FCE = 0xa0000,
	BWS = 0xa2000,
	LTI = 0xa4000,
	PEAKING = 0xa6000,
	ASE = 0xa8000,
	FCC = 0xaa000,
	DRC = 0x1b0000, /* 0xb0000? */
};
static void
phywr(int offset, u32int val)
{
	*IO(u32int, (PHYSDEV + offset)) = val;
}

static u32int
piord(int offset)
{
	coherence();
	return *IO(u32int, (PIO + offset));
}
static void
piowr(int offset, u32int val)
{
	*IO(u32int, (PIO + offset)) = val;
	coherence();
}

static u32int
derd(int offset)
{
	coherence();
	return *IO(u32int, (PIO + offset));
}
static void
dewr(int offset, u32int val)
{
	*IO(u32int, (DE + offset)) = val;
	coherence();
}

static u32int
rpiord(int offset)
{
	coherence();
	return *IO(u32int, (R_PIO + offset));
}
static void
rpiowr(int offset, u32int val)
{
	*IO(u32int, (R_PIO + offset)) = val;
	coherence();
}

static u32int
pwmrd(int offset)
{
	coherence();
	return *IO(u32int, (PWM + offset));
}

static u32int
rpwmrd(int offset)
{
	coherence();
	return *IO(u32int, (R_PWM + offset));
}

static void
rpwmwr(int offset, u32int val)
{
	*IO(u32int, (R_PWM + offset)) = val;
	coherence();
}

static int
pwrwr(u8int reg, u8int val)
{
	coherence();
	return rsb_write(PMICRTA, PMICADDR, reg, val, 1);
}

/* Move to separate backlight file? */
int brightness = 100;

void backlight(int pct){
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*pct/100)&0xffff));
	brightness = pct;
}
/**
 * Configure the backlight pins for the Pinephone display
 */
static void
backlightinit(void)
{
	/* configure PL10 for PWM */
	rpiowr(RPIO_CFG10, rpiord(RPIO_CFG10) & PIO_CFG_MASK(8) | (2<<8));
	/* disable r_pwm_ctrl_reg (pg 194) */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) & ~(1<<6));
	
	/* configure r_pwm_ch0_period (pg 195) */
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*brightness/100)&0xffff));
	/* enable r_pwm_ctrl_reg */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) | (1<<6) | (1<<4) | 0xf);
	/* configure PH10 for PIO output */
	piowr(PIO_PH_CFG01, piord(PIO_PH_CFG01) & PIO_CFG_MASK(8) | (1<<8));

	/* turn on */
	piowr(PIO_PH_DATA, piord(PIO_PH_DATA) | (1<<10));
}


static void
dengineinit(void)
{
	DEBUG("sram to DMA\n");
	/* set sram to dma. FIXME: Check if this is necessary */
	phywr(0x4, 0); 
	clkenable(PLL_DE_CTRL_REG);

	/* Sets PLL_DE_CTRL_REG clock as side-effect */
	setclkrate(DE_CLK_REG, 297*Mhz);
	clkenable(DE_CLK_REG);
	if(openthegate("DE") == -1)
		panic("DE gate open failed\n");

	delay(100);
	coherence();
	dewr(SCLK_GATE, derd(SCLK_GATE) | 1);
	dewr(AHB_RESET, derd(AHB_RESET) | 1);
	dewr(HCLK_GATE, derd(HCLK_GATE) | 1);
	dewr(DE2TCON_MUX, derd(DE2TCON_MUX) & ~(1));

	for(int i = 0; i < 0x6000; i+= 4){
		dewr(MIXER(0, i), 0);
	}

	dewr(VIDEO_SCALER_CH0(0), 0);
	dewr(UI_SCALER(0, 0), 0);
	dewr(UI_SCALER(1, 0), 0);

	dewr(MIXER(0, FCE), derd(MIXER(0, FCE)) & ~1);
	dewr(MIXER(0, BWS), 0);
	dewr(MIXER(0, LTI), 0);
	dewr(MIXER(0, PEAKING), 0);
	dewr(MIXER(0, ASE), 0);
	dewr(MIXER(0, FCC), 0);
	dewr(MIXER(0, DRC), 0);

	dewr(MIXER(0, 0), derd(MIXER(0, 0)) | 1);
}

/* OVL_UI_ATTR_CTL constants */
#define XRGB (0x4<<8)
#define LAYER_EN 1
#define ALPHA_SHIFT 24
#define ALPHA_MIX (0x2<<1)

static void
overlayinit(void) {

	u32int *fb = screeninit(WIDTH, HEIGHT, 32);
	dewr(BLD(BLD_BK_COLOUR), 0xff000000);
	dewr(BLD(BLD_PREMUL_CTL), 0);
	/* disable ui overlay, channel 1-3 */
	for(int i = 0; i < 4; i++){
		dewr(OVL_UI(i, OVL_UI_ATTR_CTRL), 0);
		dewr(UI_SCALER(i, 0), 0);
	}

	/* Channel 0 is video. Channel 1-3 are overlays.
       We only use channel 1. */
	/* set overlay for channel 1. opaque, xrgb, alphamode 2, enable */
	dewr(OVL_UI(1, OVL_UI_ATTR_CTRL), 0xff << ALPHA_SHIFT | XRGB | ALPHA_MIX | LAYER_EN);
	dewr(OVL_UI(1, OVL_UI_TOP_LADD), PADDR(fb));
	dewr(OVL_UI(1, OVL_UI_PITCH), WIDTH*4);
	dewr(OVL_UI(1, OVL_UI_MBSIZE), ((HEIGHT-1) << 16) | (WIDTH-1));
	dewr(OVL_UI(1, OVL_UI_SIZE), ((HEIGHT-1) << 16) | (WIDTH-1));
	dewr(OVL_UI(1, OVL_UI_COORD), 0);


}


/* blending coefficients */
#define BLEND1 1 /* 1 */
#define BLEND1MA 0x3 /* 1-A */

static void
blenderinit(void)
{
	dewr(BLD(BLD_SIZE), ((HEIGHT-1) << 16) | (WIDTH-1));
	dewr(MIXER(0, GLB_SIZE), ((HEIGHT-1) << 16) | (WIDTH-1));

	dewr(BLD(BLD_CH_ISIZE), ((HEIGHT-1) << 16) | (WIDTH-1));
	dewr(BLD(BLD_FILL_COLOUR), 0xff000000);
	dewr(BLD(BLD_CH_OFFSET), 0);
	dewr(BLD(BLD_BK_COLOUR), 0xff000000);
	/* coefficient of 1 for both src and dst */
	dewr(BLD(BLD_CTL), BLEND1MA <<24 | BLEND1<<16 | BLEND1MA <<8 | BLEND1);

	dewr(BLD(BLD_CH_RTCTL), 1);

	/* pipe0 fill colour enable, pipe0 enable */
	dewr(BLD(BLD_FILLCOLOUR_CTL), 1<<8 | 1);
}
static void
enablemixer(void)
{
	dewr(MIXER(0, GLB_DBUFFER), 1);
}

void
deinit(void)
{
	dengineinit();
	delay(160);
	overlayinit();
	blenderinit();
	enablemixer();
	backlightinit();
	backlight(90);
}


void
blankscreen(int blank)
{
}

void
displaylink(void)
{
	deinit();
}