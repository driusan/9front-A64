#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

/* Stubs for compatibility with pinephone kernel. These
  are referenced by axp803 but shouldn't be. */
int brightness;
void backlight(int)
{
}

void subarchinit(void)
{
}
