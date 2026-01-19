#include "u.h"
#include "../port/lib.h"

#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "pio.h"
#include "lcd.h"
#include "ccu.h"

#define DEBUG if(0)

enum {
	TCON_GCTL = 0x00,
	TCON_GINT0 = 0x04,
	TCON_GINT1 = 0x08,
 
	TCON1_CTL = 0x90,
		TCON1_CTL_ENABLE = 1<<31,
	TCON1_BASIC0 = 0x94,
	TCON1_BASIC1 = 0x98,
	TCON1_BASIC2 = 0x9c,
	TCON1_IO_TRI = 0xf4,
	TCON_SAFE_PERIOD = 0x1f0,
		SAFE_PERIOD_NUM = 3000<<16,
		SAFE_PERIOD_MODE = 3<<0,
};

static u32int
tconrd(int offset)
{
	return *IO(u32int, (SYSCTL+TCON1 + offset));
}

static void
tconwr(int offset, u32int val)
{
	*IO(u32int, (SYSCTL+TCON1 + offset)) = val;
}

static void
tcon1init(int width, int height)
{
	setclkrate(PLL_VIDEO0_CTRL_REG, 297*Mhz);
	clkenable(PLL_VIDEO0_CTRL_REG);
	clkenable(TCON1_CLK_REG);
	clkenable(HDMI_CLK_REG);
	clkenable(HDMI_SLOW_CLK_REG);

	if(closethegate("DE") == -1) /* will re-open later */
		panic("Could not close DE gate");
	if(openthegate("TCON1") == -1)
		panic("Could not open TCON1");

	if(openthegate("HDMI") == -1)
		panic("Could not open HDMI gate");

	tconwr(TCON_GCTL, tconrd(TCON_GCTL) & ~(1<<31));
	tconwr(TCON_GINT0, 0);
	tconwr(TCON_GINT1, 0);

	tconwr(TCON1_IO_TRI, 0xffffffff);
	tconwr(TCON1_CTL, TCON1_CTL_ENABLE | 0x1e<<4 /* start delay */); 

	tconwr(TCON1_BASIC0, ((width-1)<<16)|(height-1));
	tconwr(TCON1_BASIC1, ((width-1)<<16)|(height-1));
	tconwr(TCON1_BASIC2, ((width-1)<<16)|(height-1));
	tconwr(TCON_SAFE_PERIOD, SAFE_PERIOD_NUM|SAFE_PERIOD_MODE);
	tconwr(TCON1_IO_TRI, 0x7<<29);
	tconwr(TCON_GCTL, tconrd(TCON_GCTL) | (1<<31));
}


static void
pmicsetup(void)
{
	/* LCD reset pin? Not sure why we need this for
       HDMI, but otherwise we freeze on boot. */
	piocfg("PD23", PioOutput);
	pioset("PD23", 0);
	delay(15);

	setpmicvolt("DLDO1", 3300);
	setpmicstate("DLDO1", 1);
	setpmicvolt("DLDO2", 1800);
	setpmicstate("DLDO2", 1);
}

void
hdmiinit(int width, int height)
{
	tcon1init(width, height);
	pmicsetup();
}
