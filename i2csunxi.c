/*
 * i2c driver for Pinephone 
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"
#include "../port/i2c.h"
#include "ccu.h"

#define DEBUG if(0)

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

static enum {
	STATE_STARTING,
	STATE_WAIT_ADDR,
		STATE_GOT_ADDRESS,
	STATE_WR_BYTE,
	STATE_RD_BYTE,
	STATE_IDLE
};

typedef struct Ctlr Ctlr;
struct Ctlr
{
	QLock;
	const char *gatename;
	int	addr;
	int	irq;

	I2Cbus *bus;
	int	ack;

	Rendez r;
};

static u32int twird(Ctlr *ctlr, int offset) {
	return *IO(u32int, ctlr->addr + offset);
}

static void twiwr(Ctlr *ctlr, u32int offset, u32int val) {
	*IO(u32int, ctlr->addr + offset) = val;
}

enum {
	TWI_ADDR	=	0x0000,	/*TWI Slave address*/
	TWI_XADDR	=	0x0004,	/*TWI Extended slave address*/
	TWI_DATA	=	0x0008,	/*TWI Data byte*/
	TWI_CNTR	=	0x000C,	/*TWI Control register*/
	TWI_STAT	=	0x0010,	/*TWI Status register*/
	TWI_CCR		=	0x0014,	/*TWI Clock control register*/
	TWI_SRST	=	0x0018,	/*TWI Software reset*/
	TWI_EFR		=	0x001C,	/*TWI Enhance Feature register*/
	TWI_LCR		=	0x0020,	/*TWI Line Control register*/
};

/* TWI_ADDR bits */
enum {
	GCE		=	(1<<0), /* general call enable */
};

/* TWI_CNTR bits */
enum {
	INT_EN		=	(1<<7), /* Interrupt enable */
	BUS_EN		=	(1<<6), /* Bus enable */
	M_STA		=	(1<<5), /* Master mode start */
	M_STO		=	(1<<4), /* Master mode stop */
	INT_FLAG	=	(1<<3), /* Interrupt flag */
	A_ACK		=	(1<<2), /* Assert acknowledge */
};


/* (some) TWI_STAT values */
enum {
	STAT_ERROR	=	0x00, /* Status Bus Error */
	STAT_START	=	0x08, /* Start condition transmitted */
	STAT_RESTART	=	0x10, /* Repeated start condition transmitted */

	STAT_WADDR_ACK	=	0x18, /* Write address written, ack received */
	STAT_WADDR_NACK  =	0x20, /* Write address written, no ack received */

	STAT_DATA_ACK	=	0x28, /* Data byte transmitted, ack received */
	STAT_DATA_NACK	=	0x30, /* Data byte transmitted, no ack received */

	STAT_RADDR_ACK	=	0x40, /* Address + read bit written, ack received */
	STAT_RADDR_NACK	=	0x48, /* Address + read bit written, no ack received */

	STAT_RDATA_ACK	=	0x50, /* Data byte */
	STAT_RDATA_NACK	=	0x58, 

	STAT_IDLE	=	0xf8, /* Idle */
};

/* TWI_LCR bits */
enum {
	LCR_SCL_STATE	= (1<<5),
	LCR_SDA_STATE	= (1<<4),
	LCR_SCL_CTL	= (1<<3),
	LCR_SCL_CTL_EN	= (1<<2),
	LCR_SDA_CTL	= (1<<1),
	LCR_SDA_CTL_EN	= (1<<0)
};

static void dump_regs(Ctlr *ctlr){
	u32int reg = twird(ctlr, TWI_STAT);
	switch(reg){
	case	STAT_ERROR:
		iprint("	Status: Bus error\n");
		break;
	case STAT_START:
		iprint("	Status: Start condition transmitted\n");
		break;
	case STAT_RESTART:
		iprint("	Status: Restart condition transmitted\n");
		break;
	case STAT_WADDR_ACK:
		iprint("	Status: Address + write bit written, ack received\n");
		break;
	case STAT_WADDR_NACK:
		iprint("	Status: Address + write bit written, no ack received\n");
		break;
	case STAT_DATA_ACK:
		iprint("	Status: Data byte transmitted, ack received\n");
		break;
	case STAT_DATA_NACK:
		iprint("	Status: Data byte transmitted, no ack received\n");
		break;
	case STAT_RDATA_ACK:
		iprint("	Status: Read data byte ack sent\n");
		break;
	case STAT_RDATA_NACK:
		iprint("	Status: Read data byte no acck sent\n");
		break;
	case STAT_RADDR_ACK:
		iprint("	Status: Address + read bit written, ack received\n");
		break;
	case STAT_RADDR_NACK:
		iprint("	Status: Address + read bit written, no ack received\n");
		break;
	case STAT_IDLE:
		iprint("	Status: idle\n");
		break;
	default:
		iprint("	Status: %x\n", reg);
	}

	reg = twird(ctlr, TWI_CNTR);
	iprint("	CNTR (%x): ", reg);
	if(reg & INT_EN) 
		iprint(" Interrupt enabled");
	if(reg & BUS_EN)
		iprint(" Bus enabled");
	if(reg & M_STA)
		iprint(" Master mode start (in progress)");
	if(reg & M_STO)
		iprint(" Master mode stop (in progress)");
	if(reg & INT_FLAG)
		iprint(" Interrupt flag");
	if(reg & A_ACK)
		iprint(" Assert acknowledge");
	iprint("\n");
	reg = twird(ctlr, TWI_LCR);
	iprint("	LCR: (%x):", reg);
	iprint(" SCL %s", (reg & LCR_SCL_STATE) ? "high" : "low");
	iprint(" SDA: %s", (reg & LCR_SDA_STATE) ? "high" : "low");
	iprint("\n");
}

static void
twiinterrupt(Ureg*, void* a){
	Ctlr *ctlr = a;
	u32int status = twird(ctlr, TWI_STAT);

  	DEBUG {
		iprint("Interrupt on %s:\n", ctlr->gatename);
		dump_regs(ctlr);
	}

	switch(status){
	case STAT_START:
	case STAT_RESTART:
		wakeup(&ctlr->r);
		break;
	case STAT_RADDR_ACK:
	case STAT_WADDR_ACK:
		ctlr->ack = 1;
		wakeup(&ctlr->r);
		break;
	case STAT_RADDR_NACK:
	case STAT_WADDR_NACK:
		ctlr->ack = 0;
		coherence();
		wakeup(&ctlr->r);
		break;
	case STAT_DATA_ACK:
		ctlr->ack = 1;
		wakeup(&ctlr->r);
		break;
	case STAT_DATA_NACK:
		ctlr->ack = 0;
		wakeup(&ctlr->r);
		break;
	case STAT_RDATA_ACK:
	case STAT_RDATA_NACK:
		/* we don't care if we sent a nack/ack, just let the caller know we read */
		ctlr->ack = 1;
		wakeup(&ctlr->r);
		break;
	case 0xf9:
		/* ??? */
		break;
	default:
			dump_regs(ctlr);
			panic("Unexpected status: %x.\n", status);
	}
}

#define PIO_CFG0_REG 0xfc
#define PE_CFG1_REG 0x94
static void
pincfg(void)
{
	/* pg pull register
	 set bits 0-2 = 2 (PH0_SELECT)
	 set bits 4-6 = 2 (PH1_SELECT)
	A64 user manual, page 401
	*/
	/* TWI0 */
	piowr(PIO_CFG0_REG, piord(PIO_CFG0_REG) 
		& ~(0x7 | 0x7<<4) | 2 | (2<<4)
	);
	/* TWI1 */
	piowr(PIO_CFG0_REG, piord(PIO_CFG0_REG) 
		& ~(0x7<<8 | 0x7<<12) | 2<<8 | (2<<12)
	);
	/* TWI2 */
	piowr(PE_CFG1_REG, piord(PE_CFG1_REG) 
		& ~(0x7<<8 | 0x7<<12) | 2<<8 | (2<<12)
	);

#define PH_CFG0_REG 0xfc
#define PH_CFG1_REG 0x100
#define PH_DAT_REG 0x10c
	/* XXX This is for the touch panel, not TWI. Move it. */

	piowr(PH_CFG0_REG, piord(PH_CFG0_REG) & ~(0x7<<16)| (6<<16)); // ph4, interrupt
	piowr(PH_CFG1_REG, piord(PH_CFG1_REG) & ~(0x7<<12)| (1<<12));// ph11, reset, output
	piowr(PH_DAT_REG, piord(PH_DAT_REG) | (1<<11));
	delay(100);
	piowr(PH_DAT_REG, piord(PH_DAT_REG) | (0<<11));
	delay(100);
}

static int
i2cwait(Ctlr *ctlr, u32int state)
{
	int timeout = 100;
	u32int reg;
	while((reg = twird(ctlr, TWI_STAT)) != state) {
		if(state == STAT_IDLE && reg == 0xf9) break;
		if(timeout-- < 0){
			return 0;
		}
		tsleep(&ctlr->r, return0, nil, 100);
		DEBUG iprint("i2c waiting state %x got %x\n", state, reg);
		dump_regs(ctlr);
	}
	return 1;
}

static void
i2crst(Ctlr *ctlr)
{
	DEBUG iprint("APB1: %uld APB2: %uld\n", getclkrate(AHB1_APB1_CFG_REG), getclkrate(APB2_CFG_REG));
	int baseclk = getclkrate(APB2_CFG_REG);

	int n = 1;
	int m = 2;
	int realclk = (baseclk / (1<<n)) / (10*(m+1));
	DEBUG iprint("APB2_CFG_REG: %dHz, after div: %d\n", baseclk, realclk);
	twiwr(ctlr, TWI_SRST, 1);
	delay(100);
	twiwr(ctlr, TWI_SRST, 0);
	assert(m <= 7 && n <= 7);
	twiwr(ctlr, TWI_CCR, (m<<3) | n);
	twiwr(ctlr, TWI_ADDR, 0);
	twiwr(ctlr, TWI_XADDR, 0);
	twiwr(ctlr, TWI_CNTR, BUS_EN | M_STO);
	assert(realclk == ctlr->bus->speed);
}

static int
i2cbusy(Ctlr *ctlr)
{
	return !i2cwait(ctlr, STAT_IDLE);
}

static int
i2cstart(Ctlr *ctlr, int restart)
{
	assert((twird(ctlr, TWI_CNTR) & M_STA) == 0);
	twiwr(ctlr, TWI_CNTR, INT_EN | BUS_EN | M_STA | A_ACK | (restart ? INT_FLAG: 0));
	tsleep(&ctlr->r, return0, nil, 1000);
	return twird(ctlr, TWI_STAT) == (restart ? STAT_RESTART : STAT_START);
}

static int
init(I2Cbus *bus)
{
	Ctlr *ctlr = bus->ctlr;
	ctlr->bus = bus;

	if(openthegate(ctlr->gatename) == -1){
		panic("Gate open failed");
	}

	pincfg();
	qlock(ctlr);
	i2crst(ctlr);
	qunlock(ctlr);
	intrenable(ctlr->irq, twiinterrupt, ctlr, BUSUNKNOWN, bus->name);

	return 0;
}

static int
io(I2Cdev *dev, uchar *pkt, int olen, int ilen)
{
	int o, rw;
	Ctlr *ctlr = dev->bus->ctlr;

	if (dev->a10) {
		return 0;
	}

	if (i2cbusy(ctlr)) {
		print("%s: i2c busy.\n", ctlr->gatename);
		return 0;
	}
	qlock(ctlr);



	rw = pkt[0] & 1;
	pkt[0] &= ~1; /* clear read bit to write address */
	if(i2cstart(ctlr, 0) == 0){
		iprint("%s: Could not send start condition\n", ctlr->gatename);
		DEBUG dump_regs(ctlr);
		o = 0;
		goto ioout;
	}

	for(o = 0; o < olen;o++){
		ctlr->ack = -1;

		twiwr(ctlr, TWI_DATA, pkt[o]);
		twiwr(ctlr, TWI_CNTR, INT_EN | BUS_EN | INT_FLAG | A_ACK);
		tsleep(&ctlr->r, return0, nil, 1000);
		/* this shouldn't be necessary, but something somewhere is caching the value and I can't find any way to invalidate it.  */
		while(ctlr->ack == -1);

		if(ctlr->ack != 1){
			DEBUG iprint("%s: No ack (%d) on %x\n", ctlr->gatename, ctlr->ack, dev->addr);
			o = 0;
			goto ioout;
		}
	}

	if (ilen == 0) goto ioout;

	pkt[0] |= rw; /* set read bit and re-start */
	if (i2cstart(ctlr, 1) == 0) {
		print("%s: Could not re-start for read\n", ctlr->gatename);
		goto ioout;	
	}
	ctlr->ack = -1;
	twiwr(ctlr, TWI_DATA, pkt[0]);
	twiwr(ctlr, TWI_CNTR, INT_EN | BUS_EN | INT_FLAG | A_ACK);
	while(ctlr->ack == -1);
	if (ctlr->ack != 1) {
		DEBUG iprint("No ack after restart (%d)\n", ctlr->ack);
		o = 0;
		goto ioout;
	}
	/* Restart with read bit set */
	for(int i = 1; i <= ilen;i++){
		ctlr->ack = -1;
		twiwr(ctlr, TWI_CNTR, INT_EN | BUS_EN | INT_FLAG | (i == ilen-1 ? A_ACK : 0));
		tsleep(&ctlr->r, return0, nil, 1000);
		while(ctlr->ack == -1);
		if(ctlr->ack != 1) {
			print("%s: Could not read byte, ack: %x\n", ctlr->gatename, ctlr->ack);	
		}
		pkt[o++] = twird(ctlr, TWI_DATA);


		DEBUG print("Read byte: %x\n", pkt[o-1]);
	}

ioout:
	DEBUG iprint("Sending stop\n");
	assert((twird(ctlr, TWI_CNTR) & M_STO) == 0);
	twiwr(ctlr, TWI_CNTR, INT_EN | BUS_EN | M_STO | INT_FLAG | A_ACK);
	
	DEBUG iprint("dev: %x o: %d olen+ilen: %d\n", dev->addr, o, olen+ilen);
	qunlock(ctlr);
	return o;
}



static Ctlr ctlr0 = {
	.gatename = "TWI0",
	.addr = TWI0,
	.irq = IRQtwi0,
};
static Ctlr ctlr1 = {
	.gatename = "TWI1",
	.addr = TWI1,
	.irq = IRQtwi1,
};
static Ctlr ctlr2 = {
	.gatename = "TWI2",
	.addr = TWI2,
	.irq = IRQtwi2,
};

void
i2csunxilink(void)
{
	static I2Cbus i2c0 = { "i2c0", 400000, &ctlr0, init, io };
	static I2Cbus i2c1 = { "i2c1", 400000, &ctlr1, init, io };
	static I2Cbus i2c2 = { "i2c2", 400000, &ctlr2, init, io };

	addi2cbus(&i2c0);
	addi2cbus(&i2c1);
	addi2cbus(&i2c2);
}
