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

static Ctlr ctlrs[3] = {
	{
		.base = VIRTIO + SYSCTL + USB0,
		.irq = IRQotgehci,
	},
	{
		.base = VIRTIO + SYSCTL + USB1,
		.irq = IRQusbehci,
	},
};

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

static int (*ehciportstatus)(Hci*,int);

static int
portstatus(Hci *hp, int port)
{
	Ctlr *ctlr;
	Eopio *opio;
	int r, sts;

	ctlr = hp->aux;
	opio = ctlr->opio;
	r = (*ehciportstatus)(hp, port);
	if(r & HPpresent){
		sts = opio->portsc[port-1];
		r &= ~(HPhigh|HPslow);
		if(sts & (1<<9))
			r |= HPhigh;
		else if(sts & 1<<26)
			r |= HPslow;
	}
	return r;
}

static int
reset(Hci *hp)
{
	Ctlr *ctlr;
	iprint("hp port: %ulld\n", hp->port);
	for(ctlr = ctlrs; ctlr->base != 0; ctlr++)
		if(!ctlr->active && (hp->port == 0 || hp->port == ctlr->base)){
			iprint("hp port: %ulld ctrl base: %ulld\n", hp->port, ctlr->base);
			ctlr->active = 1;
			break;
		}
	if(ctlr->base == 0)
		return -1;

	hp->port = ctlr->base;
	hp->irq = ctlr->irq;
	hp->aux = ctlr;

	ctlr->opio =  (Eopio *)(ctlr->base + 0x10);
	ctlr->capio = (void *)ctlr->base;
	hp->nports = 1;

	ctlr->tdalloc = tdalloc;
	ctlr->dmaalloc = dmaalloc;
	ctlr->dmafree = dmafree;

	ehcireset(ctlr);

	ehcimeminit(ctlr);
	ehcilinkage(hp);

	ehciportstatus = hp->portstatus;
	hp->portstatus = portstatus;

	intrenable(hp->irq, hp->interrupt, hp, BUSUNKNOWN, hp->type);
	return 0;

	// ctlr->r = vmap(ctlr->base, 0x1F0);
	// ctlr->opio = (Eopio *) ((uchar *) ctlr->r + 0x140);
	// ctlr->capio = (void *) ctlr->base;
	hp->nports = 1;	

	ctlr->tdalloc = tdalloc;
	ctlr->dmaalloc = dmaalloc;
	ctlr->dmafree = dmafree;

	ehcireset(ctlr);
	// ctlr->r[USBMODE] |= USBHOST;
	// ctlr->r[ULPI] = 1<<30 | 1<<29 | 0x0B << 16 | 3<<5;
	ehcimeminit(ctlr);
	ehcilinkage(hp);

	/* hook portstatus */
	ehciportstatus = hp->portstatus;
	hp->portstatus = portstatus;

	intrenable(hp->irq, hp->interrupt, hp, BUSUNKNOWN, hp->type);
	return 0;
}

static void
usbphyinit(void)
{
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
	/* we do not open the device gate, only support host mode */
	clkenable(USBPHY_CFG_REG);
}

void
usbehcilink(void)
{
	usbphyinit();
	ehcidebug = 0;
	addhcitype("ehci", reset);
}
