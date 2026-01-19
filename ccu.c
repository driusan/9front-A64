#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ccu.h"

#define CLK_ENABLED 1<<31

/* XXX: These may also be shared with other non-SDMMC clocks */
#define SDMMC_CLK_SRC_MASK	0x3
#define	SDMMC_CLK_SRC_SHIFT 	24
#define	SDMMC_CLK_N_MASK	0x3
#define SDMMC_CLK_N_SHIFT	16
#define SDMMC_CLK_M_MASK	0xf
static u32int
ccurd(int offset)
{
	return *IO(u32int, (CCUBASE + offset));
}


static void
ccuwr(int offset, u32int val)
{
	*IO(u32int, (CCUBASE + offset)) = val;
}


static Gate*
findgate(char *name)
{
	Gate *g;

	for(g = gates; g->name != nil; g++){
		if(cistrcmp(name, g->name) == 0)
			return g;
	}

	return nil;
}


static Reset*
findreset(char *name)
{
	Reset *r;

	for(r = resets; r->name != nil; r++){
		if(cistrcmp(name, r->name) == 0)
			return r;
	}

	return nil;
}


char*
listgates(int i)
{
	Gate *g = gates;
	u32int v;
	char p[32];
	int m;

	g += i;

	if(g->name == nil)
		return nil;
	
	v = ccurd(BUS_CLK_GATING_REG0 + g->bank);
	m = (v & g->mask ? 1 : 0);

	print("%s-\n", g->name);
	print("%d-\n", m);

	snprint(p, sizeof(p), "%s %d", g->name, m);
	
	return p;
}


char*
getgatename(int i)
{
	Gate *g = gates;

	g += i;

	return g->name;
}


int
getgatestate(int i)
{
	Gate *g = gates;
	int r, s;

	g += i;

	r = ccurd(BUS_CLK_GATING_REG0 + g->bank);
	s = (r & g->mask ? 1 : 0);

	return s;
}


char*
getresetname(int i)
{
	Gate *g = gates;

	g += i;

	return g->name;
}


int
getresetstate(int i)
{
	Gate *g = gates;
	int r, s;

	g += i;

	r = ccurd(BUS_SOFT_RST_REG0 + g->bank);
	s = (r & g->mask ? 1 : 0);

	return s;
}

int
closethegate(char *name)
{
	u32int buf;
	Reset *r;
	Gate *g;

	r = findreset(name);
	g = findgate(name);

	if(g == nil || r == nil)
		return -1;

//	iprint("closethegate:%s = %s & %s\n", name, g->name, r->name);
//	iprint("closethegate:%s = %08uX & %08uX\n", name, (BUS_CLK_GATING_REG0 + g->bank), (BUS_SOFT_RST_REG0 + r->bank));
//	debuggates();

	/* open the gate */
	buf = ccurd(BUS_CLK_GATING_REG0 + g->bank);
	buf &= ~g->mask;
	ccuwr(BUS_CLK_GATING_REG0 + g->bank, buf);

	buf = ccurd(BUS_SOFT_RST_REG0 + r->bank);
	buf &= ~r->mask;
	ccuwr(BUS_SOFT_RST_REG0 + r->bank, buf);

	return 0;
}
int
openthegate(char *name)
{
	u32int buf;
	Reset *r;
	Gate *g;

	r = findreset(name);
	g = findgate(name);

//	iprint("opengate:%s = %s & %s\n", name, g->name, r->name);
//	iprint("opengate:%s = %08uX & %08uX\n", name, (BUS_CLK_GATING_REG0 + g->bank), (BUS_SOFT_RST_REG0 + r->bank));
//	debuggates();
	if(g == nil || r == nil)
		return -1;

	/* hit the reset */
	buf = ccurd(BUS_SOFT_RST_REG0 + r->bank);
	buf |= r->mask;
	ccuwr(BUS_SOFT_RST_REG0 + r->bank, buf);

	/* open the gate */
	buf = ccurd(BUS_CLK_GATING_REG0 + g->bank);
	buf |= g->mask;
	ccuwr(BUS_CLK_GATING_REG0 + g->bank, buf);

	return 1;
}



void
debuggates(void)
{
	print("%ub\n", ccurd(BUS_CLK_GATING_REG0));
	print("%ub\n", ccurd(BUS_CLK_GATING_REG1));
	print("%ub\n", ccurd(BUS_CLK_GATING_REG2));
	print("%ub\n", ccurd(BUS_CLK_GATING_REG3));
	print("%ub\n", ccurd(BUS_CLK_GATING_REG4));
}

u32int
getcpuclk_n(void)
{
	u32int n;

	n = ccurd(PLL_CPUX_CTRL_REG);
	n = (n & 0x1F00) >> 8;
	return n;
}

u32int
getcpuclk_k(void)
{
	u32int k;

	k = ccurd(PLL_CPUX_CTRL_REG);
	k = (k & 0x30) >> 4;
	return k;
}

u32int
getcpuclk_m(void)
{
	u32int m;

	m = ccurd(PLL_CPUX_CTRL_REG);
	m = (m & 0x3);
	return m;
}

u32int
getcpuclk_p(void)
{
	u32int p;

	p = ccurd(PLL_CPUX_CTRL_REG);
	p = (p & 0x30000) >> 16;
	return p;
}


static Nkmp*
findcpuclk(uint findrate)
{
	Nkmp *nkmp;

	for(nkmp = cpu_nkmp; nkmp->rate != 0; nkmp++){
		if(nkmp->rate == findrate)
			return nkmp;
	}

	return nil;
}

static Nkmp*
findcpuclk_n(u32int findn)
{
	Nkmp *nkmp;

	for(nkmp = cpu_nkmp; nkmp->rate != 0; nkmp++){
		if(nkmp->n == findn)
			return nkmp;
	}

	return nil;
}



int
setcpuclk_n(u32int setn)
{
	Nkmp *nkmp;
	u32int reg;

	nkmp = findcpuclk_n(setn);

	if(nkmp == nil){
		iprint("setcpuclk_n found nil\n");
		return -1;
	}

	reg = ccurd(PLL_CPUX_CTRL_REG);
	reg &= 0xFFFC0000;
	reg |= (nkmp->n << 8) & 0x1F00;
	reg |= (nkmp->k << 4) & 0x30;
	reg |= (nkmp->m << 0) & 0x3;
	reg |= (nkmp->p << 16) & 0x30000;

	ccuwr(PLL_CPUX_CTRL_REG, reg);

	return 1;
}


int
setcpuclk(uint setrate)
{
	Nkmp *nkmp;
	u32int reg;

	nkmp = findcpuclk(setrate);

	if(nkmp == nil){
		iprint("setcpuclk found nil\n");
		return -1;
	}

	reg = ccurd(PLL_CPUX_CTRL_REG);
	reg &= 0xFFFC0000;
	reg |= (nkmp->n << 8) & 0x1F00;
	reg |= (nkmp->k << 4) & 0x30;
	reg |= (nkmp->m << 0) & 0x3;
	reg |= (nkmp->p << 16) & 0x30000;

	ccuwr(PLL_CPUX_CTRL_REG, reg);

	return 1;
}

/* need to do a general clock setting function */
void
turnonths(void)
{
//	u32int	buf;

	ccuwr(THS_CLK_REG, 0x80000000);

//	buf = ccurd(BUS_SOFT_RST_REG3);
//	buf |= 1<<8;
//	ccuwr(BUS_SOFT_RST_REG3, buf);

//	buf = ccurd(BUS_CLK_GATING_REG2);
//	buf |= 1<<8;
//	ccuwr(BUS_CLK_GATING_REG2, buf);

	if(openthegate("THS") != 1)
		iprint("THS open FAIL\n");
}

void
clkenable(int clkid)
{
	u32int reg;
	switch(clkid){
	case SDMMC0_CLK_REG:
	case SDMMC1_CLK_REG:
	case SDMMC2_CLK_REG:
	case PLL_DE_CTRL_REG:
	case DE_CLK_REG:
	case TCON0_CLK_REG:
	case TCON1_CLK_REG:
	case PLL_VIDEO0_CTRL_REG:
	case PLL_VIDEO1_CTRL_REG:
	case PLL_MIPI_CTRL_REG: 
	case HDMI_SLOW_CLK_REG:
		reg = ccurd(clkid);
		reg |= CLK_ENABLED;	
		ccuwr(clkid, reg);
		return;
	case HDMI_CLK_REG:
		reg = ccurd(clkid);
		reg |= CLK_ENABLED;
		reg &= ~(3<<24); /* VIDEO0 src */
		ccuwr(clkid, reg);
		return;
	case MIPI_DSI_CLK_REG:
		reg = ccurd(clkid);
		reg |= 1<<15;
		ccuwr(clkid, reg);
		break;
	case USBPHY_CFG_REG:
		/* Most registers control 1 clock, but this controls many. We enable them all. */
		reg = ccurd(clkid);
		reg |= 3<<16; /* otg-ohci + ohci0 */
		/* reg |= 1<<11;  12m hsic 
		reg |= 1<<10;  hsic */
		reg |= 1<<9; /* usbphy1 */
		reg |= 1<<8; /* usbphy0 */
		ccuwr(clkid, reg);
		/* FIXME: Check if this can be done in the same ccuwr after USB is working. */
		/* reg |= 1<<2; deassert reset, usb hsic */
		reg |= 1<<1; /* deassert reset, usbphy1 */
		reg |= 1<<0; /* deassert reset, usbphy0 */
		ccuwr(clkid, reg);
		return;
	default:
		panic("clkenable: Unhandled clock: %x", clkid);
	}
}

void
clkdisable(int clkid)
{
	u32int reg;
	switch(clkid){
	case SDMMC0_CLK_REG:
	case SDMMC1_CLK_REG:
	case SDMMC2_CLK_REG:
	case PLL_VIDEO0_CTRL_REG:
	case PLL_MIPI_CTRL_REG: 
		reg = ccurd(clkid);
		reg &= ~(CLK_ENABLED);
		ccuwr(clkid, reg);
		return;	
	default:
		panic("clkdisable: Unhandled clock: %x", clkid);
	}
}
void
setclkrate(int clkid, ulong hz)
{
	u32int	reg, n, m, refspeed;
	int i, j;
	Nkmp *nm;
	switch(clkid){
	case SDMMC0_CLK_REG:
	case SDMMC1_CLK_REG:
	case SDMMC2_CLK_REG:
		if(hz <= SYSCLOCK){
			refspeed = SYSCLOCK;

		} else {
			/* should be 1.2Ghz */
			refspeed = getclkrate(PLL_PERIPH0_CTRL_REG)*2;
		}

		m = refspeed / hz;
		n = 0;
		while(m > 16){
			n++;
			// if(m & 1 != 0)
			//	iprint("ccu: warning. losing precision.\n");
			m >>= 1;
		}
		if (n > 3){
			// panic?
			iprint("ccu: n too high. Using max n and m.\n");
			n = 3;
			m = 16;
		}
		reg = ccurd(clkid);
		reg &=  ~(
			(SDMMC_CLK_N_MASK << SDMMC_CLK_N_SHIFT)
			| SDMMC_CLK_M_MASK |
			(SDMMC_CLK_SRC_MASK << SDMMC_CLK_SRC_SHIFT)
		);
		reg |= 
			((refspeed == SYSCLOCK ? 0 : 1)<< SDMMC_CLK_SRC_SHIFT)
			| (n << SDMMC_CLK_N_SHIFT)
			| ((m-1) & SDMMC_CLK_M_MASK);
		ccuwr(clkid, reg);
		break;
	case PLL_VIDEO0_CTRL_REG:
	case PLL_VIDEO1_CTRL_REG:
		/* clock is 24Mhz * n / m */
		n = 0;
		m = 0;
		for(nm = pll_video_nkmp; nm->rate != 0; nm++){
			if(nm->rate == hz){
				n = nm->n;
				m = nm->m;
				break;
			}
		}
		if(n == 0 && m == 0)
			panic("Unhandled speed for PLL_VIDEO0");
		/* don't touch enabled, frac_clk_out, pll_mode_sel, or pll_sdm_en */
		reg = ccurd(clkid) & (1<<31 | 1<<24|1<<20);
		reg |= n <<8;
		reg |= m;
		ccuwr(clkid, reg);
		break;
	case MIPI_DSI_CLK_REG:
		reg = ccurd(clkid);
		switch((reg & 0x3<<8)>>8){
		case 0:
			refspeed = getclkrate(PLL_VIDEO0_CTRL_REG);
			break;
		case 2:
			refspeed = getclkrate(PLL_PERIPH0_CTRL_REG);
			break;
		default:
			panic("Unknown reference clock for MIPI_DSI_CLK_REG");
		}
		m = refspeed / hz;

		assert(m <= 16);

		reg &= ~(0xf);
		reg |= m;
		ccuwr(MIPI_DSI_CLK_REG, reg);
		break;
	case PLL_MIPI_CTRL_REG: 
		/* clk is video0 * n * k / m. Must be 500Mhz~1.4Ghz. k >=2, m/n<=3.
			We can't use a precalculated table because it depends on PLL_VIDEO0
		 */
		
		refspeed = getclkrate(PLL_VIDEO0_CTRL_REG);
		reg = ccurd(clkid) & ~(0xf | 0x3<<4 | 0xf<<8); /* clear n, m, k */
		reg |= 1<<22 | 1<<23; /* set LDO1/2_en while we're here. */

		/* brute force it */
		for(i = 0; i <16;i++) { /* n */
			for(j = 0; j< 16; j++) { /* m */
				/* check m / n <= 3 condition */
				if((j / i) > 3)
					continue;

				if(refspeed * (i+1) * 2 / (j+1) == hz) {
					reg |= i <<8;
					reg |= j <<0;
					i = 16;
					j = 16;
				} else if(refspeed * (i+1) * 3 / (j+1) == hz) {
					reg |= i <<8;
					reg |= 1<<4;
					reg |= j << 0;
					i = 16;
					j = 16;
				} else if(refspeed * (i+1) * 4 / (j+1) == hz) {
					reg |= i <<8;
					reg |= 2 <<4;
					reg |= j << 0;
					i = 16;
					j = 16;
				}
			}
		}

		ccuwr(PLL_MIPI_CTRL_REG, reg);
		while(ccurd(PLL_MIPI_CTRL_REG) & (1<<28) == 0){
			delay(1);
		}
		break;
	case DE_CLK_REG:
		/* FIXME: Can probably do better, but these are always used together for now, so just set PLL_DE_CTRL_REG and use it as the reference with an m of 1 */
		reg = ccurd(clkid);
		setclkrate(PLL_DE_CTRL_REG, hz);
		reg = reg & ~(0x7<<24 | 0xf);
		reg = reg | (1<<24);
		ccuwr(clkid, reg);
		break;
	case PLL_DE_CTRL_REG:
		reg = ccurd(clkid);
		n = hz / (24*Mhz);
		m = 0;
		while(n > 0x7f) {
			n /= 2;
			m++;
		}
		reg |= 25; /* 297Mhz */
		reg |= 1; /* Integer mode */
		reg = reg & ~((0x7f<<8) | (0xf));
		reg |= (n-1)<<8;
		reg |= m;
		ccuwr(PLL_DE_CTRL_REG, reg);
		while(ccurd(PLL_DE_CTRL_REG) & (1<<28) == 0){
			delay(1);
		}
		break;
	default:
		panic("setclkrate: Unhandled clock: %x", clkid);
	}
}
ulong
getclkrate(int clkid)
{
	u32int	reg, n, k, ref, m;
	reg = ccurd(clkid);
	switch(clkid){
	case PLL_PERIPH0_CTRL_REG:
	case PLL_PERIPH1_CTRL_REG:
		if (!(reg & CLK_ENABLED))
			return 0;
		n = ((reg & 0x1F00) >> 8)+1;
		k = ((reg & 0x30) >> 4)+1;
		return SYSCLOCK*n*k/2;
	case SDMMC0_CLK_REG:
	case SDMMC1_CLK_REG:
	case SDMMC2_CLK_REG:
		if (!(reg & CLK_ENABLED))
			return 0;
		ref = (reg >> 24) & 0x3;
		if (ref == 0)
			ref = SYSCLOCK;
		else if (ref == 1)
			ref = getclkrate(PLL_PERIPH0_CTRL_REG)*2;
		else
			ref = getclkrate(PLL_PERIPH1_CTRL_REG)*2;
		n = (reg >> SDMMC_CLK_N_SHIFT) & 0x3;
		n = 1<<n;
		m = (reg & 0xf)+1;
		return ref / n / m;
	case AHB1_APB1_CFG_REG:
	case APB2_CFG_REG:
		ref = (reg >> 24) & 0x3;
		if (ref == 0)
			ref = 32*1000; /* LOSC = 32Khz? */
		else if (ref == 1)
			ref = SYSCLOCK;
		else
			ref = getclkrate(PLL_PERIPH0_CTRL_REG)*2;
		n = (reg >> 16) & 0x3;
		n = 1<<n;
		m = (reg & 0xf) + 1;		
		return ref / n / m;
	case PLL_DE_CTRL_REG:
		if (!(reg & CLK_ENABLED))
			return 0;
		n = (reg >> 8)&0x7f;
		m = reg & 0xf;
		return 24*Mhz*(n+1) / (m+1);
	case DE_CLK_REG:
		if (!(reg & CLK_ENABLED))
			return 0;
		ref = (reg >> 24) & 0x3;
		if (ref == 0)
			ref = getclkrate(PLL_PERIPH0_CTRL_REG)*2;
		else if (ref == 1)
			ref = getclkrate(PLL_DE_CTRL_REG);
		else
			return 0;
		m = (ref & 0xf) + 1;
		return ref / m;
	case PLL_VIDEO0_CTRL_REG:
		n = (reg>>8)& 0x7f;
		m = reg&0xf;
		return SYSCLOCK*(n+1) / (m+1);
	default:
		panic("getclkrate: Unhandled clock: %x", clkid);
	}
}
