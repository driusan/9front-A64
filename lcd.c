/*
 * Allwinner A64 Display Engine controller
 * 
 * Hacked together by Dave MacFarlane
 * Based off of https://lupyuen.github.io/articles/dsi#appendix-sequence-of-steps-for-pinephone-display-driver as a reference
 */

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

#define DSI_CTL_REG 0x00
#define DSI_BASIC_CTL0_REG 0x10
#define DSI_START_TRANS_REG 0x60
#define DSI_TRANS_ZERO_REG 0x78
#define DSI_INST_FUNC_REG(n) (0x20 + (n*4))
#define DSI_INST_JUMP_CFG_REG(n) (0x04c + (n*0x04))
#define DSI_DEBUG_DATA_REG 0x2f8
#define DSI_BASIC_CTL1_REG 0x14
#define DSI_TCON0_DRQ_REG 0x7c
#define DSI_INST_LOOP_SEL_REG 0x40
#define DSI_INST_LOOP_NUM_REG(n) (0x044 + (n*0x10))
#define DSI_PIXEL_PH_REG 0x90
#define DSI_PIXEL_PF0_REG 0x98
#define DSI_PIXEL_PF1_REG 0x9c
#define DSI_PIXEL_CTL0_REG 0x80
#define DSI_BASIC_CTL_REG 0x0c
#define DSI_SYNC_HSS_REG 0xb0
#define DSI_SYNC_HSE_REG 0xb4
#define DSI_SYNC_VSS_REG 0xb8
#define DSI_SYNC_VSE_REG 0xbc
#define DSI_BASIC_SIZE0_REG 0x18
#define DSI_BASIC_SIZE1_REG 0x1c
#define DSI_BLK_HSA0_REG 0xc0
#define DSI_BLK_HSA1_REG 0xc4
#define DSI_BLK_HBP0_REG 0xc8
#define DSI_BLK_HBP1_REG 0xcc
#define DSI_BLK_HFP0_REG 0xd0
#define DSI_BLK_HFP1_REG 0xd4
#define DSI_BLK_HBLK0_REG 0xe0
#define DSI_BLK_HBLK1_REG 0xe4
#define DSI_BLK_VBLK0_REG 0xe8
#define DSI_BLK_VBLK1_REG 0xec
#define DSI_CMD_CTRL_REG 0x200
#define DSI_CMD_TX_REGISTER 0x300

#define MIPI_DSI_DCS_SHORT_WRITE 0x05
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM 0x15
#define MIPI_DSI_DCS_LONG_WRITE 0x39
enum {
	TCON_GCTL = 0x00,
	TCON_GINT0 = 0x04,
	TCON_GINT1 = 0x08,
 
	TCON0_CTL = 0x40,
		TCON0_CTL_ENABLE = 1<<31,
		TCON0_8080_IF = 1<<24,
	TCON0_DCLK = 0x44,
		DCLK_EN_SHIFT = 28,
		DCLK_DIV_SHIFT = 0,
	TCON0_BASIC = 0x48,
	TCON0_CPU_IF = 0x60,
		CPU_IF_DSI = 1<<28,
		CPU_IF_FLUSH = 1<<16,
		CPU_IF_FIFO_DIS = 1<<2,
		CPU_IF_EN = 1<<0,
	TCON0_IO_TRI = 0x8c,
	TCON1_IO_TRI = 0xf4,
	TCON0_ECC_FIFO = 0xf8,
	TCON0_CPU_TRI0 = 0x160,
		CPU_TRI0_BLOCK_SPACE = 47<<16,
	TCON0_CPU_TRI1 = 0x164,
	TCON0_CPU_TRI2 = 0x168,
		CPU_TRI2_DELAY = 7106 << 16,
		CPU_TRI2_TRANS_START = 10 << 0,
	TCON_SAFE_PERIOD = 0x1f0,
		SAFE_PERIOD_NUM = 3000<<16,
		SAFE_PERIOD_MODE = 3<<0,
};

enum {
	DPHY_GCTL = 0x00,

	DPHY_TX_CTL = 0x04,
	DPHY_TX_TIME0 = 0x10,
	DPHY_TX_TIME1 = 0x14,
	DPHY_TX_TIME2 = 0x18,
	DPHY_TX_TIME3 = 0x1c,
	DPHY_TX_TIME4 = 0x20,

	DPHY_ANA0 = 0x4c,
	DPHY_ANA1 = 0x50,
	DPHY_ANA2 = 0x54,
	DPHY_ANA3 = 0x58,
	DPHY_ANA4 = 0x5c,
};

static u32int
tconrd(int offset)
{
	coherence();
	return *IO(u32int, (SYSCTL+TCON0 + offset));
}
static void
tconwr(int offset, u32int val)
{
	// iprint("tconwr: %ux = %ux\n", SYSCTL+TCON0 + offset, val);
	*IO(u32int, (SYSCTL+TCON0 + offset)) = val;
	coherence();
}
static int
pwrwr(u8int reg, u8int val)
{
	coherence();
	return rsb_write(PMICRTA, PMICADDR, reg, val, 1);
}
static int
pwrrd(u8int reg)
{
	coherence();
	return rsb_read(PMICRTA, PMICADDR, reg, 1);
}
static u32int
dsird(int offset)
{
	coherence();
	return *IO(u32int, (DSI + offset));
}

static void
dsiwr(int offset, u32int val)
{
	// iprint("DSI: %ux = %ux\n",DSI + offset, val);
	*IO(u32int, (DSI + offset)) = val;
	coherence();
}

static u32int
dphyrd(int offset)
{
	coherence();
	return *IO(u32int, (DPHY + offset));
}

static void
dphywr(int offset, u32int val)
{
	*IO(u32int, (DPHY + offset)) = val;
	coherence();
}

static void
tcon0init(int width, int height)
{
	setclkrate(PLL_VIDEO0_CTRL_REG, 297*Mhz);
	clkenable(PLL_VIDEO0_CTRL_REG);

	setclkrate(PLL_MIPI_CTRL_REG, 648*Mhz);
	clkenable(PLL_MIPI_CTRL_REG);

	clkenable(TCON0_CLK_REG);
	/* some of these are leftover by the bootloader and interfere with our initialization */
	if(closethegate("VE") == -1)
		panic("Could not close VE gate");
	if(closethegate("DE") == -1) /* will re-open later */
		panic("Could not close DE gate");

	if(openthegate("TCON0") == -1)
		panic("Could not open TCON0");

	tconwr(TCON_GCTL, tconrd(TCON_GCTL) & ~(1<<31));
	tconwr(TCON_GINT0, 0);
	tconwr(TCON_GINT1, 0);
	tconwr(TCON0_IO_TRI, 0xffffffff);
	tconwr(TCON1_IO_TRI, 0xffffffff);

	tconwr(TCON0_DCLK, 8<<DCLK_EN_SHIFT | 6<<DCLK_DIV_SHIFT);
	tconwr(TCON0_CTL, TCON0_CTL_ENABLE|TCON0_8080_IF);
	tconwr(TCON0_BASIC, ((width-1)<<16)|(height-1));
	/* undocumented register? */
	tconwr(TCON0_ECC_FIFO, 8);

	tconwr(TCON0_CPU_IF, CPU_IF_DSI|CPU_IF_FLUSH|CPU_IF_FIFO_DIS|CPU_IF_EN);
	tconwr(TCON0_CPU_TRI0, CPU_TRI0_BLOCK_SPACE|(width-1));
	tconwr(TCON0_CPU_TRI1, height-1);
	tconwr(TCON0_CPU_TRI2, CPU_TRI2_DELAY|CPU_TRI2_TRANS_START);

	tconwr(TCON_SAFE_PERIOD, SAFE_PERIOD_NUM|SAFE_PERIOD_MODE);

	/* invalid or undocumented value write? */ 
	tconwr(TCON0_IO_TRI, 0x7<<29);

	tconwr(TCON_GCTL, tconrd(TCON_GCTL) | (1<<31));
}

static void
dsiinit(void)
{
	if(openthegate("MIPIDSI") == -1)
		panic("Could not open MIPI DSI gate");

	dsiwr(DSI_CTL_REG, 1);

	dsiwr(DSI_BASIC_CTL0_REG, (1<<17)|(1<<16)); /* crc + ecc */
	dsiwr(DSI_START_TRANS_REG, 10);
	dsiwr(DSI_TRANS_ZERO_REG, 0);

	dsiwr(DSI_INST_FUNC_REG(0), 0x1f);
	dsiwr(DSI_INST_FUNC_REG(1), 0x10000001);
	dsiwr(DSI_INST_FUNC_REG(2), 0x20000010);
	dsiwr(DSI_INST_FUNC_REG(3), 0x2000000f);
	dsiwr(DSI_INST_FUNC_REG(4), 0x30100001);
	dsiwr(DSI_INST_FUNC_REG(5), 0x40000010);
	dsiwr(DSI_INST_FUNC_REG(6), 0xf);
	dsiwr(DSI_INST_FUNC_REG(7), 0x5000001f);

	dsiwr(DSI_INST_JUMP_CFG_REG(0), 0x560001);
	dsiwr(DSI_DEBUG_DATA_REG, 0xff);

	dsiwr(DSI_BASIC_CTL1_REG, dsird(DSI_BASIC_CTL1_REG) & ~(0x1fff << 4) | 1468<<4 | 1<<2 | 1<<1 | 1);

	dsiwr(DSI_TCON0_DRQ_REG, 0x10000007);

	dsiwr(DSI_INST_LOOP_SEL_REG, 0x30000002);

	dsiwr(DSI_INST_LOOP_NUM_REG(0), 0x310031);
	dsiwr(DSI_INST_LOOP_NUM_REG(1), 0x310031);

	dsiwr(DSI_PIXEL_PH_REG, (19<<24) | 2160<<8 | 0x3e);
	dsiwr(DSI_PIXEL_PF0_REG, 0xffff);
	dsiwr(DSI_PIXEL_PF1_REG, 0xffffffff);
	dsiwr(DSI_PIXEL_CTL0_REG, dsird(DSI_PIXEL_CTL0_REG) & ~(1<<4 | 0xf) | (1<<16)|8); /* RGB888 */

	dsiwr(DSI_BASIC_CTL_REG, 0);
	dsiwr(DSI_SYNC_HSS_REG, 0x12<<24 | 0x21);
	dsiwr(DSI_SYNC_HSE_REG, 1<<24 | 0x31);
	dsiwr(DSI_SYNC_VSS_REG, 7<<24 | 1);
	dsiwr(DSI_SYNC_VSE_REG, 0x14<<24 | 0x11);

	dsiwr(DSI_BASIC_SIZE0_REG, 17<<16|10);
	dsiwr(DSI_BASIC_SIZE1_REG, 1485<<16| 1440);

	dsiwr(DSI_BLK_HSA0_REG, 0x9004a19);
	dsiwr(DSI_BLK_HSA1_REG, 0x50b4<<16);
	dsiwr(DSI_BLK_HBP0_REG, 0x35005419);
	dsiwr(DSI_BLK_HBP1_REG, 0x757a << 16);
	dsiwr(DSI_BLK_HFP0_REG, 0x9004a19);
	dsiwr(DSI_BLK_HFP1_REG, 0x50b4<<16);
	dsiwr(DSI_BLK_HBLK0_REG, 0xc091a19);
	dsiwr(DSI_BLK_HBLK1_REG, 0x72bd <<16);

	dsiwr(DSI_BLK_VBLK0_REG, 0x1a000019);
	dsiwr(DSI_BLK_VBLK1_REG, 0xffff<<16);
}

static void
dphyinit(void)
{
	setclkrate(MIPI_DSI_CLK_REG, 150*Mhz);
	clkenable(MIPI_DSI_CLK_REG);

	dphywr(DPHY_TX_CTL, 0x10000000);
	dphywr(DPHY_TX_TIME0, 0xa06000e);
	dphywr(DPHY_TX_TIME1, 0xa033207);
	dphywr(DPHY_TX_TIME2, 0x1e);
	dphywr(DPHY_TX_TIME3, 0);
	dphywr(DPHY_TX_TIME4, 0x303);

	dphywr(DPHY_GCTL, 0x31);
	dphywr(DPHY_ANA0, 0x9f007f00);
	dphywr(DPHY_ANA1, 0x17000000);
	dphywr(DPHY_ANA4, 0x1f01555);
	dphywr(DPHY_ANA2, 0x2);
	delay(5);

	dphywr(DPHY_ANA3, 0x3040000);
	delay(1);
	dphywr(DPHY_ANA3, dphyrd(DPHY_ANA3) & ~(0xf8000000) | 0xf8000000);
	delay(1);
	dphywr(DPHY_ANA3,dphyrd(DPHY_ANA3) & ~(0x4000000) |  0x4000000);
	delay(1);
	dphywr(DPHY_ANA2, dphyrd(DPHY_ANA2) | dphyrd(DPHY_ANA2) | 0x10);
	delay(1);
	dphywr(DPHY_ANA1, dphyrd(DPHY_ANA1) | 0x80000000);
	dphywr(DPHY_ANA2, dphyrd(DPHY_ANA2) | 0xf000000);	
}

static void
lcdreset(void)
{
	piocfg("PD23", PioOutput);
	pioset("PD23", 1);
	delay(15);
}

// XXX: document where this comes from
static void
calcecc(uchar *dst)
{
	u32int di_wc_word;
	uchar d[24];
	uchar ecc[8];
	int i;

	di_wc_word = dst[0] | (dst[1] << 8) | (dst[2] << 16);

	for (i = 0; i < 24; i++) {
		d[i] = di_wc_word & 1;
		di_wc_word >>= 1;
	}

	ecc[7] = 0;
	ecc[6] = 0;
	ecc[5] = d[10] ^ d[11] ^ d[12] ^ d[13] ^ d[14] ^ d[15] ^ d[16] ^ d[17] ^ d[18] ^ d[19] ^ d[21] ^ d[22] ^ d[23];
	ecc[4] = d[4]  ^ d[5]  ^ d[6]  ^ d[7]  ^ d[8]  ^ d[9]  ^ d[16] ^ d[17] ^ d[18] ^ d[19] ^ d[20] ^ d[22] ^ d[23];
	ecc[3] = d[1]  ^ d[2]  ^ d[3]  ^ d[7]  ^ d[8]  ^ d[9]  ^ d[13] ^ d[14] ^ d[15] ^ d[19] ^ d[20] ^ d[21] ^ d[23];
	ecc[2] = d[0]  ^ d[2]  ^ d[3]  ^ d[5]  ^ d[6]  ^ d[9]  ^ d[11] ^ d[12] ^ d[15] ^ d[18] ^ d[20] ^ d[21] ^ d[22];
	ecc[1] = d[0]  ^ d[1]  ^ d[3]  ^ d[4]  ^ d[6]  ^ d[8]  ^ d[10] ^ d[12] ^ d[14] ^ d[17] ^ d[20] ^ d[21] ^ d[22] ^ d[23];
  ecc[0] = d[0]  ^ d[1]  ^ d[2]  ^ d[4]  ^ d[5]  ^ d[7]  ^ d[10] ^ d[11] ^ d[13] ^ d[16] ^ d[20] ^ d[21] ^ d[22] ^ d[23];

	dst[3] = ecc[0]
		| (ecc[1] << 1)
		| (ecc[2] << 2)
		| (ecc[3] << 3)
		| (ecc[4] << 4)
		| (ecc[5] << 5)
		| (ecc[6] << 6)
		| (ecc[7] << 7);
}

static int
mipidcsshortpkt(uchar *buf, int n, uchar *dst)
{
	assert(n == 1 || n == 2);
	dst[0] = (n == 1 ? MIPI_DSI_DCS_SHORT_WRITE : MIPI_DSI_DCS_SHORT_WRITE_PARAM);
	dst[1] = buf[0];
	dst[2] = (n == 2 ? buf[1] : 0);
	calcecc(dst);
	return 4;
}

static u16int
crc16ccitt(uchar *buf, int n)
{
	u16int v = 0xffff;
	int i;
	for(i = 0; i < n;i++){
		v = (v >> 8) ^ crc16ccitt_tab[(v ^ buf[i]) & 0xff];		
	}
	return v;
}

static int
mipidcslongpkt(uchar *buf, int n, uchar *dst)
{
	dst[0] = MIPI_DSI_DCS_LONG_WRITE;
	dst[1] = n & 0xff;
	dst[2] = n >> 8;
	uchar footer[2];

	u16int crc = crc16ccitt(buf, n);
	calcecc(dst);
	footer[0] = crc & 0xff;
	footer[1] = crc >> 8;

	for(int i = 0; i < n; i++)
		dst[4+i] = buf[i];

	dst[4+n] = footer[0];
	dst[4+n+1] = footer[1];
	return n+6;
}

static int
dsitx(void)
{
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) & ~(1));
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | 1);

	int timeout = 1000;
	while(timeout-- && (dsird(DSI_BASIC_CTL0_REG) & 1))
		delay(50);

	if(timeout <= 0)
		return -1;
	return 0;
}

static void
mipidcs(uchar* buf, int n)
{
	uchar pkt[128];
	int i;
	u32int v;
	u32int pktlen;

	memset(pkt, 0, 128);
	assert(n > 0);
	switch(n){
	case 1:
		pktlen = mipidcsshortpkt(buf, n, pkt);
		break;
	case 2:
		pktlen = mipidcsshortpkt(buf, n, pkt);
		break;
	default:
		pktlen = mipidcslongpkt(buf, n, pkt);
		break;
	}

	dsiwr(DSI_CMD_CTRL_REG, (1<<26)|(1<<25)|(1<<9));

	for(i = 0; i < pktlen; i += 4){
		v = pkt[i] 
			| ((i + 1 < pktlen) ?  pkt[i+1] : 0) << 8
			| ((i + 2 < pktlen) ?  pkt[i+2] : 0) << 16
			| ((i + 3 < pktlen) ?  pkt[i+3] : 0) << 24;
		assert(i <= 0xfc);
		dsiwr(DSI_CMD_TX_REGISTER + i, v);
	}

	dsiwr(DSI_CMD_CTRL_REG, dsird(DSI_CMD_CTRL_REG) & ~(0xff) | (pktlen-1));

#define DSI_INST_JUMP_SEL_REG 0x48
	dsiwr(DSI_INST_JUMP_SEL_REG, 
		4<<(4*0)
		| 15<<(4*4)
	);

	if(dsitx() == -1)
		panic("dcs command timeout");

}

static void
lcdpanelinit(void)
{
	mipidcs(setextc, sizeof(setextc));
	mipidcs(setmipi, sizeof(setmipi));
	mipidcs(setpowerext, sizeof(setpowerext));
	mipidcs(setrgbif, sizeof(setrgbif));
	mipidcs(setscr, sizeof(setscr));
	mipidcs(setvdc, sizeof(setvdc));
	mipidcs(setpanel, sizeof(setpanel));
	mipidcs(setcyc, sizeof(setcyc));
	mipidcs(setdisp, sizeof(setdisp));
	mipidcs(seteq, sizeof(seteq));
	// XXX: Test if removing this still works after everything
	// is done.
	mipidcs(undoccmd11, sizeof(undoccmd11));
	mipidcs(setpower, sizeof(setpower));
	mipidcs(setbgp, sizeof(setbgp));
	mipidcs(setvcom, sizeof(setvcom));
	mipidcs(undoccmd15, sizeof(undoccmd15));
	mipidcs(setgip1, sizeof(setgip1)); assert(sizeof(setgip1) == 64);
	mipidcs(setgip2, sizeof(setgip2));
	mipidcs(setgamma, sizeof(setgamma));
	mipidcs(slpout, sizeof(slpout));
	delay(120);
	mipidcs(dispon, sizeof(dispon));	
}

static void
pmicsetup(void)
{
	/* Pin is active low */
	piocfg("PD23", PioOutput);
	pioset("PD23", 0);
	delay(15);
	setpmicvolt("DLDO1", 3300);
	setpmicstate("DLDO1", 1);
	setpmicvolt("DLDO2", 1800);
	setpmicstate("DLDO2", 1);

	// XXX: GPIO0LDO for capacitive touch panel
	pwrwr(0x91, pwrrd(0x91) & ~(0x1f) | 26); // XXX: Move this to axp803.c. Enable GPIO0LDO for capacitive touch panel.
	pwrwr(0x90, pwrrd(0x90) & ~(0x7) | 3); // XXX: Low noise LDO on
	pwrwr(0x90, 3); // XXX: Low noise LDO on

	delay(15);
}

static void
mipihscinit(void)
{
	dsiwr(DSI_INST_JUMP_SEL_REG, 0xf02);

	if(dsitx() == -1)
		panic("Could not transmit DSI packet");
	dsiwr(DSI_INST_FUNC_REG(0), dsird(DSI_INST_FUNC_REG(0)) & ~(1<<4));
	delay(1);
	coherence();
	dsiwr(DSI_INST_JUMP_SEL_REG, 0x63f07006);
	coherence();
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | 1);
	delay(10);
}

void
lcdinit(void)
{
	DEBUG iprint("tcon0 init\n");
	tcon0init(720, 1440);
	DEBUG iprint("pmic setup\n");
	pmicsetup();
	DEBUG iprint("dsi block\n");
	dsiinit();
	DEBUG iprint("dphyinit\n");
	dphyinit();
	DEBUG iprint("lcd reset\n");
	lcdreset();
	DEBUG iprint("lcdinit\n");
	lcdpanelinit();
	DEBUG iprint("mipihscinit\n");
	mipihscinit();
	DEBUG iprint("done lcd init\n");
}

void
lcdlink(void)
{
	lcdinit();
}
