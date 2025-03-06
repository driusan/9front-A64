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
	
};

static PioPin piopins[] = {
	{"PB0", PIO, 0x24, 0, 0x34, 0 },
	{"PB1", PIO, 0x24, 4, 0x34, 1 },
	{"PB2", PIO, 0x24, 8, 0x34, 2 },
	{"PB3", PIO, 0x24, 12, 0x34, 3 },
	{"PB4", PIO, 0x24, 16, 0x34, 4 },
	{"PB5", PIO, 0x24, 20, 0x34, 5 },
	{"PB6", PIO, 0x24, 24, 0x34, 6 },
	{"PB7", PIO, 0x24, 28, 0x34, 7 },
	{"PB8", PIO, 0x28, 0, 0x34, 8 },
	{"PB9", PIO, 0x28, 4, 0x34, 9 },
	{"PC0", PIO, 0x48, 0, 0x58, 0 },
	{"PC1", PIO, 0x48, 4, 0x58, 1 },
	{"PC2", PIO, 0x48, 8, 0x58, 2 },
	{"PC3", PIO, 0x48, 12, 0x58, 3 },
	{"PC4", PIO, 0x48, 16, 0x58, 4 },
	{"PC5", PIO, 0x48, 20, 0x58, 5 },
	{"PC6", PIO, 0x48, 24, 0x58, 6 },
	{"PC7", PIO, 0x48, 28, 0x58, 7 },
	{"PC8", PIO, 0x4c, 0, 0x58, 8 },
	{"PC9", PIO, 0x4c, 4, 0x58, 9 },
	{"PC10", PIO, 0x4c, 8, 0x58, 10 },
	{"PC11", PIO, 0x4c, 12, 0x58, 11 },
	{"PC12", PIO, 0x4c, 16, 0x58, 12 },
	{"PC13", PIO, 0x4c, 20, 0x58, 13 },
	{"PC14", PIO, 0x4c, 24, 0x58, 14 },
	{"PC15", PIO, 0x4c, 28, 0x58, 15 },
	{"PC16", PIO, 0x50, 0, 0x58, 16 },
	{"PD0", PIO, 0x6c, 0, 0x7c, 0 },
	{"PD1", PIO, 0x6c, 4, 0x7c, 1 },
	{"PD2", PIO, 0x6c, 8, 0x7c, 2 },
	{"PD3", PIO, 0x6c, 12, 0x7c, 3 },
	{"PD4", PIO, 0x6c, 16, 0x7c, 4 },
	{"PD5", PIO, 0x6c, 20, 0x7c, 5 },
	{"PD6", PIO, 0x6c, 24, 0x7c, 6 },
	{"PD7", PIO, 0x6c, 28, 0x7c, 7 },
	{"PD8", PIO, 0x70, 0, 0x7c, 8 },
	{"PD9", PIO, 0x70, 4, 0x7c, 9 },
	{"PD10", PIO, 0x70, 8, 0x7c, 10 },
	{"PD11", PIO, 0x70, 12, 0x7c, 11 },
	{"PD12", PIO, 0x70, 16, 0x7c, 12 },
	{"PD13", PIO, 0x70, 20, 0x7c, 13 },
	{"PD14", PIO, 0x70, 24, 0x7c, 14 },
	{"PD15", PIO, 0x70, 28, 0x7c, 15 },
	{"PD16", PIO, 0x74, 0, 0x7c, 16 },
	{"PD17", PIO, 0x74, 4, 0x7c, 17 },
	{"PD18", PIO, 0x74, 8, 0x7c, 18 },
	{"PD19", PIO, 0x74, 12, 0x7c, 19 },
	{"PD20", PIO, 0x74, 16, 0x7c, 20 },
	{"PD21", PIO, 0x74, 20, 0x7c, 21 },
	{"PD22", PIO, 0x74, 24, 0x7c, 22 },
	{"PD23", PIO, 0x74, 28, 0x7c, 23 },
	{"PD24", PIO, 0x78, 0, 0x7c, 24 },
	{"PE0", PIO, 0x90, 0, 0xa0, 0 },
	{"PE1", PIO, 0x90, 4, 0xa0, 1 },
	{"PE2", PIO, 0x90, 8, 0xa0, 2 },
	{"PE3", PIO, 0x90, 12, 0xa0, 3 },
	{"PE4", PIO, 0x90, 16, 0xa0, 4 },
	{"PE5", PIO, 0x90, 20, 0xa0, 5 },
	{"PE6", PIO, 0x90, 24, 0xa0, 6 },
	{"PE7", PIO, 0x90, 28, 0xa0, 7 },
	{"PE8", PIO, 0x94, 0, 0xa0, 8 },
	{"PE9", PIO, 0x94, 4, 0xa0, 9 },
	{"PE10", PIO, 0x94, 8, 0xa0, 10 },
	{"PE11", PIO, 0x94, 12, 0xa0, 11 },
	{"PE12", PIO, 0x94, 16, 0xa0, 12 },
	{"PE13", PIO, 0x94, 20, 0xa0, 13 },
	{"PE14", PIO, 0x94, 24, 0xa0, 14 },
	{"PE15", PIO, 0x94, 28, 0xa0, 15 },
	{"PE16", PIO, 0x98, 0, 0xa0, 16 },
	{"PE17", PIO, 0x98, 4, 0xa0, 17 },
	{"PF0", PIO, 0xb4, 0, 0xc4, 0 },
	{"PF1", PIO, 0xb4, 4, 0xc4, 1 },
	{"PF2", PIO, 0xb4, 8, 0xc4, 2 },
	{"PF3", PIO, 0xb4, 12, 0xc4, 3 },
	{"PF4", PIO, 0xb4, 16, 0xc4, 4 },
	{"PF5", PIO, 0xb4, 20, 0xc4, 5 },
	{"PF6", PIO, 0xb4, 24, 0xc4, 6 },
	{"PG0", PIO, 0xd8, 0, 0xe8, 0 },
	{"PG1", PIO, 0xd8, 4, 0xe8, 1 },
	{"PG2", PIO, 0xd8, 8, 0xe8, 2 },
	{"PG3", PIO, 0xd8, 12, 0xe8, 3 },
	{"PG4", PIO, 0xd8, 16, 0xe8, 4 },
	{"PG5", PIO, 0xd8, 20, 0xe8, 5 },
	{"PG6", PIO, 0xd8, 24, 0xe8, 6 },
	{"PG7", PIO, 0xd8, 28, 0xe8, 7 },
	{"PG8", PIO, 0xdc, 0, 0xe8, 8 },
	{"PG9", PIO, 0xdc, 4, 0xe8, 9 },
	{"PG10", PIO, 0xdc, 8, 0xe8, 10 },
	{"PG11", PIO, 0xdc, 12, 0xe8, 11 },
	{"PG12", PIO, 0xdc, 16, 0xe8, 12 },
	{"PG13", PIO, 0xdc, 20, 0xe8, 13 },
	{"PH0", PIO, 0xfc, 0, 0x10c, 0 },
	{"PH1", PIO, 0xfc, 4, 0x10c, 1 },
	{"PH2", PIO, 0xfc, 8, 0x10c, 2 },
	{"PH3", PIO, 0xfc, 12, 0x10c, 3 },
	{"PH4", PIO, 0xfc, 16, 0x10c, 4 },
	{"PH5", PIO, 0xfc, 20, 0x10c, 5 },
	{"PH6", PIO, 0xfc, 24, 0x10c, 6 },
	{"PH7", PIO, 0xfc, 28, 0x10c, 7 },
	{"PH8", PIO, 0x100, 0, 0x10c, 8 },
	{"PH9", PIO, 0x100, 4, 0x10c, 9 },
	{"PH10", PIO, 0x100, 8, 0x10c, 10 },
	{"PH11", PIO, 0x100, 12, 0x10c, 11 },
	{"PL0", R_PIO, 0x0, 0, 0x10, 0 },
	{"PL1", R_PIO, 0x0, 4, 0x10, 1 },
	{"PL2", R_PIO, 0x0, 8, 0x10, 2 },
	{"PL3", R_PIO, 0x0, 12, 0x10, 3 },
	{"PL4", R_PIO, 0x0, 16, 0x10, 4 },
	{"PL5", R_PIO, 0x0, 20, 0x10, 5 },
	{"PL6", R_PIO, 0x0, 24, 0x10, 6 },
	{"PL7", R_PIO, 0x0, 28, 0x10, 7 },
	{"PL8", R_PIO, 0x4, 0, 0x10, 8 },
	{"PL9", R_PIO, 0x4, 4, 0x10, 9 },
	{"PL10", R_PIO, 0x4, 8, 0x10, 10 },
	{"PL11", R_PIO, 0x4, 12, 0x10, 11 },
	{"PL12", R_PIO, 0x4, 16, 0x10, 12 },
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
