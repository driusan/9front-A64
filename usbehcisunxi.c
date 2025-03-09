/*
 * USB EHCI controller for A64 board / pinephone.
 *
 * Theoretically, this works, but nothing is getting enumerated.
 * It's possible that the Pinephone port is defaulting to peripheral
 * mode and the internal USB1 port doesn't have anything (the modem)
 * powered on, but this needs investigation.
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"
#include	"../port/usb.h"
#include	"usbehci.h"

#include "ccu.h"
#include "pio.h"

/* undocumented usbphy registers */
enum {
	PHYCSR = 0x0,
	PHYCTL = 0x10,
	OTGPHYCFG = 0x20,
};

enum {
	PmuIRQ = 0x0, /* documented on page 618 of A64 user manual */
	PmuUnkH3 = 0x10, /* undocumented */
};
static Ctlr ctlrs[2] = {
	{
		.base = PHYSDEV + USB0,
		.irq = IRQotgehci,
	},
	{
		.base = PHYSDEV + USB1,
		.irq = IRQusbehci,
	},
};

static void
usbphywr(u32int reg, u32int val)
{
	*IO(u32int, 0x1c19400 + reg) = val;
}

static u32int
usbphyrd(int reg) {
	return *IO(u32int, 0x1c19400 + reg);
}

static u32int
pmurd(Ctlr *ctlr, int reg)
{
	return *IO(u32int, ctlr->base + 0x800 + reg);
}
static void
pmuwr(Ctlr *ctlr, int reg, u32int val)
{
	*IO(u32int, ctlr->base + 0x800 + reg) = val;
}

static void
ehcireset(Ctlr *ctlr)
{
	int i;
	Eopio *opio;

	ilock(ctlr);
	opio = ctlr->opio;
	ehcirun(ctlr, 0);
	opio->cmd |= Chcreset;
	for(i = 0; i < 100; i++){
		if((opio->cmd & Chcreset) == 0)
			break;
		delay(1);
	}
	if(i == 100)
		print("ehci %#p controller reset timed out\n", ctlr->base);
	opio->cmd |= Citc1;
	switch(opio->cmd & Cflsmask){
	case Cfls1024:
		ctlr->nframes = 1024;
		break;
	case Cfls512:
		ctlr->nframes = 512;
		break;
	case Cfls256:
		ctlr->nframes = 256;
		break;
	default:
		panic("ehci: unknown fls %ld", opio->cmd & Cflsmask);
	}
	dprint("ehci: %d frames\n", ctlr->nframes);
	iunlock(ctlr);
}

/* descriptors need to be allocated in uncached memory */
static void*
tdalloc(ulong size, int, ulong)
{
	return ucalloc(size);
}

static void*
dmaalloc(ulong len)
{
	return mallocalign(ROUND(len, BLOCKALIGN), BLOCKALIGN, 0, 0);
}
static void
dmafree(void *data)
{
	free(data);
}

static void
otghostmode(void)
{
	u32int phy_csr = usbphyrd(PHYCSR);

	/* FIXME: Use proper constants. */
	phy_csr &= ~(1<<6|1<<5|1<<4);
	phy_csr |= 1<<17 | 1<<16;
	phy_csr &= ~(0x3<<12);
	phy_csr |= 3<<12; // or 2?
	phy_csr &= ~(0x3<<14);
	phy_csr |= 2<<14;


	u32int otg_phy_cfg = usbphyrd(OTGPHYCFG);
	otg_phy_cfg &= ~1;
	usbphywr(OTGPHYCFG, otg_phy_cfg);
	usbphywr(PHYCSR, phy_csr);

}
static int
reset(Hci *hp)
{
	Ctlr *ctlr;
	for(ctlr = ctlrs; ctlr->base != 0; ctlr++)
		if(!ctlr->active && (hp->port == 0 || hp->port == ctlr->base)){
			ctlr->active = 1;
			break;
		}
	if(ctlr->base == 0)
		return -1;

	hp->port = ctlr->base;
	hp->irq = ctlr->irq;
	hp->aux = ctlr;

	ctlr->opio =  (Eopio *)(VIRTIO + ctlr->base + 0x10);
	ctlr->capio = (void *)(VIRTIO + ctlr->base);
	hp->nports = 1;

	ctlr->tdalloc = tdalloc;
	ctlr->dmaalloc = dmaalloc;
	ctlr->dmafree = dmafree;

	ehcireset(ctlr);

	ehcimeminit(ctlr);
	ehcilinkage(hp);

	intrenable(hp->irq, hp->interrupt, hp, BUSUNKNOWN, hp->type);
	return 0;
}

static void
usbphyinit(void)
{

	clkenable(USBPHY_CFG_REG);
	/* XXX: Investigate if OHCI gate is needed for EHCI */
	if(openthegate("USBOHCI0") == -1){
		panic("Could not open USBEHCI0 gate");
	}

	if(openthegate("USBEHCI0") == -1){
		panic("Could not open USBEHCI0 gate");
	}

	if(openthegate("USB-OTG-OHCI") == -1) {
		panic("Could not open USB-OTG-OHCI");
	}
	if(openthegate("USB-OTG-EHCI") == -1) {
		panic("Could not open USB-OTG-EHCI");
	}
	/* do we need this? */
	if(openthegate("USB-OTG-Device") == -1) {
		panic("Could not open USB-OTG-Device");
	}

	pmuwr(&ctlrs[0], PmuUnkH3,  pmurd(&ctlrs[0], PmuUnkH3) & ~(0x2));
	pmuwr(&ctlrs[0], PmuIRQ,  1 | (1<<8) | (1<<9) | (1<<10));
	pmuwr(&ctlrs[1], PmuUnkH3,  pmurd(&ctlrs[1], PmuUnkH3) & ~(0x2));
	pmuwr(&ctlrs[1], PmuIRQ,  1 | (1<<8) | (1<<9) | (1<<10));
	otghostmode();
}

void
usbehcilink(void)
{
	usbphyinit();
	ehcidebug = 0;
	addhcitype("ehci", reset);
}
