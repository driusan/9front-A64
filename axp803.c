/*
 *	The AXP803 is the PMIC (power manager)
 *	used for Allwinner A64 SoC platforms
 */


#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"

#include "axp803.h"

#define DEBUG if(0)

static enum {
	Asleep,
	Awake,
};
typedef struct Ctlr Ctlr;
static struct Ctlr{
	QLock;
	int state;
	int lastbrightness;
};

#define PWR_REG_MASK	0xFF

#define	PWR_SOURCE	0x00
#define	PWR_MODE	0x01
#define	PWR_REASON	0x02
#define	PWR_ID_NUM	0x03

rintcrd(u32int reg)
{
	return *IO(u32int, R_INTC + reg);
}

static void
rintcwr(u32int reg, u32int val)
{
	*IO(u32int, R_INTC + reg) = val;
}
static u8int
pwrrd(u8int reg)
{
	u32int buf;

	buf = rsb_read(PMICRTA, PMICADDR, reg, 1);
	return (u8int)(buf & PWR_REG_MASK);
}


static int
pwrwr(u8int reg, u8int val)
{
	return rsb_write(PMICRTA, PMICADDR, reg, val, 1);
}


u8int
pmic_id(void)
{
	u8int buf, msb, lsb;

/*
 *	Reg 03, IC type no.
 *	bits 4 & 5 are uncertain
 *	AXP803 should then return b01xx0001
 *	shift bits 6 & 7 over to get b010001
 */

	buf = pwrrd(PWR_ID_NUM);

	msb = (0xC0 & buf) >> 2;
	lsb = 0x0F & buf;

	return (msb | lsb);
}


int
pmic_acin(void)
{
	u8int buf;

	buf = pwrrd(PWR_SOURCE);

	return (buf & 0x80);
}


int
pmic_vbat(void)
{
	u8int buf;

	buf = pwrrd(PWR_SOURCE);

	return (buf & 0x08);
}


static Pmicregs*
findpmicrail(char *name)
{
	Pmicregs *pr;

	for(pr = pmicregs; pr->name != nil; pr++){
		if(cistrcmp(name, pr->name) == 0)
			return pr;
	}

	return nil;
}


int
getpmicstate(int rail)
{
	u8int buf;
	Pmicregs *pr = pmicregs;

	pr += rail;
	if(pr->onoffreg == 0){
		return -1;
	}
	buf = pwrrd(pr->onoffreg);

	buf &= 1<<pr->onoffbit;

	return (int)(buf? 1 : 0);
}


int
setpmicstate(char *name, int state)
{
	u8int buf;
	Pmicregs *pr;// = pmicregs;

	pr = findpmicrail(name);

	if(pr == nil || pr->onoffreg == 0)
		return -1;

	buf = pwrrd(pr->onoffreg);

	DEBUG iprint("setstate: %s = %d _ %08ub_", name, state, buf);

	if((buf & 1<<pr->onoffbit) == (state<<pr->onoffbit))
		return 1;

	if(state)
		buf |= 1<<pr->onoffbit;
	else
		buf ^= 1<<pr->onoffbit;

	DEBUG iprint("%08ub\n", buf);

	if(pwrwr(pr->onoffreg, buf))
		return 1;


	iprint("setpmicstate: pwrwr failed\n");
	return 0;
}


/*
 *	Some of the rails have split voltage steps.
 *	Above a certain value, each step will be 
 *	twice as many mV.
 *	ex. 10mV steps till 1200mV, and 20mv steps above
 */
int
getpmicvolt(int rail)
{
	u8int buf;
	int steps, mv;
	Pmicregs *pr = pmicregs;

	pr += rail;

	buf = pwrrd(pr->voltreg);
	buf &= pr->voltmask;

	steps = (int)buf;

	mv = (steps * pr->voltstep1) + pr->voltbase;

	if(mv > pr->voltsplit){
		int mvl, mvh;
		mvl = (pr->voltsplit - pr->voltbase) / pr->voltstep1;
		mvh = steps - mvl;
		
		mv = (mvl * pr->voltstep1) + (mvh * pr->voltstep2);
	}

	return mv;
}


int
setpmicvolt(char *name, int val)
{
	u8int buf;
	Pmicregs *pr;// = pmicregs;

	pr = findpmicrail(name);

	if(pr == nil)
		return -1;

	if(val > pr->voltmax)
		val = pr->voltmax;

	if(val < pr->voltbase)
		val = pr->voltbase;

	if(val <= pr->voltsplit){
		buf = (u8int)((val - pr->voltbase) / pr->voltstep1);
		buf &= pr->voltmask;
	}
	if(val > pr->voltsplit){
		int mvl, mvh;
		mvl = (pr->voltsplit - pr->voltbase) / pr->voltstep1;
		mvh = (val - pr->voltsplit) / pr->voltstep2;
		buf = (u8int)(mvl + mvh);
		buf &= pr->voltmask;
	}

	DEBUG iprint("setvolt: %ud\n", buf);

	if(pwrwr(pr->voltreg, buf))
		return 1;


	iprint("setpmicvolt: pwrwr failed\n");
	return 0;
}


char*
getpmicname(int rail)
{
	Pmicregs *pr = pmicregs;

	pr += rail;

	return pr->name;
}

int
pmic_batterypresent(void)
{
	u8int reg = pwrrd(PWR_MODE);
	if ((reg & (1<<4)) == 0) {
		return -1; /* invalid flag is set */
	}
	return reg & (1<<5);
}

int
pmic_batterycharging(void)
{
	u8int reg = pwrrd(PWR_MODE);
	if ((reg & (1<<4)) == 0) {
		return -1; /* invalid flag is set for battery presence */
	}
	if ((reg & (1<<5)) == 0){
		return -1; /* no battery present */
	}
	return (reg & (1<<6)) != 0;
	
}


int
pmic_chargepct(void)
{
	u8int buf = pwrrd(0xb9);

	if(!(buf & (1<<7)))
		return -1;
	return buf & 0x7f;
}



int
axpgetmaxcharge(void)
{
	int val;
	u8int buf = pwrrd(0xe0);

//	if(!(buf & (1<<7)))
//		return -1;

	val = (buf & 0x7f)<<8;
	val |= pwrrd(0xe1);
	return (int )((float )val * 1.456);
}

int
axpgetcurrentcharge(void)
{
	int val;
	u8int buf = pwrrd(0xe2);

//	if(!(buf & (1<<7)))
//		return -1;

	val = (buf & 0x7f)<<8;
	val |= pwrrd(0xe3);
	return (int )((float )val * 1.456);
}


int
axpgetbatteryvoltage(void)
{
	int val = pwrrd(0xa1) & 0xf;
	val |= (u8int) pwrrd(0xe3) << 4;
	/* FIXME: This needs to be converted to a voltage. */
	return val;
}
int
axpgetwarning1(void)
{
	int val = pwrrd(0xe5) & 0xf0;
	val >>= 4;
	return val + 5;
}
int
axpgetwarning2(void)
{
	return pwrrd(0xe5) & 0xf;
}
extern int brightness;
static void togglestate(Ctlr *ctlr)
{
	switch(ctlr->state){
	case Asleep:
		setpmicstate("DLDO1", 1);
		setpmicstate("DLDO2", 1);
		DEBUG iprint("Setting brightness to %d\n", ctlr->lastbrightness);
		backlight(ctlr->lastbrightness);
		ctlr->state = Awake;
		break;
	case Awake:
		if(brightness > 0){
			DEBUG iprint("Saving last brightness %d\n", ctlr->lastbrightness);
			ctlr->lastbrightness = brightness;
		}
		backlight(0);


		setpmicstate("DLDO1", 0); /* video/sensors/usb hsic*/
		setpmicstate("DLDO2", 0); /* mipi */
		setpmicstate("DLDO3", 0); /* avdd-csi? camera? */
		// setpmicstate("DLDO4", 0); /* wifi */
		ctlr->state = Asleep;
		break;
	default:
		panic("Invalid state");
	}
}
static void
axp803interrupt(Ureg*, void* a)
{
	Ctlr *ctlr = a;
	u8int reg;

	reg = pwrrd(0x2);
	if(reg & 1<<7)
		DEBUG iprint("Power on key override shutdown\n");
	if(reg & 1<<5)
		DEBUG iprint("PMIC UVLR shutdown reason\n");
	if(reg & 1<<2)
		DEBUG iprint("Battery insertion startup reason\n");
	if(reg & 1<<1)
		DEBUG iprint("Charger insertion startup reason\n");
	if(reg & 1<<0)
		DEBUG iprint("Power on key startup reason\n");
	pwrwr(0x2, reg);

	reg = pwrrd(0x48);
	if(reg & 1<<7)
		iprint("ACIN over voltage\n");
	if(reg & 1<<6)
		iprint("ACIN from low to high\n");
	if(reg & 1<<5)
		iprint("ACIN from high to low\n");
	if(reg & 1<<4)
		iprint("VBUS over voltage\n");
	if(reg & 1<<3)
		iprint("VBUS from low to high\n");
	if(reg & 1<<2)
		iprint("VBUS from high to low\n");
	pwrwr(0x48, reg);

	reg = pwrrd(0x49);
	if(reg & 1<<7)
		DEBUG iprint("Battery append IRQ\n");
	if(reg & 1<<6)
		DEBUG iprint("Battery absent IRQ\n");
	if(reg & 1<<5)
		DEBUG iprint("Battery may be bad IRQ\n");
	if(reg & 1<<4)
		DEBUG iprint("Quit battery safe mode IRQ\n");
	if(reg & 1<<3)
		DEBUG iprint("Charger is charging IRQ\n");
	if(reg & 1<<2)
		DEBUG iprint("Battery charge done IRQ\n");
	pwrwr(0x49, reg);

	reg = pwrrd(0x4a);
	if(reg & 1<<7)
		iprint("CBTOIRQ\n");
	if(reg & 1<<6)
		DEBUG iprint("QCBTOIRQ\n");
	if(reg & 1<<5)
		DEBUG iprint("CBTUIRQ\n");
	if(reg & 1<<4)
		DEBUG iprint("QCBTUIRQ\n");
	if(reg & 1<<3)
		DEBUG iprint("WBTOIRQ\n");
	if(reg & 1<<2)
		DEBUG iprint("QWBTOIRQ\n");
	if(reg & 1<<1)
		DEBUG iprint("WBTUIRQ\n");
	if(reg & 1<<2)
		DEBUG iprint("QWBTUIRQ\n");
	pwrwr(0x4a, reg);

	reg = pwrrd(0x4b);
	if(reg & 1<<7)
		DEBUG iprint("OTIRQ\n");
	if(reg & 1<<2)
		DEBUG iprint("GPADC(GPIO0 ADC finished\n");
	if(reg & 1<<1)
		DEBUG iprint("Battery lower than warning 1\n");
	if(reg & 1<<0)
		DEBUG iprint("Battery lower than warning 2\n");
	pwrwr(0x4b, reg);

	reg = pwrrd(0x4c);
	if(reg & (1<<5)) 
		DEBUG iprint("POK negative edge");
	if(reg & (1<<6))
		DEBUG iprint("POK positive edge");
	if(reg & (1<<4)){
		DEBUG iprint("POK short time");
		togglestate(ctlr);
	}
	if(reg & (1<<3)){
		DEBUG iprint("POK long time");
		togglestate(ctlr);
		if(ctlr->state == Awake)
			backlight(100);

	}
	pwrwr(0x4c, reg);

	reg = pwrrd(0x4d);
	if (reg & 1<<1)
		DEBUG iprint("BC_USB_ChngEvnt\n");
	if(reg & 1<<0)
		DEBUG iprint("MV_ChngEvnt\n");
	pwrwr(0x4d, reg);

	/* ack the interupt in r_intc so it doesn't keep firing */
	rintcwr(0x10, 1);
}

static void
rintrinit(void)
{
	rintcwr(0xc, 0); /* NMI is low level interrupt */
	rintcwr(0x40, 1); /* enable */
}

static Ctlr ctlr;
void
axp803link(void)
{
	ctlr.state = Awake;
	ctlr.lastbrightness = brightness;

	rintrinit();
	intrenable(IRQpmic, axp803interrupt, &ctlr, BUSUNKNOWN, "axp803");
}
