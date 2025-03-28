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

typedef struct PioPin PioPin;
struct PioPin {
	char *name;
	u32int memio;

	int cfgreg;
	int cfgoff;
	int datareg;
	int dataoff;

	int eintreg;
	int eintoff;
	
};

static PioPin piopins[] = {
	{"PB0", PIO, 0x24, 0, 0x34, 0, 0, 0 },
	{"PB1", PIO, 0x24, 4, 0x34, 1 ,0, 0},
	{"PB2", PIO, 0x24, 8, 0x34, 2 ,0, 0},
	{"PB3", PIO, 0x24, 12, 0x34, 3 ,0, 0},
	{"PB4", PIO, 0x24, 16, 0x34, 4 ,0, 0},
	{"PB5", PIO, 0x24, 20, 0x34, 5 ,0, 0},
	{"PB6", PIO, 0x24, 24, 0x34, 6 ,0, 0},
	{"PB7", PIO, 0x24, 28, 0x34, 7 ,0, 0},
	{"PB8", PIO, 0x28, 0, 0x34, 8 ,0, 0},
	{"PB9", PIO, 0x28, 4, 0x34, 9 ,0, 0},
	{"PC0", PIO, 0x48, 0, 0x58, 0 ,0, 0},
	{"PC1", PIO, 0x48, 4, 0x58, 1 ,0, 0},
	{"PC2", PIO, 0x48, 8, 0x58, 2 ,0, 0},
	{"PC3", PIO, 0x48, 12, 0x58, 3 ,0, 0},
	{"PC4", PIO, 0x48, 16, 0x58, 4 ,0, 0},
	{"PC5", PIO, 0x48, 20, 0x58, 5 ,0, 0},
	{"PC6", PIO, 0x48, 24, 0x58, 6 ,0, 0},
	{"PC7", PIO, 0x48, 28, 0x58, 7 ,0, 0},
	{"PC8", PIO, 0x4c, 0, 0x58, 8 ,0, 0},
	{"PC9", PIO, 0x4c, 4, 0x58, 9 ,0, 0},
	{"PC10", PIO, 0x4c, 8, 0x58, 10 ,0, 0},
	{"PC11", PIO, 0x4c, 12, 0x58, 11 ,0, 0},
	{"PC12", PIO, 0x4c, 16, 0x58, 12 ,0, 0},
	{"PC13", PIO, 0x4c, 20, 0x58, 13 ,0, 0},
	{"PC14", PIO, 0x4c, 24, 0x58, 14 ,0, 0},
	{"PC15", PIO, 0x4c, 28, 0x58, 15 ,0, 0},
	{"PC16", PIO, 0x50, 0, 0x58, 16 ,0, 0},
	{"PD0", PIO, 0x6c, 0, 0x7c, 0 ,0, 0},
	{"PD1", PIO, 0x6c, 4, 0x7c, 1 ,0, 0},
	{"PD2", PIO, 0x6c, 8, 0x7c, 2 ,0, 0},
	{"PD3", PIO, 0x6c, 12, 0x7c, 3 ,0, 0},
	{"PD4", PIO, 0x6c, 16, 0x7c, 4 , 0, 0},
	{"PD5", PIO, 0x6c, 20, 0x7c, 5 , 0, 0},
	{"PD6", PIO, 0x6c, 24, 0x7c, 6 , 0, 0},
	{"PD7", PIO, 0x6c, 28, 0x7c, 7 , 0, 0},
	{"PD8", PIO, 0x70, 0, 0x7c, 8 , 0, 0},
	{"PD9", PIO, 0x70, 4, 0x7c, 9 , 0, 0},
	{"PD10", PIO, 0x70, 8, 0x7c, 10 , 0, 0},
	{"PD11", PIO, 0x70, 12, 0x7c, 11 , 0, 0},
	{"PD12", PIO, 0x70, 16, 0x7c, 12 , 0, 0},
	{"PD13", PIO, 0x70, 20, 0x7c, 13 , 0, 0},
	{"PD14", PIO, 0x70, 24, 0x7c, 14 , 0, 0},
	{"PD15", PIO, 0x70, 28, 0x7c, 15 , 0, 0},
	{"PD16", PIO, 0x74, 0, 0x7c, 16 , 0, 0},
	{"PD17", PIO, 0x74, 4, 0x7c, 17 , 0, 0},
	{"PD18", PIO, 0x74, 8, 0x7c, 18 , 0, 0},
	{"PD19", PIO, 0x74, 12, 0x7c, 19 , 0, 0},
	{"PD20", PIO, 0x74, 16, 0x7c, 20 , 0, 0},
	{"PD21", PIO, 0x74, 20, 0x7c, 21 , 0, 0},
	{"PD22", PIO, 0x74, 24, 0x7c, 22 , 0, 0},
	{"PD23", PIO, 0x74, 28, 0x7c, 23 , 0, 0},
	{"PD24", PIO, 0x78, 0, 0x7c, 24 , 0, 0},
	{"PE0", PIO, 0x90, 0, 0xa0, 0 , 0, 0},
	{"PE1", PIO, 0x90, 4, 0xa0, 1 , 0, 0},
	{"PE2", PIO, 0x90, 8, 0xa0, 2 ,0, 0},
	{"PE3", PIO, 0x90, 12, 0xa0, 3 ,0, 0},
	{"PE4", PIO, 0x90, 16, 0xa0, 4 ,0, 0},
	{"PE5", PIO, 0x90, 20, 0xa0, 5 ,0, 0},
	{"PE6", PIO, 0x90, 24, 0xa0, 6 ,0, 0},
	{"PE7", PIO, 0x90, 28, 0xa0, 7 ,0, 0},
	{"PE8", PIO, 0x94, 0, 0xa0, 8 ,0, 0},
	{"PE9", PIO, 0x94, 4, 0xa0, 9 ,0, 0},
	{"PE10", PIO, 0x94, 8, 0xa0, 10 ,0, 0},
	{"PE11", PIO, 0x94, 12, 0xa0, 11 ,0, 0},
	{"PE12", PIO, 0x94, 16, 0xa0, 12 ,0, 0},
	{"PE13", PIO, 0x94, 20, 0xa0, 13 ,0, 0},
	{"PE14", PIO, 0x94, 24, 0xa0, 14 ,0, 0},
	{"PE15", PIO, 0x94, 28, 0xa0, 15 ,0, 0},
	{"PE16", PIO, 0x98, 0, 0xa0, 16 ,0, 0},
	{"PE17", PIO, 0x98, 4, 0xa0, 17 ,0, 0},
	{"PF0", PIO, 0xb4, 0, 0xc4, 0 ,0, 0},
	{"PF1", PIO, 0xb4, 4, 0xc4, 1 ,0, 0},
	{"PF2", PIO, 0xb4, 8, 0xc4, 2 ,0, 0},
	{"PF3", PIO, 0xb4, 12, 0xc4, 3 ,0, 0},
	{"PF4", PIO, 0xb4, 16, 0xc4, 4 ,0, 0},
	{"PF5", PIO, 0xb4, 20, 0xc4, 5 ,0, 0},
	{"PF6", PIO, 0xb4, 24, 0xc4, 6 ,0, 0},
	{"PG0", PIO, 0xd8, 0, 0xe8, 0 ,0, 0},
	{"PG1", PIO, 0xd8, 4, 0xe8, 1 ,0, 0},
	{"PG2", PIO, 0xd8, 8, 0xe8, 2 ,0, 0},
	{"PG3", PIO, 0xd8, 12, 0xe8, 3 ,0, 0},
	{"PG4", PIO, 0xd8, 16, 0xe8, 4 ,0, 0},
	{"PG5", PIO, 0xd8, 20, 0xe8, 5 ,0, 0},
	{"PG6", PIO, 0xd8, 24, 0xe8, 6 ,0, 0},
	{"PG7", PIO, 0xd8, 28, 0xe8, 7 ,0, 0},
	{"PG8", PIO, 0xdc, 0, 0xe8, 8 ,0, 0},
	{"PG9", PIO, 0xdc, 4, 0xe8, 9 ,0, 0},
	{"PG10", PIO, 0xdc, 8, 0xe8, 10 ,0, 0},
	{"PG11", PIO, 0xdc, 12, 0xe8, 11 ,0, 0},
	{"PG12", PIO, 0xdc, 16, 0xe8, 12 ,0, 0},
	{"PG13", PIO, 0xdc, 20, 0xe8, 13 ,0, 0},
	{"PH0", PIO, 0xfc, 0, 0x10c, 0 , 0x240, 0},
	{"PH1", PIO, 0xfc, 4, 0x10c, 1 , 0x240, 4},
	{"PH2", PIO, 0xfc, 8, 0x10c, 2 , 0x240, 8},
	{"PH3", PIO, 0xfc, 12, 0x10c, 3 , 0x240, 12},
	{"PH4", PIO, 0xfc, 16, 0x10c, 4 , 0x240, 16},
	{"PH5", PIO, 0xfc, 20, 0x10c, 5 , 0x240, 20},
	{"PH6", PIO, 0xfc, 24, 0x10c, 6 , 0x240, 24},
	{"PH7", PIO, 0xfc, 28, 0x10c, 7 , 0x240, 28},
	{"PH8", PIO, 0x100, 0, 0x10c, 8 , 0x244, 0},
	{"PH9", PIO, 0x100, 4, 0x10c, 9 , 0x244, 4},
	{"PH10", PIO, 0x100, 8, 0x10c, 10 , 0x244, 8},
	{"PH11", PIO, 0x100, 12, 0x10c, 11 , 0x244, 12},
	{"PL0", R_PIO, 0x0, 0, 0x10, 0 ,0, 0},
	{"PL1", R_PIO, 0x0, 4, 0x10, 1 ,0, 0},
	{"PL2", R_PIO, 0x0, 8, 0x10, 2 ,0, 0},
	{"PL3", R_PIO, 0x0, 12, 0x10, 3 ,0, 0},
	{"PL4", R_PIO, 0x0, 16, 0x10, 4 ,0, 0},
	{"PL5", R_PIO, 0x0, 20, 0x10, 5 ,0, 0},
	{"PL6", R_PIO, 0x0, 24, 0x10, 6 ,0, 0},
	{"PL7", R_PIO, 0x0, 28, 0x10, 7 ,0, 0},
	{"PL8", R_PIO, 0x4, 0, 0x10, 8 ,0, 0},
	{"PL9", R_PIO, 0x4, 4, 0x10, 9 ,0, 0},
	{"PL10", R_PIO, 0x4, 8, 0x10, 10 ,0, 0},
	{"PL11", R_PIO, 0x4, 12, 0x10, 11 ,0, 0},
	{"PL12", R_PIO, 0x4, 16, 0x10, 12 ,0, 0},
	{ nil, },
};

static u32int
piordcfg(PioPin *p)
{
	return *IO(u32int, p->memio + p->cfgreg);
}
static void
piowrcfg(PioPin *p, u32int val)
{
	*IO(u32int, p->memio + p->cfgreg) = val;
}
static void
piowreintcfg(PioPin *p, u32int val)
{
	*IO(u32int, p->memio + p->cfgreg) = val;
}

static u32int
piorddata(PioPin *p)
{
	return *IO(u32int, p->memio + p->datareg);
}
static void
piowrdata(PioPin *p, u32int val)
{
	*IO(u32int, p->memio + p->datareg) = val;
}

static PioPin*
findpio(char *name)
{
	PioPin *p;
	for(p = piopins; p != nil; p++){
		if(cistrcmp(name, p->name) == 0){
			return p;
		} 
	}
	return nil;
}

int
piocfg(char *name, int cfg)
{
	PioPin *p = findpio(name);
	u32int reg;
	if (p == nil) return -1;
	reg = piordcfg(p);
	reg &= ~(0x7 << p->cfgoff);
	reg |= cfg<<p->cfgoff;
	piowrcfg(p, reg);

	return 1;
}

int pioset(char *name, int on)
{
	PioPin *p = findpio(name);
	u32int reg;

	if(p == nil) return -1;
	reg = piorddata(p);
	if(on)
		reg |= 1<<p->dataoff;
	else
		reg &= ~(1<<p->dataoff);
	piowrdata(p, reg);
	return 0;
}

int pioget(char *name)
{
	PioPin *p = findpio(name);
	u32int reg;

	if(p == nil) return -1;
	reg = piorddata(p);
	return (reg >> p->dataoff) & 1;
}

void pioeintcfg(char *name, int val)
{
	PioPin *p = findpio(name);
	u32int reg;
	if(p->eintreg == 0){
		print("FIXME: Unknown eintcfg for pin %s\n", name);
		return;
	}
	print("%s: Setting eint to %x", name, val);
	reg = *IO(u32int, p->memio + p->eintreg);
	reg &= ~(0xf << p->eintoff);
	reg |= val<<p->eintoff;

	*IO(u32int, p->memio + p->eintreg) = reg;
	/* Enable interrupt. FIXME: Use real value for pin. This is PH4. */
	*IO(u32int, p->memio + 0x250) = 1<<4;
	print("%s: %x = %x\n", name, p->eintreg, reg);

}
