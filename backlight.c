/*
 * Backlight driver for Pinephone
 */
#include "u.h"
#include "../port/lib.h"

#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

#define PIO_CFG_MASK(n) ~(7<<n)

#define PIO_PH_CFG01 0x100
#define	PIO_PH_DATA 0x10c

#define	RPIO_CFG10	0x04	/* Port L Configure Register 1 */

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

static u32int
rpwmrd(int offset)
{
	coherence();
	return *IO(u32int, (R_PWM + offset));
}

static void
rpwmwr(int offset, u32int val)
{
	*IO(u32int, (R_PWM + offset)) = val;
	coherence();
}
#define R_PWM_CTRL_REG 0x0
#define	R_PWM_CH0_PERIOD 0x04

int brightness = 100;

void backlight(int pct){
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*pct/100)&0xffff));
	brightness = pct;
};

/**
 * Configure the backlight pins for the Pinephone display
 */
void
backlightinit(void)
{
	/* configure PL10 for PWM */
	rpiowr(RPIO_CFG10, rpiord(RPIO_CFG10) & PIO_CFG_MASK(8) | (2<<8));
	/* disable r_pwm_ctrl_reg (pg 194) */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) & ~(1<<6));
	
	/* configure r_pwm_ch0_period (pg 195) */
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*brightness/100)&0xffff));
	/* enable r_pwm_ctrl_reg */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) | (1<<6) | (1<<4) | 0xf);
	/* configure PH10 for PIO output */
	piowr(PIO_PH_CFG01, piord(PIO_PH_CFG01) & PIO_CFG_MASK(8) | (1<<8));

	/* turn on */
	piowr(PIO_PH_DATA, piord(PIO_PH_DATA) | (1<<10));
}

void
backlightlink(void)
{
	char *pct;
	int bright = 90;
	backlightinit();
	if(pct = getconf("brightness")){
		bright = (int )strtol(pct, nil, 10);
		if(bright < 0)
			bright = 90;
		if(bright > 100)
			bright = 100;
	}
	backlight(bright);
}