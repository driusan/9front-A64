/*
 *	Ethernet for the Quactel EG25-G LTE modem used by the Pinephone
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/netif.h"
#include "../port/etherif.h"

#define PIN_CFG_MASK 0x7
/* pio */
#define PB_CFG0_REG 0x24
#define PC_CFG0_REG 0x48
#define PH_CFG0_REG 0xfc
#define PH_CFG1_REG 0x100

#define PB3_CFG_SHIFT 12
#define PB2_CFG_SHIFT 8

#define PC4_CFG_SHIFT 16
#define PH7_CFG_SHIFT 28
#define PH8_CFG_SHIFT 0
#define PH9_CFG_SHIFT 4

#define PB_DATA 0x34
#define PC_DATA 0x58
#define PH_DATA 0x10c

/* rpio */
#define PL_CFG0_REG 0
#define PL6_CFG_SHIFT 24
#define PL7_CFG_SHIFT 28
#define PL_DATA 0x10

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
typedef struct Ctlr Ctlr;
struct Ctlr {
	Rendez;
};
static int
pnp(Ether *edev)
{
	static Ctlr ctlr[1];

	if(edev->ctlr != nil)
		return -1;
	return -1;
}

static void
pincfg(void)
{
	/* PL7 - Baseband power - output */
	rpiowr(PL_CFG0_REG, rpiord(PL_CFG0_REG) & ~(PIN_CFG_MASK << PL7_CFG_SHIFT) | (1 << PL7_CFG_SHIFT));

	/* PB3 - LTE modem power - output */
	piowr(PB_CFG0_REG, piord(PB_CFG0_REG) & ~(PIN_CFG_MASK << PB3_CFG_SHIFT) | (1 << PB3_CFG_SHIFT));
	/* PB2 - DTR? Output? What is this? */
	piowr(PB_CFG0_REG, piord(PB_CFG0_REG) & ~(PIN_CFG_MASK << PB2_CFG_SHIFT) | (1 << PB2_CFG_SHIFT));
	/* PC4 - modem reset - output */
	piowr(PC_CFG0_REG, piord(PC_CFG0_REG) & ~(PIN_CFG_MASK << PC4_CFG_SHIFT) | (1 << PC4_CFG_SHIFT));

	/* PH9 - modem status - input */
	piowr(PH_CFG1_REG, piord(PH_CFG1_REG) & ~(PIN_CFG_MASK << PH9_CFG_SHIFT) | (0 << PH9_CFG_SHIFT));
	/* PH8 - airplane mode - output */
	piowr(PH_CFG1_REG, piord(PH_CFG1_REG) & ~(PIN_CFG_MASK << PH8_CFG_SHIFT) | (1 << PH8_CFG_SHIFT));
	/* PH7 - sleep status - output */
	piowr(PH_CFG0_REG, piord(PH_CFG0_REG) & ~(PIN_CFG_MASK << PH7_CFG_SHIFT) | (1 << PH7_CFG_SHIFT));
	/* PL6 - ring indicator - input. FIXME: Change to S_PL_EINT6 for interrupt? */
	rpiowr(PL_CFG0_REG, rpiord(PL_CFG0_REG) & ~(PIN_CFG_MASK << PL6_CFG_SHIFT) | (0 << PL6_CFG_SHIFT));
}

static void
poweron(void)
{
	int timeout = 30;
/* 	setpmicvolt("DCDC1", 3300);
	setpmicstate("DCDC1", 1);
	delay(10); */
	iprint("PH: %x\n", piord(PH_DATA));
	/* PL7 - Baseband power */
	rpiowr(PL_DATA, rpiord(PL_DATA) | (1<<7));
	/* PC4 - Reset modem */
	piowr(PC_DATA, piord(PC_DATA) & ~(1<<4));
	
	/* Exit sleep state */
	piowr(PH_DATA, piord(PH_DATA) & ~(1<<7));
	piowr(PB_DATA, piord(PB_DATA) & ~(1<<2));
	delay(30);

	/* PB3 - power up LTE modem by pressing the power key */
	piowr(PB_DATA, piord(PB_DATA) | (1<<3));
	delay(600);
	piowr(PB_DATA, piord(PB_DATA) & ~(1<<3));

	/* Disable airplane mode */
	piowr(PH_DATA, piord(PH_DATA) |(1<<8));
	while((piord(PH_DATA) & (1<<9)) != 0 && timeout--)
	{
		delay(1000);
		iprint("Status still set.. %x\n", piord(PH_DATA));
	}
	iprint("PH status: %x\n", piord(PH_DATA));
	if(timeout <= 0) {
		iprint("Modem init timeout\n");
	}
	/* PH9 -> status. Nothing to do? */
	/* PH8 -> airplane mode. Don't care? */
	/* PH7 > sleep state. Don't care? */
	/* PL6 -> ring indicator. Don't care? */

}

void
modeminit(void)
{
	iprint("Modem init\n");
	pincfg();
	poweron();
}
void
ethereg25link(void)
{
//	modeminit();
	iprint("Ether link\n");
	/* addethercard("ethereg25", pnp);*/
}
