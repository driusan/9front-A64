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
#include "pio.h"


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
	/* PL7 - Baseband power */
	piocfg("PL7", PioOutput);

	/* PB3 - LTE modem power */
	piocfg("PB3", PioOutput);
	/* PB2 - DTR? Output? What is this? */
	piocfg("PB2", PioOutput);
	/* PC4 - modem reset */
	piocfg("PC4", PioOutput);
	/* PH9 - modem status */
	piocfg("PH9", PioInput);
	/* PH8 - airplane mode */
	piocfg("PH8", PioOutput);
	/* PH7 - sleep status -- conflicts with UART3 CTS */
	piocfg("PH7", PioOutput);
	/* PL6 - ring indicator - input. FIXME: Change to S_PL_EINT6 for interrupt? */
	piocfg("PL6", PioInput); 

	/* uart */
	if(openthegate("UART3") == -1)
		panic("Could not open UART3");
	piocfg("PD1", 3); /* UART3_RX */
	piocfg("PD0", 3); /* UART3_TX */

}

static void
poweron(void)
{
	int timeout = 30;

	/* PL7 - Baseband power */
	pioset("PL7", 1);
	/* PC4 - Reset modem */
	pioset("PC4", 0);

	/* Exit sleep state */
	pioset("PH7", 0);
	pioset("PB2", 0);
	delay(30);

	/* PB3 - power up LTE modem by pressing the power key */
	pioset("PB3", 1);
	delay(600);
	pioset("PB3", 0);
	/* Disable airplane mode. */
	pioset("PH8", 0);

	while(pioget("PH9") != 0 && timeout--)
	{
		delay(1000);
		iprint("Waiting for modem to initialize..\n");
	}
	if(timeout <= 0) {
		iprint("Modem init timeout\n");
	}

	/* PL6 -> ring indicator. Don't care? */

}

void
modeminit(void)
{
	pincfg();
	poweron();
}
void
ethereg25link(void)
{
	/* addethercard("ethereg25", pnp);*/
}
