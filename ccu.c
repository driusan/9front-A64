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
openthegate(char *name)
{
	u32int buf;
	Reset *r;
	Gate *g;

	r = findreset(name);
	g = findgate(name);

//	iprint("opengate:%s = %s & %s\n", name, g->name, r->name);
//	iprint("opengate:%s = %08uX & %08uX\n", name, (BUS_CLK_GATING_REG0 + g->bank), (BUS_SOFT_RST_REG0 + r->bank));

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
		reg = ccurd(clkid);
		reg |= CLK_ENABLED;	
		ccuwr(clkid, reg);
		return;
	default:
		panic("Unhandled clock");
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
		reg = ccurd(clkid);
		reg &= ~(CLK_ENABLED);
		ccuwr(clkid, reg);
		return;	
	default:
		panic("Unhandled clock");
	}
}
void
setclkrate(int clkid, ulong hz)
{
	u32int	reg, n, m, refspeed;
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
	default:
		panic("Unhandled clock");
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
	default:
		panic("Unhandled clock");
	}
}
