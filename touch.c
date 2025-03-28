#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "pio.h"

typedef struct Ctlr Ctlr;

static struct Ctlr {
	Rendez;
};

static Ctlr ctlr;
/* FIXME: Move these to axp803 */
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

void
touchwait(void)
{
	sleep(&ctlr, return0, nil);	
}

static void
touchinterrupt(Ureg*, void*)
{
	wakeup(&ctlr);
	/* Ack the interrupt */
	*IO(u32int, PIO + 0x254) = 1<<4;
}
static void
touchinit(void)
{
	// XXX: GPIO0LDO for capacitive touch panel
	pwrwr(0x91, pwrrd(0x91) & ~(0x1f) | 26); // XXX: Move this to axp803.c. Enable GPIO0LDO for capacitive touch panel.
	pwrwr(0x90, pwrrd(0x90) & ~(0x7) | 3); // XXX: Low noise LDO on
	pwrwr(0x90, 3); // XXX: Low noise LDO on

}

static void
touchintcfg(void)
{
	/* 53 is PH interrupt. Need to figure out how to derive it. Interrupt is from PH */

	// TODO: Set priority edge
	piocfg("PH4", PioInterrupt); /* 6 = interrupt */
	// TODO: Drive low?
	pioeintcfg("PH4", PioEIntNeg);
	intrenable(53, touchinterrupt, &ctlr, BUSUNKNOWN, "CTP");


}
void
touchlink(void)
{
	touchinit();
	touchintcfg();
}
