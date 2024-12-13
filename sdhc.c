/*
 * Allwinner A64 embedded sd/mmc host controller
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/sd.h"
#include "ccu.h"

#define CTRL_REG	0x00
#define		CTRL_FIFO_AC_MOD  (1<<31)
#define		CTRL_TIME_UNIT_CMD	(1<<12)
#define		CTRL_TIME_UNIT_DAT	(1<<11)
#define		CTRL_DDR_MOD_SEL	(1<<10)
#define		CTRL_CD_DBC_ENB	(1<<8)
#define		CTRL_DMA_ENB	(1<<5)
#define 	CTRL_INT_ENB	(1<<4)
#define		CTRL_DMA_RST	(1<<2)
#define		CTRL_FIFO_RST	(1<<1)
#define		CTRL_SOFT_RST	(1<<0)
#define CLKDIV_REG	0x04
#define		CLKDIV_MASK_DATA0	(1<<31)
#define		CLKDIV_CCLK_CTRL	(1<<17)
#define		CLKDIV_CCLK_ENB		(1<<16)
#define		CLKDIV_CCLK_DIV_MASK	0xff
#define 		CLKDIV_MAX CLKDIV_CCLK_DIV_MASK
#define TMOUT_REG	0x08
#define		TMOUT_DTO_LMT_MASK	0xffffff00
#define			TMOUT_DTO(x)		(x << 8)
#define		TMOUT_RTO_LMT_MASK	0xff
#define			TMOUT_RTO(x)		(x & TMOUT_RTO_LMT_MASK)
#define CTYPE_REG	0x0C
#define BLKSIZ_REG	0x10
#define		BLKSIZ_BLK_SZ_MASK	0xffff
#define		BLKSIZ_BLK_SZ(x)	(x & BLKSIZ_BLK_SZ_MASK)

#define BYTCNT_REG	0x14
#define CMD_REG	0x18
#define		CMD_CMD_LOAD	(1<<31)
#define		CMD_VOL_SW	(1<<28)
#define		CMD_BOOT_ABT	(1<<27)
#define		CMD_EXP_BOOT_ACK	(1<<26)
#define		CMD_BOOT_MOD_MASK	(3<<24)
#define		CMD_PRG_CLK	(1<<21)
#define		CMD_SEND_INIT_SEQ	(1<<15)
#define		CMD_STOP_ABT_CMD	(1<<14)
#define		CMD_WAIT_PRE_OVER	(1<<13)
#define		CMD_STOP_CMD_FLAG	(1<<12)
#define		CMD_TRANS_MODE	(1<<11)
#define		CMD_TRANS_DIR	(1<<10)
#define		CMD_DATA_TRANS	(1<<9)
#define		CMD_CHK_RESP_CRC	(1<<8)
#define		CMD_LONG_RESP	(1<<7)
#define		CMD_RESP_RCV (1<<6)
#define		CMD_MASK   (0x3f)

#define CMDARG_REG	0x1c
#define RESP0_REG	0x20
#define RESP1_REG	0x24
#define RESP2_REG	0x28
#define RESP3_REG	0x2c

#define INTMASK_REG	0x30
#define MINTSTS_REG	0x34
#define RINTSTS_REG	0x38
#define		INT_CARD_REMOVAL	(1<<31)
#define		INT_CARD_INSERT	(1<<30)
#define		INT_SDIOI_INT	(1<<16)
#define		INT_DEE	(1<<15)
#define		INT_ACD		(1<<14)
#define		INT_DSE_BC	(1<<13)
#define		INT_CB_IW	(1<<12)
#define		INT_FU_FO	(1<<11)
#define		INT_DSTO_VSD	(1<<10)
#define		INT_DTO_BDS	(1<<9)
#define		INT_RTO_BACK	(1<<8)
#define		INT_DCE		(1<<7)
#define		INT_RCE		(1<<6)
#define		INT_DRR		(1<<5)
#define		INT_DTR		(1<<4)
#define		INT_DTC		(1<<3)
#define 	INT_CC	(1<<2)
#define		INT_CMD_DONE	INT_CC
#define		INT_RE (1<<1)
#define		INT_CMD_FAILED	INT_RE

#define STATUS_REG	0x3C
#define		STATUS_DMA_REQ	(1<<31)
#define		STATUS_FIFO_LEVEL_MASK	(0xff << 17)
#define		STATUS_RESP_IDX_MASK	(0x1f << 11)
#define		STATUS_FSM_BUSY	(1<<10)
#define		STATUS_CARD_BUSY	(1<<9)
#define		STATUS_CARD_PRESENT (1<<8)
#define		STATUS_FSM_STA_MASK	(0xf << 4)
#define		STATUS_FIFO_FULL	(1<<3)
#define		STATUS_FIFO_EMPTY	(1<<2)
#define		STATUS_FIFO_TX_LEVEL	(1<<1)
#define		STATUS_FIFO_RX_LEVEL	(1<<0)

#define FIFOTH_REG	0x40
#define		FIFOTH_BSIZE_OF_TRANS_MASK (0xf << 28)
#define		FIFOTH_RX_TL_MASK (0xff << 16)
#define		FIFOTH_TX_TL_MASK (0xff)

#define FUNS_REG	0x44 /* A64 manually incorrectly calls this "CTRL" */
#define		FUNS_READ_WAIT (1<<1)
#define TBC0_REG	0x48
#define TBC1_REG	0x4C

#define CSDC_REG	0x54
#define		CSDC_CRC_DET_PARA_MASK (0xf)
#define A12A_REG	0x58
#define		A12A_SD_A12A_MASK (0xffff)
#define NTSR_REG	0x5C /* SMHC0 and SMHC1 only */
#define		NTSR_MODE_SELEC (1<<31)
#define		NTSR_SAMPLE_TIMING_PHASE	(0x3<<4)
#define HWRST_REG	0x78
#define		HWRST_HW_RESET	(1<<0)
#define DMAC_REG	0x80
#define		DMAC_DES_LOAD_CTRL (1<<31)
#define		DMAC_IDMAC_ENB (1<<7)
#define		DMAC_FIX_BUST_CTRL (1<<1)
#define		DMAC_IDMAC_RST (1<<0)
#define DLBA_REG	0x84

#define IDST_REG	0x88
#define IDIE_REG	0x8C
#define		ID_DMAC_ERR_STA		(0x7 << 10)
#define		ID_ABN_INT_SUM		(1<<9)
#define		ID_NOR_INT_SUM		(1<<8)
#define		ID_ERR_FLAG_SUM		(1<<5)
#define			ID_ERR_SUM_INT (1<<5)
#define		ID_DES_UNAVL_INT	(1<<4)
#define		ID_FATAL_BERR_INT	(1<<2)
#define		ID_RX_INT		(1<<1)
#define		ID_TX_INT		(1<<0)

#define THLD_REG	0x100
#define		THLD_CARD_RD_THLD_MASK	(0xfff<<16)
#define		THLD_CARD_WR_THLD_ENB	(1<<2) /* SMHC2 only */
#define		THLD_BCIG		(1<<1) /* SMHC2 only */
#define		THLD_CARD_RD_THLD_ENB	(1<<0)
#define EDSD_REG	0x10C
#define		EDSD_HS_MD_EN	(1<<31)
#define RES_CRC_REG	0x110
#define		RES_CRC_RESP_CRC_MASK	(0x3f)
#define DATA7_CRC_REG	0x114
#define DATA6_CRC_REG	0x118
#define DATA5_CRC_REG	0x11C
#define DATA4_CRC_REG	0x120
#define DATA3_CRC_REG	0x124
#define DATA2_CRC_REG	0x128
#define DATA1_CRC_REG	0x12C
#define DATA0_CRC_REG	0x130
#define CRC_STA_REG	0x134
#define		CRC_STA_CRC_STA_MASK (0x7)
#define DDC_REG		0x140
#define		DDC_DAT_DRV_PH_SEL	(1<<17)
#define		DDC_CMD_DRV_PH_SEL	(1<<16)
#define SAMP_DL_REG	0x144
#define		SAMP_DL_SAMP_DL_CAL_START	(1<<15)
#define		SAMP_DL_SAMP_DL_CAL_DONE	(1<<14)
#define		SAMP_DL_SAMP_DL_MASK	(0x3f << 8)
#define		SAMP_DL_SAMP_DL_SW_EN	(1<<7)
#define		SAMP_DL_SAMP_DL_SW_MASK	(0x3f<<0)
#define DS_DL_REG	0x148
#define FIFO_REG	0x200

// #define DBG if(ctrl->dev == SMHC0)
// #define DBG if(1)
#define DBG if(0)
typedef struct IdmacChain IdmacChain;

#define IDMAC_CONFIG_DES_OWNER_FLAG	(1<<31)
#define IDMAC_CONFIG_ERR_FLAG		(1<<30)
#define IDMAC_CONFIG_CHAIN_MOD		(1<<4)
#define	IDMAC_CONFIG_FIRST_FLAG		(1<<3)
#define IDMAC_CONFIG_LAST_FLAG		(1<<2)
#define IDMAC_CONFIG_DISABLE_INTERRUPT	(1<<1)
static struct IdmacChain {
	u32int config;
	u32int bufsz; // bufsize = 0-15, must be multiple of 4. 0 means skipped.
	u32int bufaddr; // ptr to physical address
	u32int nextdes; // ptr to next Idmac descriptor
};


typedef struct Ctrlr Ctrlr;
static struct Ctrlr {
	QLock;
	Rendez r;
	Rendez cmdr;

	/* internal settings for setup */
	const char *gatename;
	const char *intname;
	const int irq;
	const int mask_data0;
	const int ntsr;
	const int maxdma;
	
	/* state */
	int initialize;
	int dev;
	int clk;
	int datadone;
	int dma;
	IdmacChain* dmac;
	int autocmd;

	int cmddone;	
	int cmderr;
};


static u32int
RR(Ctrlr *ctrl, int reg)
{
	return *IO(u32int, PHYSDEV + ctrl->dev + reg);
}
static void
WR(Ctrlr *ctrl, int reg, u32int val)
{
	*IO(u32int, (PHYSDEV + ctrl->dev + reg)) = val;
}

static void debug_idst(Ctrlr *ctrl);
static void debug_status(Ctrlr *ctrl);
static void debug_rintsts(Ctrlr *ctrl);
static void dump_registers(Ctrlr *ctrl, char *header);

static int
datadone(void *a)
{
	Ctrlr* ctrl = a;
	return ctrl->datadone;
}

static int
cmddone(void *a)
{
	Ctrlr* ctrl = a;
	return ctrl->cmddone;
}

static int
clkprogwait(Ctrlr *ctrl)
{
	int timeout = 1000;

	while(timeout-- && (RR(ctrl, CMD_REG) & CMD_CMD_LOAD)){
		DBG {
			iprint("%s: wait for cmd..\n", ctrl->gatename);
			delay(100);
		}
		delay(5);
	}

	if(timeout == 0) {
		iprint("%s: cmd timeout\n", ctrl->gatename);
		DBG {
			debug_rintsts(ctrl);
			debug_idst(ctrl);
			debug_status(ctrl);
		}
		return 0;
	}
	WR(ctrl, RINTSTS_REG, RR(ctrl, RINTSTS_REG));
	return 1;
}

static int
cmdwait(Ctrlr *ctrl)
{
	DBG iprint("%s: Waiting for cmd...\n", ctrl->gatename);
	tsleep(&ctrl->cmdr, cmddone, ctrl, 3000);

	if(ctrl->cmderr == 1){
		iprint("%s: command failed.\n", ctrl->gatename);
		DBG {
			debug_rintsts(ctrl);
			debug_idst(ctrl);
			debug_status(ctrl);
			dump_registers(ctrl, "Failed command");
		}
		return 0;
	}
	if(ctrl->cmddone == 0) {
		iprint("%s: cmd timeout\n", ctrl->gatename);
		DBG {
			debug_rintsts(ctrl);
			debug_idst(ctrl);
			debug_status(ctrl);
		}
		return 0;
	}

	return 1;
}

static void sdhcinterrupt(Ureg*, void* a);

static int
sdhcinit(SDio* s)
{
	Ctrlr *ctrl = s->aux;

	DBG iprint("%s: init sdhc\n", ctrl->gatename);
	if (openthegate(ctrl->gatename) != 1){
		iprint("%s: Could not open the gate\n", ctrl->gatename);
		return -1;
	};
	if(!ctrl->initialize){
		return -1;
	}

	WR(ctrl, HWRST_REG, 0);
	delay(100);
	WR(ctrl, HWRST_REG, 1);
	delay(500);

	ctrl->dmac = 0;
	ctrl->autocmd = 0;
	ctrl->dma = 0;

	DBG iprint("%s: Enabling interrupt\n", ctrl->gatename);
	intrenable(ctrl->irq, sdhcinterrupt, ctrl, BUSUNKNOWN, ctrl->intname);

	/* XXX: This doesn't seem to be reliable */
	if (RR(ctrl, STATUS_REG) & STATUS_CARD_PRESENT){
		return 0;
	}
	iprint("%s: No card present\n", ctrl->gatename);
	return -1;
}

static void
calibratedelay(Ctrlr *ctrl, int reg)
{
	WR(ctrl, reg, RR(ctrl, reg) | (1<<7));
	/* Allegedly calibration causes performance degradation, needs investigation */
	/*
	uchar rst;
	DBG iprint("%s: Calibrating register %d\n", ctrl->gatename, reg);
	WR(ctrl->dev, reg, 0xa0); // enable software delay and set delay chain to 0x20
	WR(ctrl->dev, reg, 0x00); // clear the value
	WR(ctrl->dev, reg, 0x8000); // start calibrate delay chain
	DBG iprint("%s: Waiting for calibration to complete..\n", ctrl->gatename);
	while(!( RR(ctrl->dev, reg) & (1<<14) )){
		// wait until calibration done
		// delay(10);
	}
	
	rst = (RR(ctrl->dev, reg) & (0x3f << 8) >> 8);
	DBG iprint("%s: Enabling calibration with result of: %x\n", ctrl->gatename, rst);
	WR(ctrl->dev, reg, rst | (1<<7));
	*/
}

static void
setclkspeed(Ctrlr *ctrl, int speed)
{
	uint div, ext, hz;
	u32int buf;

	/* "New timing mode" has an internal divider of 2 */
	hz = speed*2;
	div = 1;
	
	if(ctrl->dev == SMHC2){
		hz *= 2;
		div = 2;
	}

	DBG iprint("%s: Setting clock divider to %d for frequency: %udHz.\n", ctrl->gatename, div, speed);
	WR(ctrl, RINTSTS_REG, RR(ctrl, RINTSTS_REG));

	/* disable clock */
	buf = RR(ctrl, CLKDIV_REG);
	buf &= ~(CLKDIV_CCLK_ENB|CLKDIV_MASK_DATA0|CLKDIV_CCLK_CTRL);
	if (ctrl->mask_data0)
		buf |= CLKDIV_MASK_DATA0;

	WR(ctrl, CLKDIV_REG, buf);
	WR(ctrl, CMD_REG, CMD_WAIT_PRE_OVER | CMD_PRG_CLK | CMD_CMD_LOAD);
	clkprogwait(ctrl);

	/* set sysclock to the speed we used to calculate the internal divider */
	clkdisable(ctrl->clk);
	setclkrate(ctrl->clk, hz);
	clkenable(ctrl->clk);

	DBG {
		ext = getclkrate(ctrl->clk);
		iprint("%s: Clock external rate is %ud\n", ctrl->gatename, ext);
	}
	/* update divider */
	buf = RR(ctrl, CLKDIV_REG) & ~0xff;
	buf |= (div-1);

	WR(ctrl, CLKDIV_REG, buf);
	/* set new timing register if necessary. no-op on SMHC2 */
	WR(ctrl, NTSR_REG, RR(ctrl, NTSR_REG) | NTSR_MODE_SELEC);
	calibratedelay(ctrl,SAMP_DL_REG);
	if (ctrl->dev == SMHC2){
		/* only for HS400 mode?
		calibratedelay(ctrl,DS_DL_REG);
		*/
	}
	buf = RR(ctrl, CLKDIV_REG) | CLKDIV_CCLK_ENB;
	WR(ctrl, CLKDIV_REG, buf);
	WR(ctrl, CMD_REG, CMD_WAIT_PRE_OVER | CMD_PRG_CLK | CMD_CMD_LOAD);
	DBG iprint("%s: Sending reset clock command..", ctrl->gatename);
	clkprogwait(ctrl);

	/* unmask data0 */
	if (ctrl->mask_data0){
		buf = RR(ctrl, CLKDIV_REG) & ~CLKDIV_MASK_DATA0;
		WR(ctrl, CLKDIV_REG, buf);
	}
	DBG iprint("%s: External clock: %uld, internal divider: %ud. Final: %uld\n", ctrl->gatename, getclkrate(ctrl->clk), div, getclkrate(ctrl->clk) / div);
}

static void
sdhcenable(SDio *s)
{
	Ctrlr *ctrl = s->aux;
	if (ctrl->dev == SMHC2){
		WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) | CTRL_TIME_UNIT_CMD | CTRL_TIME_UNIT_DAT );
	}
	WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) & ~CTRL_DDR_MOD_SEL);
	/* WR(ctrl, INTMASK_REG, 0xffffffff); */
	WR(ctrl, INTMASK_REG, ~(INT_SDIOI_INT));

	WR(ctrl, TMOUT_REG, TMOUT_DTO(0xffffff) | TMOUT_RTO(0xff));
	WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) | CTRL_INT_ENB);
}

static int
sdhccmd(SDio* s, SDiocmd* cmd, u32int arg, u32int *resp)
{
	Ctrlr* ctrl = s->aux;
	DBG iprint("%s: sdhc cmd %s arg: %ux\n", ctrl->gatename, cmd->name, arg);
	u32int c;

	c = cmd->index & CMD_MASK;
	if (cmd == &GO_IDLE_STATE)
		c |= CMD_SEND_INIT_SEQ;
	else if (cmd == &STOP_TRANSMISSION) {
		/* autocmd done handles this */
		return 0;
	}
	if(ctrl->dev == SMHC2 && cmd==&SD_SEND_OP_COND){
		/* SMHC2 is eMMC, not SD. We need to error so that
                   that sdmmc driver tries mmc.. */
		error("not sdcard");
	}
	c |= CMD_RESP_RCV;
	switch(cmd->resp){
	case 0:
		c &= ~(CMD_RESP_RCV);
		break;
	case 1:
		if(cmd->busy){
			c |= CMD_CHK_RESP_CRC;
			break;
		}
	default:
		c |= CMD_CHK_RESP_CRC;
		break;
	case 2:
		c |= CMD_LONG_RESP | CMD_CHK_RESP_CRC;
		break;
	case 3:
		break;
	}

	if(cmd->data){
		c |= CMD_DATA_TRANS;
		if(cmd->data & 1){
			c = c & ~CMD_TRANS_DIR; /* read */
		}else
			c |= CMD_TRANS_DIR; /* write */

		if (cmd->data > 2){
			ctrl->autocmd = 1;
			c |= CMD_STOP_CMD_FLAG;
		}
	}

	if(RR(ctrl, CMD_REG) & CMD_CMD_LOAD){
		panic("Command already in progress");
		// return -1;
	}
	qlock(ctrl);
	while(RR(ctrl, STATUS_REG) & (STATUS_CARD_BUSY|STATUS_FSM_BUSY)){
		delay(10);
	}
	/* clear errors */
	WR(ctrl, RINTSTS_REG, RR(ctrl, RINTSTS_REG));
	WR(ctrl, CMDARG_REG, arg);
	WR(ctrl, CMD_REG, c | CMD_CMD_LOAD);

	ctrl->cmddone = 0;
	ctrl->cmderr = 0;
	if (!cmdwait(ctrl))
		iprint("%s: Command %s (cmd register: %ux) with arg %ux failed\n", ctrl->gatename, cmd->name, c, arg);

	if(!(c & CMD_RESP_RCV)) {
		resp[0] = 0;
	} else if(c & CMD_LONG_RESP) {
		resp[0] = RR(ctrl, RESP0_REG);
		resp[1] = RR(ctrl, RESP1_REG);
		resp[2] = RR(ctrl, RESP2_REG);
		resp[3] = RR(ctrl, RESP3_REG);
		DBG iprint("mmc response: 0x%ux 0x%ux 0x%ux 0x%ux\n", resp[0], resp[1], resp[2], resp[3]);
	} else {
		resp[0] = RR(ctrl, RESP0_REG);
		DBG iprint("mmc short response: 0x%ux\n", resp[0]);
	}
	qunlock(ctrl);
	return 0;
}

static void
fiforeset(Ctrlr *ctrl)
{
	int timeout = 1000;
	WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) | CTRL_FIFO_RST);
	while(timeout-- && (RR(ctrl, CTRL_REG) & CTRL_FIFO_RST)) {
		DBG {
			iprint("%s: wait for fifo rst..\n", ctrl->gatename);
			delay(10);
		}
	}
	if(timeout==0){
		iprint("%s: FIFO reset timeout\n", ctrl->gatename);
		return;
	}
}

static void
sdhciosetup(SDio* s, int write, void* buf, int bsize, int bcount)
{
	Ctrlr *ctrl = s->aux;
	int timeout;
	int len = bsize*bcount;
	int ndesc, lenrem, i;
	IdmacChain *curdesc;
	DBG iprint("%s: sdhciosetup. %s %d*%d=%d\n", ctrl->gatename, write ? "write" : "read", bsize, bcount, len);

	WR(ctrl, DMAC_REG, RR(ctrl, DMAC_REG) & ~DMAC_IDMAC_ENB);
	WR(ctrl, BLKSIZ_REG, BLKSIZ_BLK_SZ(bsize));
	WR(ctrl, BYTCNT_REG, (u32int )len);

	/* arbitrary cutoff for dma */
	if (len < 512) {
		/* Transfer via FIFO register */
		ctrl->dma = 0;
		WR(ctrl, CTRL_REG, (RR(ctrl, CTRL_REG) & ~(CTRL_DMA_ENB)) | CTRL_FIFO_AC_MOD);
		fiforeset(ctrl);

		/* recommended values according to A64 User manual */
		if (strcmp(ctrl->gatename, "SMHC0")) {
			WR(ctrl, FIFOTH_REG, 8 | (7<<16) | (2<<28));
		}else{
			WR(ctrl, FIFOTH_REG, 240 | (15<<16) | (3<<28));
		}
	} else {
		/* Transfer via DMA */
		ctrl->dma = 1;
		ndesc = len / ctrl->maxdma;
		if (len % ctrl->maxdma != 0){
			ndesc++;
		}

		ctrl->dmac = sdmalloc(sizeof(IdmacChain)*(ndesc));
		DBG iprint("%s: Initiating DMA xfer\n", ctrl->gatename);
		ctrl->datadone = 0;
		lenrem = len;
		i = 0;
		while(lenrem > 0){
			curdesc = &ctrl->dmac[i];
			curdesc->config = 0;
			if (i == 0){
				curdesc->config |= IDMAC_CONFIG_FIRST_FLAG;
			}

			curdesc->config |= IDMAC_CONFIG_CHAIN_MOD;
			curdesc->config |= IDMAC_CONFIG_DES_OWNER_FLAG;

			curdesc->bufaddr = PADDR((uchar*) buf + i*ctrl->maxdma);

			if (lenrem > ctrl->maxdma){
				curdesc->bufsz = ctrl->maxdma;
				curdesc->nextdes = PADDR(&ctrl->dmac[i+1]);
				lenrem -= ctrl->maxdma;
				curdesc->config |= IDMAC_CONFIG_DISABLE_INTERRUPT;
			} else {
				curdesc->nextdes = 0;
				curdesc->bufsz = lenrem;
				lenrem = 0;
				curdesc->config |= IDMAC_CONFIG_LAST_FLAG;
			}
			i++;
		}

		dmaflush(1, ctrl->dmac, sizeof(IdmacChain)*ndesc);

		/* dma enable */
		WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) & ~CTRL_FIFO_AC_MOD | (CTRL_DMA_ENB));
		/* dma reset */
		WR(ctrl, CTRL_REG, RR(ctrl, CTRL_REG) | (CTRL_DMA_RST));

		/* idma reset  */
		timeout = 1000;
		WR(ctrl, DMAC_REG, RR(ctrl, DMAC_REG) | DMAC_IDMAC_RST);
		while(timeout-- && (RR(ctrl, DMAC_REG) & DMAC_IDMAC_RST)) {
			DBG {
				iprint("%s: wait for dmac reset..\n", ctrl->gatename);
				delay(10);
			}
		}
		if (timeout == 0){
			iprint("%s: DMAC reset timeout\n", ctrl->gatename);
			return;
		}
		/* interrupts */
		WR(ctrl, IDIE_REG,
			(write ? ID_TX_INT : ID_RX_INT)
			| ID_FATAL_BERR_INT
			| ID_DES_UNAVL_INT
			| ID_ERR_SUM_INT
		);
		WR(ctrl, DMAC_REG, DMAC_FIX_BUST_CTRL | DMAC_IDMAC_ENB);
		WR(ctrl, DLBA_REG, PADDR(ctrl->dmac));
		WR(ctrl, FIFOTH_REG, 8 | (7<<16) | (2<<28));
	}
	if (write)
		cachedwbse(buf, len);
	else
		cachedwbinvse(buf, len);
}

static void
sdhcinterrupt(Ureg*, void* a)
{
	Ctrlr *ctrl = a;
	// DBG iprint("sdhcinterrupt\n");
	u32int reg = RR(ctrl, RINTSTS_REG);
	WR(ctrl, RINTSTS_REG, reg);

	if(reg & INT_RE){
		ctrl->cmderr = 1;
		ctrl->cmddone = 1;
		DBG iprint("%s: command response error\n", ctrl->gatename);
	}
	if (reg & INT_CC){
		ctrl->cmddone = 1;
		DBG iprint("%s: command done\n", ctrl->gatename);
	}

	if (reg & (INT_CC|INT_RE)){
		wakeup(&ctrl->cmdr);
	}
	if (reg & INT_RTO_BACK){
		iprint("%s: response timeout\n", ctrl->gatename);
	}

	if (reg & INT_DRR){
		if (ctrl->dma){
			WR(ctrl, IDST_REG, ID_RX_INT|ID_RX_INT);
		}
	}

	if (reg & INT_DTC){
		// iprint("%s: Data transfer complete\n");
		ctrl->datadone = 1;
		wakeup(&ctrl->r);
	}
	if (reg & INT_ACD){
		DBG iprint("%s: Auto command done\n", ctrl->gatename);
		if (ctrl->datadone != 1){
			DBG iprint("%s: ACD before DTC\n", ctrl->gatename);
		}

	}

	/* Unhandled interrupts, just print them if they happen */
	if (reg & INT_CARD_REMOVAL)
		iprint("%s: Card removal\n", ctrl->gatename);
	if (reg & INT_CARD_INSERT)
		iprint("%s: Card insert\n", ctrl->gatename);
	if (reg & INT_SDIOI_INT)
		DBG iprint("%s: SDIO interrupt\n", ctrl->gatename);
	if (reg & INT_DEE)
		iprint("%s: Data end-bit error\n", ctrl->gatename);
	if (reg & INT_DSE_BC)
		iprint("%s: Data start error\n", ctrl->gatename);
	if (reg & INT_CB_IW)
		iprint("%s: Command busy / illegal write\n", ctrl->gatename);
	if (reg & INT_FU_FO)
		iprint("%s: FIFO underrun /overflow\n", ctrl->gatename);
	if (reg & INT_DSTO_VSD)
		iprint("%s: Data starvation timeout / v1.8 switch done\n", ctrl->gatename);
	if (reg & INT_DTO_BDS)
		iprint("%s: data timeout / boot data startout\n", ctrl->gatename);
	if (reg & INT_DCE){
		iprint("%s: data crc error\n", ctrl->gatename);
		// panic("CRC error");
	}
	if (reg & INT_RCE)
		iprint("%s: response crc error\n", ctrl->gatename);
}

static int
sdhcinquiry(SDio *, char* inquiry, int inqlen)
{
	return snprint(inquiry, inqlen,
		"Allwinner A64 SD-MMC Host Controller"
	);
}

static void
sdhcio(SDio* s, int write, uchar* buf, int len)
{
	Ctrlr *ctrl = s->aux;
	int i;
	u32int *wbuf; // buf in words
	DBG iprint("%s: sdhcio %s to %p sz %d\n", ctrl->gatename, write ? "write" : "read", buf, len);
	if(ctrl->autocmd)
		tsleep(&ctrl->r, datadone, ctrl, 3000);
	if (ctrl->dma == 0) {
		wbuf = (u32int *)buf;
		WR(ctrl, RINTSTS_REG, RR(ctrl, RINTSTS_REG));	
		DBG debug_status(ctrl);
		for(i = 0; i < (len / 4); i++){
			if (write){
				while(RR(ctrl, STATUS_REG) & (STATUS_FIFO_FULL)) {
					DBG iprint("Waiting to write data..\n");
					delay(5);
				}
				WR(ctrl, FIFO_REG, wbuf[i]);
			} else {
				while(RR(ctrl, STATUS_REG) & (STATUS_FIFO_EMPTY)) {
					// DBG iprint("Waiting for data...\n");
					delay(5);
				}
				wbuf[i] = RR(ctrl, FIFO_REG);
				// DBG iprint("%s: read %x\n", ctrl->gatename, wbuf[i]);
			}
		}
	} else {
		/* DMA */
		if(write)
			cachedwbse(buf, len);
		else
			cachedwbinvse(buf, len);
		dmaflush(0, buf, len);
		dmaflush(0, ctrl->dmac, sizeof(ctrl->dmac));
		if (ctrl->dmac->config & (1<<31) != 0)
			iprint("%s: dmac still owns descriptor!\n", ctrl->gatename);
		if(ctrl->datadone != 1) {
			iprint("%s: Data not done\n", ctrl->gatename);
		}
		DBG iprint("%s: DMA Complete! :%s\n", ctrl->gatename, (char *)buf);
		sdfree(ctrl->dmac);
		ctrl->dmac = nil;
	}
	WR(ctrl, IDST_REG, RR(ctrl, IDST_REG));
	WR(ctrl, RINTSTS_REG, RR(ctrl, RINTSTS_REG));
}

static void
sdhcbus(SDio* s, int width, int speed)
{
	Ctrlr *ctrl = s->aux;
	DBG iprint("sdhcbus width: %d speed: %d\n", width, speed);
	switch(width){
	case 1:
		WR(ctrl, CTYPE_REG, 0x00);
		break;
	case 4:
		WR(ctrl, CTYPE_REG, 0x01);
		break;
	case 8:
		WR(ctrl, CTYPE_REG, 0x02);
		break;
	case 0:
		break;
	default:
		iprint("%s: mmc bus: invalid width\n", ctrl->gatename);
	}
	if(speed){
		setclkspeed(ctrl, speed);
	}
}

static void
sdhcled(SDio* s, int)
{
	Ctrlr *ctrl = s->aux;
	DBG iprint("%s: sdhcled\n", ctrl->gatename);
}

/* Define and link controllers */
static Ctrlr ctrls[3] = {
	{
		.gatename = "SMHC0",
		.dev = SMHC0,
		.clk = SDMMC0_CLK_REG,
		.irq = IRQsdmmc0,
		.intname = "sdmmc0",
		.mask_data0 = 1,
		.ntsr = 1,
		.initialize = 1,
		.maxdma = 0x10000,
	},
	{
		.gatename = "SMHC1",
		.dev = SMHC1,
		.clk = SDMMC1_CLK_REG,
		.irq = IRQsdmmc1,
		.intname = "sdmmc1",
		.mask_data0 = 1,
		.ntsr = 1,
		.initialize = 0,
		.maxdma = 0x10000,
	},
	{
		.gatename = "SMHC2",
		.dev = SMHC2,
		.clk = SDMMC2_CLK_REG,
		.irq = IRQsdmmc2,
		.intname = "sdmmc2",
		.mask_data0 = 0,
		.ntsr = 0,
		.initialize = 1,
		.maxdma = 0xa000,
	}
};

static SDio mmc[3] = {
	/* SD Card */
	{
		.name = "SMHC0",
		.init = sdhcinit,
		.enable = sdhcenable,
		.inquiry = sdhcinquiry,
		.cmd = sdhccmd,
		.iosetup = sdhciosetup,
		.io = sdhcio,
		.bus = sdhcbus,
		.led = sdhcled,
		.aux = &ctrls[0]
	},
	/* Unused? */
	 {
		.name = "SMHC1",
		.init = sdhcinit,
		.enable = sdhcenable,
		.inquiry = sdhcinquiry,
		.cmd = sdhccmd,
		.iosetup = sdhciosetup,
		.io = sdhcio,
		.bus = sdhcbus,
		.led = sdhcled,
		.aux = &ctrls[1]
	},
	/* eMMC */
	{
		.name = "SMHC2",
		.init = sdhcinit,
		.enable = sdhcenable,
		.inquiry = sdhcinquiry,
		.cmd = sdhccmd,
		.iosetup = sdhciosetup,
		.io = sdhcio,
		.bus = sdhcbus,
		.led = sdhcled,
		.aux = &ctrls[2]
	}
};

void
sdhclink(void)
{	
	addmmcio(&mmc[0]);
	// addmmcio(&mmc[1]);
	addmmcio(&mmc[2]);
}

/* HERE BE DRAGONS */
static void 
debug_idst(Ctrlr *ctrl)
{
	u32int reg = RR(ctrl, IDST_REG);
	iprint("%s: IDST %ux", ctrl->gatename, reg);
	if (reg & ID_TX_INT)
		iprint(": transmit interrupt");
	if (reg & ID_RX_INT)
		iprint(": receive interrupt");
	if (reg & ID_FATAL_BERR_INT)
		iprint(": fatal bus error interrupt");
	if (reg & ID_DES_UNAVL_INT)
		iprint(": descriptor unavailable interrupt");
	if (reg & ID_ERR_FLAG_SUM)
		iprint(": card error summary");
	if (reg & ID_NOR_INT_SUM)
		iprint(": normal interrupt");
	if (reg & ID_ABN_INT_SUM)
		iprint(": abnormal interrupt");
	if ((reg & ID_DMAC_ERR_STA) >> 10)
		iprint(": bus error type %ux", (reg & ID_DMAC_ERR_STA) >> 10);
	iprint("\n");
}

static void
debug_status(Ctrlr *ctrl)
{
	u32int reg = RR(ctrl, STATUS_REG);
	iprint("%s: RSTATUS %ux", ctrl->gatename, reg);
	if (reg & STATUS_DMA_REQ)
		iprint(": dma request");
	if (reg & STATUS_FIFO_LEVEL_MASK)
		iprint(": fifo level (%ud)", (reg & STATUS_FIFO_LEVEL_MASK) >> 17);
	if (reg & STATUS_RESP_IDX_MASK)
		iprint(": response index (%ud)", (reg & STATUS_RESP_IDX_MASK) >> 11);
	if (reg & STATUS_FSM_BUSY)
		iprint(": fsm busy");
	if (reg & STATUS_CARD_BUSY)
		iprint(": card busy");
	if (reg & STATUS_CARD_PRESENT)
		iprint(": card present");
	iprint(": fsm state (%ud)", (reg & STATUS_FSM_STA_MASK) >> 4);
	if (reg & STATUS_FIFO_FULL)
		iprint(": fifo full");
	if (reg & STATUS_FIFO_EMPTY)
		iprint(": fifo empty");
	if (reg & STATUS_FIFO_TX_LEVEL)
		iprint(": fifo tx level trigger hit");
	if (reg & STATUS_FIFO_RX_LEVEL)
		iprint(": fifo rx level trigger hit");

	iprint("\n");
}

static void 
debug_rintsts(Ctrlr *ctrl)
{
	u32int reg = RR(ctrl, RINTSTS_REG);
	iprint("%s: RINTSTS %ux", ctrl->gatename, reg);
	if (reg & INT_CARD_REMOVAL)
		iprint(": Card removal");
	if (reg & INT_CARD_INSERT)
		iprint(": Card insert");
	if (reg & INT_SDIOI_INT)
		iprint(": SDIO interrupt");
	if (reg & INT_DEE)
		iprint(": Data end-bit error");
	if (reg & INT_ACD)
		iprint(": Auto command done completed");
	if (reg & INT_DSE_BC)
		iprint(": Data start error");
	if (reg & INT_CB_IW)
		iprint(": Command busy / illegal write");
	if (reg & INT_FU_FO)
		iprint(": FIFO underrun /overflow");
	if (reg & INT_DSTO_VSD)
		iprint(": Data starvation timeout / v1.8 switch done");
	if (reg & INT_DTO_BDS)
		iprint(": data timeout / boot data startout");
	if (reg & INT_RTO_BACK)
		iprint(": response timeout / boot ack received");
	if (reg & INT_DCE)
		iprint(": data crc error");
	if (reg & INT_RCE)
		iprint(": response crc error");
	if (reg & INT_DRR)
		iprint(": data receive request");
	if (reg & INT_DTC)
		iprint(": data transfer complete");
	if (reg & INT_CMD_DONE)
		iprint(": command complete");
	if (reg & INT_CMD_FAILED)
		iprint(": command failed");
	iprint("\n");
}

static void
dump_registers(Ctrlr *ctrl, char *header)
{
	iprint("%s: %s\n", ctrl->gatename, header);
#define dump(reg) DBG iprint("\treg(%ux) -> %ux\n", reg, RR(ctrl, reg))
	dump(CTRL_REG);
	dump(CLKDIV_REG);
	dump(TMOUT_REG);
	dump(CTYPE_REG);
	dump(BLKSIZ_REG);
	dump(BYTCNT_REG);
	dump(CMD_REG);
	dump(CMDARG_REG);
	dump(RESP0_REG);
	dump(RESP1_REG);
	dump(RESP2_REG);
	dump(RESP3_REG);
	dump(INTMASK_REG);
	dump(MINTSTS_REG);
	dump(RINTSTS_REG);
	dump(STATUS_REG);
	dump(FIFOTH_REG);
	dump(FUNS_REG);
	dump(TBC0_REG);
	dump(TBC1_REG);
	dump(CSDC_REG);
	dump(A12A_REG);
	dump(NTSR_REG);
	dump(HWRST_REG);
	dump(DMAC_REG);
	dump(DLBA_REG);
	dump(IDST_REG);
	dump(IDIE_REG);
	dump(THLD_REG);
	dump(EDSD_REG);
	dump(RES_CRC_REG);
	dump(DATA7_CRC_REG);
	dump(DATA6_CRC_REG);
	dump(DATA5_CRC_REG);
	dump(DATA4_CRC_REG);
	dump(DATA3_CRC_REG);
	dump(DATA2_CRC_REG);
	dump(DATA1_CRC_REG);
	dump(DATA0_CRC_REG);
	dump(CRC_STA_REG);
	dump(DDC_REG);
	dump(SAMP_DL_REG);
	dump(DS_DL_REG);
	// Reading changes value, so don't dump it
	// dump(FIFO_REG);
}
