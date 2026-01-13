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

void
subarchinit(void)
{
	/* These should go in the conf file instead of devarch,
		but they need to be called after rsbinit.
		Moving rsb to the conf file causes things to freeze on boot
		(presumably because of some other timing/order dependency
	*/
	lcdinit();
	deinit();
	modeminit();

	addarchfile("touchevent", DMEXCL|0444, toucheventread, nil);
}
