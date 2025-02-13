/*
 * Allwinner A64 Display Engine controller
 * 
 * Hacked together by Dave MacFarlane
 * Based off of https://lupyuen.github.io/articles/dsi#appendix-sequence-of-steps-for-pinephone-display-driver as a reference
 */
#define DEBUG(x) { iprint(x); }
#include "u.h"
#include "../port/lib.h"

#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#define R_PWM_CTRL_REG 0x0
#define	R_PWM_CH0_PERIOD 0x04

#define PIO_CFG_MASK(n) ~(7<<n)

#define PIO_PH_CFG01 0x100
#define PIO_PD_CFG02 0x74
#define	PIO_PH_DATA 0x10c
#define PIO_PD_DATA 0x7c

#define	RPIO_CFG10	0x04	/* Port L Configure Register 1 */
#define PLL_VIDEO00_CTRL_REG 0x10
#define TCON0_CLK_REG 0x118
#define BUS_CLK_GATING_REG1 0x64
static u32int
piord(int offset)
{
	coherence();
	return *IO(u32int, (PIO + offset));
}
static void
piowr(int offset, u32int val)
{
	// iprint("piowr: %ux = %ux\n", PIO + offset, val);
	*IO(u32int, (PIO + offset)) = val;
	coherence();
}

static void* demmio;
static u32int
derd(int offset)
{
	coherence();
	return *IO(u32int, (PIO + offset));
}
static void
dewr(int offset, u32int val)
{
	iprint("de: %ux = %ux\n", DE+ offset, val);
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
	// iprint("rpio: %ux = %ux\n", R_PIO + offset, val);
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
pwmwr(int offset, u32int val)
{
	// iprint("pwm: %ux = %ux\n", PWM + offset, val);
	*IO(u32int, (PWM + offset)) = val;
	coherence();
}
static void
rpwmwr(int offset, u32int val)
{
	// iprint("rpwm: %ux = %ux\n", R_PWM + offset, val);
	*IO(u32int, (R_PWM + offset)) = val;
	coherence();
}

static void
ccuwr(int offset, u32int val)
{
	// iprint("ccwr: %ux = %ux\n", CCUBASE + offset, val);
	*IO(u32int, (CCUBASE + offset)) = val;
	coherence();
}
static u32int
ccurd(int offset)
{
	coherence();
	return *IO(u32int, (CCUBASE + offset));
}

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
//	iprint("DPHY: %ux = %ux\n",DPHY + offset, val);
	*IO(u32int, (DPHY + offset)) = val;
	coherence();
}

static void
phywr(int offset, u32int val)
{
//	iprint("%ux = %ux\n", PHYSDEV + offset, val);
	*IO(u32int, (PHYSDEV + offset)) = val;
}

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
	// disable r_pwm_ctrl_reg (pg 194)
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) & ~(1<<6));
	
	// configure r_pwm_ch0_period (pg 195)
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*brightness/100)&0xffff));
	// enable r_pwm_ctrl_reg
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) | (1<<6) | (1<<4) | 0xf);
	/* configure PH10 for PIO output */
	piowr(PIO_PH_CFG01, piord(PIO_PH_CFG01) & PIO_CFG_MASK(8) | (1<<8));
	// on
	piowr(PIO_PH_DATA, piord(PIO_PH_DATA) | (1<<10));
}

#define PLL_MIPI_CTRL_REG 0x40
#define BUS_SOFT_RST_REG1 0x2c4
#define TCON_GCTL_REG 0x00
#define TCON_GINT0_REG 0x04
#define TCON_GINT1_REG 0x08
#define TCON0_IO_TRI_REG 0x8c
#define TCON1_IO_TRI_REG 0xf4
#define TCON0_DCLK_REG 0x44

static void
tcon0init(void)
{
	// configure video00
	ccuwr(PLL_VIDEO00_CTRL_REG, (1<<31)|(1<<24)|(0x62<<8)|7);
	coherence();
	// enable ldo1 and ldo2
	// ccuwr(PLL_MIPI_CTRL_REG, ccurd(PLL_MIPI_CTRL_REG) | (1<<23) | (1<<22));
	ccuwr(PLL_MIPI_CTRL_REG, (1<<23) | (1<<22));
	coherence();
	delay(100);
	// configure mipi pll
	ccuwr(PLL_MIPI_CTRL_REG, (1<<31)|(1<<23)|(1<<22)|(7<<8)|(1<<4)|10);
	coherence();

	// set tcon0 src clk to mipi pll
	ccuwr(TCON0_CLK_REG, (1<<31) & ~(7<<24));
	coherence();
	// enable tcon0 clk
	ccuwr(BUS_CLK_GATING_REG1, (1<<3));
	coherence();
	// deassert tcon0 reset
	ccuwr(BUS_SOFT_RST_REG1, (1<<3));
	coherence();
	// disable tcon0 and interrupts
	tconwr(TCON_GCTL_REG, tconrd(TCON_GCTL_REG) & ~(1<<31));
	tconwr(TCON_GINT0_REG, 0);
	tconwr(TCON_GINT1_REG, 0);

	// enable tristate output (XXX: According to the A64 User Manual this is disabling it?)
	tconwr(TCON0_IO_TRI_REG, 0xffffffff);
	tconwr(TCON1_IO_TRI_REG, 0xffffffff);

	// set dclk to mipi pll
	tconwr(TCON0_DCLK_REG, tconrd(TCON0_DCLK_REG) & ~(0xf<<28 | 0x7f) | (8<<28)|6);
#define TCON0_CTL_REG 0x40
#define TCON0_BASIC_REG 0x48
	tconwr(TCON0_CTL_REG, (1<<31)|(1<<24));
	tconwr(TCON0_BASIC_REG, (719<<16)|1439); /* panel size */
#define TCON0_ECC_FIFO 0xf8
	tconwr(TCON0_ECC_FIFO, 8);
#define TCON0_CPU_IF_REG 0x60
	tconwr(TCON0_CPU_IF_REG, (1<<28)|(1<<16)|(1<<2)|1);

	// set cpu panel to trigger
#define TCON0_CPU_TRI0_REG 0x160
#define TCON0_CPU_TRI1_REG 0x164
#define TCON0_CPU_TRI2_REG 0x168
	tconwr(TCON0_CPU_TRI0_REG, (47<<16)|719);
	tconwr(TCON0_CPU_TRI1_REG, 1439);
	tconwr(TCON0_CPU_TRI2_REG, (7106<<16)|10);

	// set safe period
#define TCON_SAFE_PERIOD_REG 0x1f0
	tconwr(TCON_SAFE_PERIOD_REG, (3000<<16)|3);

	// enable output triggers (XXX: This is actually enabling it) 
	tconwr(TCON0_IO_TRI_REG, 0x7<<29);

	// enable tcon0
	tconwr(TCON_GCTL_REG, tconrd(TCON_GCTL_REG) | (1<<31));
	

}

static void
dsiinit(void)
{
#define BUS_CLK_GATING_REG0 0x60
#define BUS_SOFT_RST_REG0 0x2c0
	// enable mipi dsi bus
	ccuwr(BUS_CLK_GATING_REG0, ccurd(BUS_CLK_GATING_REG0) | (1<<1));
	ccuwr(BUS_SOFT_RST_REG0, ccurd(BUS_SOFT_RST_REG0) |(1<<1));

	// enable dsi block
#define DSI_CTL_REG 0x00
	// dsiwr(DSI_CTL_REG, dsird(DSI_CTL_REG) | 1);
	dsiwr(DSI_CTL_REG, 1);
#define DSI_BASIC_CTL0_REG 0x10
//	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | (1<<17)|(1<<16)); // crc + ecc
	dsiwr(DSI_BASIC_CTL0_REG, (1<<17)|(1<<16)); // crc + ecc
#define DSI_START_TRANS_REG 0x60
	dsiwr(DSI_START_TRANS_REG, 10);
#define DSI_TRANS_ZERO_REG 0x78
	dsiwr(DSI_TRANS_ZERO_REG, 0);

	// set instructions
	// XXX: Figure out what these mean.
#define DSI_INST_FUNC_REG(n) (0x20 + (n*4))
	dsiwr(DSI_INST_FUNC_REG(0), 0x1f);
	dsiwr(DSI_INST_FUNC_REG(1), 0x10000001);
	dsiwr(DSI_INST_FUNC_REG(2), 0x20000010);
	dsiwr(DSI_INST_FUNC_REG(3), 0x2000000f);
	dsiwr(DSI_INST_FUNC_REG(4), 0x30100001);
	dsiwr(DSI_INST_FUNC_REG(5), 0x40000010);
	dsiwr(DSI_INST_FUNC_REG(6), 0xf);
	dsiwr(DSI_INST_FUNC_REG(7), 0x5000001f);

	// configure jump instructions (undocumented)
#define DSI_INST_JUMP_CFG_REG(n) (0x04c + (n*0x04))
	dsiwr(DSI_INST_JUMP_CFG_REG(0), 0x560001);
#define DSI_DEBUG_DATA_REG 0x2f8
	dsiwr(DSI_DEBUG_DATA_REG, 0xff);

	// set video delay
#define DSI_BASIC_CTL1_REG 0x14
	dsiwr(DSI_BASIC_CTL1_REG, dsird(DSI_BASIC_CTL1_REG) & ~(0x1fff << 4) | 1468<<4 | 1<<2 | 1<<1 | 1);

	// set burst (undocumented)
#define DSI_TCON0_DRQ_REG 0x7c
	dsiwr(DSI_TCON0_DRQ_REG, 0x10000007);

	// set loop instruction (undocumented)
#define DSI_INST_LOOP_SEL_REG 0x40
	dsiwr(DSI_INST_LOOP_SEL_REG, 0x30000002);
#define DSI_INST_LOOP_NUM_REG(n) (0x044 + (n*0x10))
	dsiwr(DSI_INST_LOOP_NUM_REG(0), 0x310031);
	dsiwr(DSI_INST_LOOP_NUM_REG(1), 0x310031);
	// set pixel format
#define DSI_PIXEL_PH_REG 0x90
	dsiwr(DSI_PIXEL_PH_REG, (19<<24) | 2160<<8 | 0x3e);
#define DSI_PIXEL_PF0_REG 0x98
	dsiwr(DSI_PIXEL_PF0_REG, 0xffff);
#define DSI_PIXEL_PF1_REG 0x9c
	dsiwr(DSI_PIXEL_PF1_REG, 0xffffffff);
#define DSI_PIXEL_CTL0_REG 0x80
	dsiwr(DSI_PIXEL_CTL0_REG, dsird(DSI_PIXEL_CTL0_REG) & ~(1<<4 | 0xf) | (1<<16)|8); // RGB888

	// set sync timings
#define DSI_BASIC_CTL_REG 0x0c
	dsiwr(DSI_BASIC_CTL_REG, 0);
#define DSI_SYNC_HSS_REG 0xb0
#define DSI_SYNC_HSE_REG 0xb4
	dsiwr(DSI_SYNC_HSS_REG, 0x12<<24 | 0x21);
	dsiwr(DSI_SYNC_HSE_REG, 1<<24 | 0x31);
#define DSI_SYNC_VSS_REG 0xb8
#define DSI_SYNC_VSE_REG 0xbc
	dsiwr(DSI_SYNC_VSS_REG, 7<<24 | 1);
	dsiwr(DSI_SYNC_VSE_REG, 0x14<<24 | 0x11);

	// set basic size (undocumented)
#define DSI_BASIC_SIZE0_REG 0x18
#define DSI_BASIC_SIZE1_REG 0x1c
	dsiwr(DSI_BASIC_SIZE0_REG, 17<<16|10);
	dsiwr(DSI_BASIC_SIZE1_REG, 1485<<16| 1440);

	// set horizontal blanking
#define DSI_BLK_HSA0_REG 0xc0
#define DSI_BLK_HSA1_REG 0xc4
#define DSI_BLK_HBP0_REG 0xc8
#define DSI_BLK_HBP1_REG 0xcc
#define DSI_BLK_HFP0_REG 0xd0
#define DSI_BLK_HFP1_REG 0xd4
#define DSI_BLK_HBLK0_REG 0xe0
#define DSI_BLK_HBLK1_REG 0xe4
	dsiwr(DSI_BLK_HSA0_REG, 0x9004a19);
	dsiwr(DSI_BLK_HSA1_REG, 0x50b4<<16);
	dsiwr(DSI_BLK_HBP0_REG, 0x35005419);
	dsiwr(DSI_BLK_HBP1_REG, 0x757a << 16);
	dsiwr(DSI_BLK_HFP0_REG, 0x9004a19);
	dsiwr(DSI_BLK_HFP1_REG, 0x50b4<<16);
	dsiwr(DSI_BLK_HBLK0_REG, 0xc091a19);
	dsiwr(DSI_BLK_HBLK1_REG, 0x72bd <<16);

	// set vertical blanking
#define DSI_BLK_VBLK0_REG 0xe8
#define DSI_BLK_VBLK1_REG 0xec
	dsiwr(DSI_BLK_VBLK0_REG, 0x1a000019);
	dsiwr(DSI_BLK_VBLK1_REG, 0xffff<<16);
}

static void
dphyinit(void)
{
#define MIPI_DSI_CLK_REG 0x168
	// XXX: Move this to the ccu setclkspeed function. Set the clock to 150Mhz
	ccuwr(MIPI_DSI_CLK_REG, 1<<15|2<<8|3);

	// Power on DPHY TX (undocumented)
#define DPHY_TX_CTL_REG 0x04
	dphywr(DPHY_TX_CTL_REG, 0x10000000);
#define DPHY_TX_TIME0_REG 0x10
	dphywr(DPHY_TX_TIME0_REG, 0xa06000e);
#define DPHY_TX_TIME1_REG 0x14
	dphywr(DPHY_TX_TIME1_REG, 0xa033207);
#define DPHY_TX_TIME2_REG 0x18
	dphywr(DPHY_TX_TIME2_REG, 0x1e);
#define DPHY_TX_TIME3_REG 0x1c
	dphywr(DPHY_TX_TIME3_REG, 0);
#define DPHY_TX_TIME4_REG 0x20
	dphywr(DPHY_TX_TIME4_REG, 0x303);

	// Enable DPHY (undocumented)
#define DPHY_GCTL_REG 0x00
#define DPHY_ANA0_REG 0x4c
#define DPHY_ANA1_REG 0x50
#define DPHY_ANA2_REG 0x54
#define DPHY_ANA3_REG 0x58
#define DPHY_ANA4_REG 0x5c

	dphywr(DPHY_GCTL_REG, 0x31);
	dphywr(DPHY_ANA0_REG, 0x9f007f00);
	dphywr(DPHY_ANA1_REG, 0x17000000);
	dphywr(DPHY_ANA4_REG, 0x1f01555);
	dphywr(DPHY_ANA2_REG, 0x2);
	delay(5);

	// Enable LDOR, LDOC, LDOD (undocumented)
	dphywr(DPHY_ANA3_REG, 0x3040000);// enable ldor, ldoc, ldod
	delay(1);
	dphywr(DPHY_ANA3_REG, dphyrd(DPHY_ANA3_REG) & ~(0xf8000000) | 0xf8000000); // enable vtcc, vttd
	delay(1);
	dphywr(DPHY_ANA3_REG,dphyrd(DPHY_ANA3_REG) & ~(0x4000000) |  0x4000000); // enable div
	delay(1);
	dphywr(DPHY_ANA2_REG, dphyrd(DPHY_ANA2_REG) | dphyrd(DPHY_ANA2_REG) | 0x10); //enable ck_cpu
	delay(1);
	dphywr(DPHY_ANA1_REG, dphyrd(DPHY_ANA1_REG) | 0x80000000); // vtt mode
	dphywr(DPHY_ANA2_REG, dphyrd(DPHY_ANA2_REG) | 0xf000000); // enable ps2 cpu
	
}
static void
lcdreset(void)
{
	DEBUG("LCD Reset");
	// configure PD23 for output
	piowr(PIO_PD_CFG02, piord(PIO_PD_CFG02) & PIO_CFG_MASK(28) | (1<<28));

	// set PD23 to high
	piowr(PIO_PD_DATA, piord(PIO_PD_DATA) | (1<<23));
	delay(15);
}
#define MIPI_DSI_DCS_SHORT_WRITE 0x05
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM 0x15
#define MIPI_DSI_DCS_LONG_WRITE 0x39


// XXX: document where this comes from
static void setecc(uchar header[4])
{
  u32int di_wc_word;
  uchar d[24];
  uchar ecc[8];
  int i;


  di_wc_word = header[0] | (header[1] << 8) | (header[2] << 16);

  for (i = 0; i < 24; i++)
    {
      d[i] = di_wc_word & 1;
      di_wc_word >>= 1;
    }
  ecc[7] = 0;
  ecc[6] = 0;
  ecc[5] = d[10] ^ d[11] ^ d[12] ^ d[13] ^ d[14] ^ d[15] ^ d[16] ^ d[17] ^
           d[18] ^ d[19] ^ d[21] ^ d[22] ^ d[23];
  ecc[4] = d[4]  ^ d[5]  ^ d[6]  ^ d[7]  ^ d[8]  ^ d[9]  ^ d[16] ^ d[17] ^
           d[18] ^ d[19] ^ d[20] ^ d[22] ^ d[23];
  ecc[3] = d[1]  ^ d[2]  ^ d[3]  ^ d[7]  ^ d[8]  ^ d[9]  ^ d[13] ^ d[14] ^
           d[15] ^ d[19] ^ d[20] ^ d[21] ^ d[23];
  ecc[2] = d[0]  ^ d[2]  ^ d[3]  ^ d[5]  ^ d[6]  ^ d[9]  ^ d[11] ^ d[12] ^
           d[15] ^ d[18] ^ d[20] ^ d[21] ^ d[22];
  ecc[1] = d[0]  ^ d[1]  ^ d[3]  ^ d[4]  ^ d[6]  ^ d[8]  ^ d[10] ^ d[12] ^
           d[14] ^ d[17] ^ d[20] ^ d[21] ^ d[22] ^ d[23];
  ecc[0] = d[0]  ^ d[1]  ^ d[2]  ^ d[4]  ^ d[5]  ^ d[7]  ^ d[10] ^ d[11] ^
           d[13] ^ d[16] ^ d[20] ^ d[21] ^ d[22] ^ d[23];

  header[3] = ecc[0] | (ecc[1] << 1) | (ecc[2] << 2) | (ecc[3] << 3) |
         (ecc[4] << 4) | (ecc[5] << 5) | (ecc[6] << 6) | (ecc[7] << 7);
}

static int mipidcsshortpkt(uchar *buf, int n, uchar *dst){
	assert(n == 1 || n == 2);
	uchar header[4] = {
		n == 1 ? MIPI_DSI_DCS_SHORT_WRITE : MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		buf[0],
		n == 2 ? buf[1] : 0,
	};
	setecc(header);
	dst[0] = header[0];
	dst[1] = header[1];
	dst[2] = header[2];
	dst[3] = header[3];
	return 4;
}

static u16int crc16ccitt_tab[256] =
{
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};
static u16int crc16ccitt(uchar *buf, int n) {
	/* CRC polynomial x^16+x^12+x^5+1 */
	u16int v = 0xffff;
	int i;
	for(i = 0; i < n;i++){
		v = (v >> 8) ^ crc16ccitt_tab[(v ^ buf[i]) & 0xff];		
	}
	return v;
}
static int mipidcslongpkt(uchar *buf, int n, uchar *dst){
	uchar header[4] = {
		MIPI_DSI_DCS_LONG_WRITE,
		n & 0xff,
		n >> 8
	};
	uchar footer[2];

	u16int crc = crc16ccitt(buf, n);
	setecc(header);
	footer[0] = crc & 0xff;
	footer[1] = crc >> 8;

	// XXX: Find a better way to do this or add memcpy to the kernel
	dst[0] = header[0];
	dst[1] = header[1];
	dst[2] = header[2];
	dst[3] = header[3];
	for(int i = 0; i < n; i++){
		dst[4+i] = buf[i];
	}
	dst[4+n] = footer[0];
	dst[4+n+1] = footer[1];
	/*
	memcpy(dst, header, 4);
	memcpy(dst+4, buf, n);
	memcpy(dst+4+n, footer, 2);
	*/
	return n+6;
}
static void mipidcs(uchar* buf, int n){
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

	// DSI low power mode
#define DSI_CMD_CTRL_REG 0x200
#define DSI_CMD_TX_REGISTER 0x300
	dsiwr(DSI_CMD_CTRL_REG, (1<<26)|(1<<25)|(1<<9));

	// Write the packet
	for(i = 0; i < pktlen; i += 4){
		v = pkt[i] 
			| ((i + 1 < pktlen) ?  pkt[i+1] : 0) << 8
			| ((i + 2 < pktlen) ?  pkt[i+2] : 0) << 16
			| ((i + 3 < pktlen) ?  pkt[i+3] : 0) << 24;
		assert(i <= 0xfc);
		dsiwr(DSI_CMD_TX_REGISTER + i, v);
	}
	// set the packet length
	dsiwr(DSI_CMD_CTRL_REG, dsird(DSI_CMD_CTRL_REG) & ~(0xff) | (pktlen-1));

	// begin transmission
#define DSI_INST_JUMP_SEL_REG 0x48
	dsiwr(DSI_INST_JUMP_SEL_REG, 
		4<<(4*0)
		| 15<<(4*4)
	);
	// toggle dsi to start transmission
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) & ~(1));
	// re-enable dsi to send packet
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | 1);

	// wait for transmission
	int timeout = 1000;
	while(timeout-- && (dsird(DSI_BASIC_CTL0_REG) & 1)){
		delay(50);
	}
	if(timeout <= 0)
		panic("dcs command timeout");

}
static uchar setextc[] = {
	0xb9, /* setextc, pg 131 */
	0xf1,
	0x12,
	0x83,
};
static uchar setmipi[] = {
	0xba, /* setmipi, pg144 */
	0x33,
	0x81,
	0x05,
	0xf9,
	0x0e,
	0x0e,
	0x20, /* undocumented */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x44,
	0x25,
	0x00,
	0x91,
	0x0a,
	0x00,
	0x00,
	0x02,
	0x4F,
	0x11,
	0x00,
	0x00,
	0x37,	
};

static uchar setpowerext[] = {
	0xb8, /* setpower_ext, pg142 */
	0x25,
	0x22,
	0x20,
	0x03,
	
};
static uchar setrgbif[] = {
	0xb3, /* setrgbif, pg134 */
	0x10,
	0x10,
	0x05,
	0x05,
	0x03,
	0xff,
	0x00,
	0x00,
	0x00,
	0x00,
};
static uchar setscr[] = {
	0xc0, /* setscr, pg147 */
	0x73,
	0x73,
	0x50,
	0x50,
	0x0,
	0xc0,
	0x08,
	0x70,
	0x0,
};

static uchar setvdc[] = {
	0xbc, /* setvdc, pg 146 */
	0x4e,
};

static uchar setpanel[] = {
	0xcc, /* setpanel, pg154 */
	0xb
};
static uchar setcyc[] = {
	0xb4, /* setcyc, pg135 */
	0x80
};

static uchar setdisp[] = {
	0xb2, /* setdisp, pg132 */
	0xf0,
	0x12,
	0xf0,
};

static uchar seteq[] = {
	0xe3, /* seteq, pg159 */
	0x0,
	0x0,
	0x0b,
	0x0b,
	0x10,
	0x10,
	0x0,
	0x0,
	0x0,
	0x0,
	0xff,
	0x00,
	0xc0,
	0x10,
};

static uchar undoccmd11[] = {
	0xc6, /* undocumented */
	0x01,
	0x00,
	0xff,
	0xff,
	0x00,
};

static uchar setpower[] = {
	0xc1, /* setpower, pg149 */
	0x74,
	0x0,
	0x32,
	0x32,
	0x77,
	0xf1,
	0xff,
	0xff,
	0xcc,
	0xcc,
	0x77,
	0x77,
};

static uchar setbgp[] = {
	0xb5, /* setbgp, pg136*/
	0x07,
	0x07,
};

static uchar setvcom[] ={
	0xb6, /* setvcom, pg137 */
	0x2c,
	0x2c,
};

static uchar undoccmd15[] = {
	0xbf,
	0x02,
	0x11,
	0x00
};

static uchar setgip1[] = {
	0xe9, /* set gip, pg163 */
	0x82,
	0x10,
	0x06,
	0x05,
	0xa2,
	0x0a,
	0xa5,
	0x12,
	0x31,
	0x23,
	0x37,
	0x83,
	0x04,
	0xbc,
	0x27,
	0x38,
	0x0c,
	0x0,
	0x03,
	0x0, 0x0, 0x0,
	0x0c,
	0x0,
	0x03,
	0x0, 0x0, 0x0,
	0x75,
	0x75,
	0x31,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x13,
	0x88,
	0x64,
	0x64,
	0x20,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x2,
	0x88,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static uchar setgip2[] = {
0xEA, /* setgip2, pg170 */
0x02,
0x21,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x02,
0x46,
0x02,
0x88,
0x88,
0x88,
0x88,
0x88,
0x88,
0x64,
0x88,
0x13,
0x57,
0x13,
0x88,
0x88,
0x88,
0x88,
0x88,
0x88,
0x75,
0x88,
0x23,
0x14,
0x00,
0x00,
0x02,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x03,
0x0A,
0xA5,
0x00,
0x00,
0x00,
0x0,
};

static uchar setgamma[] = {
0xE0, /* setgamma, pg158 */
0x00,
0x09,
0x0D,
0x23,
0x27,
0x3C,
0x41,
0x35,
0x07,
0x0D,
0x0E,
0x12,
0x13,
0x10,
0x12,
0x12,
0x18,
0x00,
0x09,
0x0D,
0x23,
0x27,
0x3C,
0x41,
0x35,
0x07,
0x0D,
0x0E,
0x12,
0x13,
0x10,
0x12,
0x12,
0x18,
};

static uchar slpout[] = {
	0x11, /* exit sleep mode, pg 89 */
};

static uchar dispon[] = {
	0x29, /* display on, pg 97 */
};
static void
lcdinit(void) {

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
	// configure PD23 for output, set to low. (Pin is active low)
	piowr(PIO_PD_CFG02, piord(PIO_PD_CFG02) & PIO_CFG_MASK(28) | (1<<28));
	piowr(PIO_PD_DATA, piord(PIO_PD_DATA) & ~(1<<23));
	delay(15);
	setpmicvolt("DLDO1", 3300); // camera / usb hsic / i2c sensors
	setpmicstate("DLDO1", 1);
	setpmicvolt("DLDO2", 1800);
	setpmicstate("DLDO2", 1);
	// XXX: GPIO0LDO for capacitive touch panel
	pwrwr(0x91, pwrrd(0x91) & ~(0x1f) | 26); // XXX: Move this to axp803.c. Enable GPIO0LDO for capacitive touch panel.
	pwrwr(0x90, pwrrd(0x90) & ~(0x7) | 3); // XXX: Low noise LDO on
	pwrwr(0x90, 3); // XXX: Low noise LDO on

	delay(15); // wait for power on
}

static void
mipihscinit(void)
{
	DEBUG("MIPI DSI HSC\n");
	dsiwr(DSI_INST_JUMP_SEL_REG, 0xf02);
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | 1);
	delay(1);
	iprint("mipi hsc init 1\n");
	coherence();
	while(dsird(DSI_BASIC_CTL0_REG) & 1){
		delay(50);
	}
	dsiwr(DSI_INST_FUNC_REG(0), dsird(DSI_INST_FUNC_REG(0)) & ~(1<<4));
	delay(1);
	coherence();
	dsiwr(DSI_INST_JUMP_SEL_REG, 0x63f07006);
	coherence();
	iprint("mipi hsc init 2\n");
	dsiwr(DSI_BASIC_CTL0_REG, dsird(DSI_BASIC_CTL0_REG) | 1);
	coherence();
	/*
	print("Waiting for init..");
	while(dsird(DSI_BASIC_CTL0_REG) & 1){
		delay(50);
	}
	*/
	delay(10);
}


static void
dengineinit(void)
{
	DEBUG("sram to DMA\n");
	phywr(0x4, 0); // set sram to dma
	coherence();
#define PLL_DE_CTRL_REG 0x48
	// FIXME: move to ccu. setclkspeed(297Mhz)
	DEBUG("Set DE clock\n");
	ccuwr(PLL_DE_CTRL_REG, (1<<31)|(1<<24)|(23<<8)|1);
	DEBUG("Waiting for DE clock to be stable");
	coherence();
	while((ccurd(PLL_DE_CTRL_REG) & (1<<28)) == 0){
		delay(1);
	}
#define DE_CLK_REG 0x104
	ccuwr(DE_CLK_REG, ccurd(DE_CLK_REG) & ~(1<<31 | 7<<24) | ((1<<31)|(1<<24)));
	iprint("DE_CLK_REG: %ux\n", ccurd(DE_CLK_REG)); delay(100);
	ccuwr(BUS_SOFT_RST_REG1, ccurd(BUS_SOFT_RST_REG1) |(1<<12));
	ccuwr(BUS_CLK_GATING_REG1, ccurd(BUS_CLK_GATING_REG1) |(1<<12));
	delay(100);
#define SCLK_GATE 0x0
	DEBUG("sclk\n"); delay(1000);
	coherence();
	dewr(SCLK_GATE, derd(SCLK_GATE) | 1);
#define AHB_RESET 0x8
	DEBUG("ahbreset\n"); delay(1000);
	dewr(AHB_RESET, derd(AHB_RESET) | 1);
#define HCLK_GATE 0x4
	DEBUG("hclk_gate\n");
	dewr(HCLK_GATE, derd(HCLK_GATE) | 1);

#define DE2TCON_MUX 0x10
	DEBUG("de2tcon\n");
	dewr(DE2TCON_MUX, derd(DE2TCON_MUX) & ~(1));

#define MIXER0 (0x100000)
	DEBUG("mixer0 to 0x5fff\n");
	for(int i = 0; i < 0x6000; i+= 4){
		dewr(MIXER0 + i, 0);
	}
#define VIDEO_SCALER_CH0(n) (MIXER0 + 0x20000 + n)
#define UI_SCALER_CH0(n) (MIXER0 + 0x40000 + n)
#define UI_SCALER_CH1(n) (MIXER0 + 0x50000 + n)
	DEBUG("video scaler\n");
	dewr(VIDEO_SCALER_CH0(0), 0);
	dewr(MIXER0 + 0x30000, 0); //??
dewr(VIDEO_SCALER_CH0(0), 0);
	DEBUG("ui scaler\n");
	dewr(UI_SCALER_CH0(0), 0);
dewr(UI_SCALER_CH0(0),0);
	DEBUG("ui scaler1\n");
	dewr(UI_SCALER_CH1(0), 0);
dewr(UI_SCALER_CH1(0), 0);
	DEBUG("fce 0xa0000\n");
	dewr(MIXER0 + 0xa0000, derd(MIXER0 + 0xa0000) & ~1);
	DEBUG("bws a2000\n");
dewr(MIXER0 + 0xa2000, 0);
	DEBUG("lti a4000\n");
	dewr(MIXER0 + 0xa4000, 0);

	DEBUG("peaking a6000\n");
	dewr(MIXER0 + 0xa6000, 0);
	DEBUG("ase a8000\n");
	dewr(MIXER0 + 0xa8000, 0);
	DEBUG("fcc aa000\n");
	dewr(MIXER0 + 0xaa000, 0);
#define DRCBASE 0x11b0000
	DEBUG("DRC\n");
//	u32int* drcbase = vmap(DRCBASE, 1024);
	coherence();
//	*(drcbase + 0) = *(drcbase + 0) & ~1;
//*(drcbase + 0) = 0;
	dewr(MIXER0 + 0xb0000, 0);
	delay(1000);
	DEBUG("enable mixer0\n");
	dewr(MIXER0, derd(MIXER0) | 1);
}

u32int *fb;
static void
displaysomething(void) {
	iprint("Display something\n");
	//fb = fbmemalloc(720*1440*4);
	fb = malloc(720*1440*4);
	iprint("Alloced\n");
	for(int i = 0;i < 720*1440; i++) {
		// random pattern
		fb[i] = 0xffff00ff | ((i%255)<<8);
	}
#define BLD(x) (MIXER0 + 0x1000 + x)
	//BLK_BK_COLOR
	iprint("blk_bk_color\n");
	dewr(BLD(0x88), 0xffff0000); // black bg
	// BLD_PREMUL_CTL
	iprint("blk_premul_ctl\n");
	dewr(BLD(0x84), 0);
	// disable ui overlay, channel 1-3
#define OVL_UI(i, reg) (MIXER0 + (0x2000 + 0x1000*i) + reg)
#define OVL_UI_ATTR_CTRL 0x00
	for(int i = 0; i < 4; i++){
		// disable ui overlay, channel 0-3
		iprint("disable ch%d overlay\n", i);
		dewr(OVL_UI(i, OVL_UI_ATTR_CTRL), 0);
		// disable ui scaler, channel 1-3
		iprint("disable ch%d scaler\n", i);
		dewr(MIXER0 + 0x40000 + (0x10000*i), 0);
	}

	iprint("set overlay\n");
	// set overlay for channel 0. opaque, xrgb, alphamode 2, enable
	int i = 1;
	// dewr(OVL_UI(i, OVL_UI_ATTR_CTRL), 0xff000405 | (1<<15));
	dewr(OVL_UI(i, OVL_UI_ATTR_CTRL), (0xff<<24)|(1<<4)|(1<<0));
#define OVL_UI_TOP_LADD 0x10
	iprint("set top\n");
	dewr(OVL_UI(i, OVL_UI_TOP_LADD), PADDR(fb));
// dewr(OVL_UI(i, OVL_UI_TOP_LADD), 0); // just use fill colour, testing.
#define OVL_UI_PITCH 0xc
iprint("set pitch\n");
	dewr(OVL_UI(i, OVL_UI_PITCH), 720*4);
#define OVL_UI_MBSIZE 0x4
iprint("set mbsize\n");
	dewr(OVL_UI(i, OVL_UI_MBSIZE), ((1440-1) << 16) + (720-1));
#define OVL_UI_SIZE 0x88
iprint("set uisize\n");
	dewr(OVL_UI(i, OVL_UI_SIZE), ((1440-1) << 16) + (720-1));

#define OVL_UI_COORD 0x8
iprint("setcoord\n");
	dewr(OVL_UI(i, OVL_UI_COORD), 0);
#define OVL_UI_FILL_COLOR 0x18
	dewr(OVL_UI(i, OVL_UI_FILL_COLOR), 0xffff00ff);
	// set blender output
iprint("BLD_SIZE\n");
	dewr(BLD(0x8c), ((1440-1) << 16) + (720-1));
iprint("GLB_SIZE\n");
	dewr(MIXER0 + 0xc, ((1440-1) << 16) + (720-1));

	// set blender input
iprint("BLD_CH_ISIZE\n");
	dewr(BLD(0x8 + i*0x10), ((1440-1) << 16) + (720-1));
iprint("bld pipe fcolor\n");
	dewr(BLD(0x4 + i*0x10), 0xffff0000); // fill color, red
iprint("bld offset\n");
	dewr(BLD(0xc + i*0x10), 0); // offset, none
iprint("bld pipe mode\n");
	dewr(BLD(0x88), 0xffff00ff); // BLD_BK_COLOR, red
	dewr(BLD(0x90), 0x03010301); // use coefficient of 1 for the blender source and dst

	// disable scaler (already done) by init
	dewr(MIXER0 + 0x40000, 0);

	coherence();
	dewr(BLD(0x80), 1);
iprint("8\n");
	coherence();
	dewr(BLD(0x0), 0x101);
iprint("dbuffering!!\n");
	coherence();
	dewr(MIXER0 + 0x8, 1); // enable double buffering
	iprint("Can you see anything?\n");
}

void
deinit(void)
{
//	void* tmp = ucalloc(720*1440*4);
	iprint("Backlight");
	backlightinit();
	backlight(90);

	DEBUG("deinit\n");

	DEBUG("tcon0 init\n");
	tcon0init();
	DEBUG("pmic setup\n");
	pmicsetup(); // pmic was already initialized for rsb, just need to configure.
	DEBUG("dsi block\n");
	dsiinit();	// enable mipi dsi block
	DEBUG("dphyinit\n");
	dphyinit(); // enable mipi physical layer
	DEBUG("lcd reset\n");
	lcdreset(); // reset lcd panel and wait 15 ms
	DEBUG("lcd init\n");
	lcdinit(); // initialize lcd controller
	DEBUG("hsc + hsd\n");
	mipihscinit();// start mipi hsc
	DEBUG("de engine init\n");
	dengineinit(); // initialize display engine
	delay(160); // wait 160ms
	DEBUG("rendering\n");
	displaysomething();
}
