/*
 * Backlight driver for Pinephone
 */
#include "u.h"
#include "../port/lib.h"

#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "pio.h"

#define R_PWM_CTRL_REG 0x0
#define	R_PWM_CH0_PERIOD 0x04

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

int brightness = 100;

void backlight(int pct){
	if(pct == 0) {
		pioset("PH10", 0);
	} else {
		setpmicstate("DLDO1", 1);
		setpmicstate("DLDO2", 1);
		pioset("PH10", 1);
	}
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
	piocfg("PL10", 2);

	/* disable r_pwm_ctrl_reg (pg 194) */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) & ~(1<<6));
	
	/* configure r_pwm_ch0_period (pg 195) */
	rpwmwr(R_PWM_CH0_PERIOD,  (0x4af << 16) | ((0x4af*brightness/100)&0xffff));
	/* enable r_pwm_ctrl_reg */
	rpwmwr(R_PWM_CTRL_REG, rpwmrd(R_PWM_CTRL_REG) | (1<<6) | (1<<4) | 0xf);
	/* configure PH10 for PIO output */
	piocfg("PH10", PioOutput);
	/* turn on */
	pioset("PH10", 1);
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
