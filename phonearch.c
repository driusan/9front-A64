#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

static long
toucheventread(Chan*, void *a, long n, vlong)
{
	touchwait();
	n = readstr(0, a, n, "touch\n");
	return n;
}

int
displayishdmi(void)
{
	return 0;
}

void
subarchinit(void)
{
	lcdinit();
	deinit(720, 1440);
	modeminit();

	addarchfile("touchevent", DMEXCL|0444, toucheventread, nil);
}
