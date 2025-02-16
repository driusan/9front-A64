#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../arm64/sysreg.h"

#define INITMAP	(ROUND((uintptr)end + BY2PG, PGLSZ(1))-KZERO)

/*
 * Create initial identity map in top-level page table
 * (L1BOT) for TTBR0. This page table is only used until
 * mmu1init() loads m->mmutop.
 */
void
mmuidmap(uintptr *l1bot)
{
	uintptr pa, pe, attr;

	/* VDRAM */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTESH(SHARE_INNER);
	pe = -KZERO;
	for(pa = VDRAM - KZERO; pa < pe; pa += PGLSZ(PTLEVELS-1))
		l1bot[PTLX(pa, PTLEVELS-1)] = pa | PTEVALID | PTEBLOCK | attr;
	return;
}

/*
 * Create initial shared kernel page table (L1) for TTBR1.
 * This page table coveres the INITMAP and VIRTIO,
 * and later we fill the ram mappings in meminit().
 */
void
mmu0init(uintptr *l1)
{
	uintptr va, pa, pe, attr;

	/* DRAM - INITMAP */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTESH(SHARE_INNER);
	pe = INITMAP;
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(1), va += PGLSZ(1)){
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;
		l1[PTL1X(pa, 1)] = pa | PTEVALID | PTEBLOCK | attr;
}

	/* VIRTIO */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTEPXN | PTESH(SHARE_OUTER) | PTEDEVICE;
	pe = PHYSIO + IOSIZE;
	for(pa = PHYSIO, va = VIRTIO+PHYSIO; pa < pe; pa += PGLSZ(1), va += PGLSZ(1)){
		if(((pa|va) & PGLSZ(1)-1) != 0){
			l1[PTL1X(va, 1)] = (uintptr)l1 | PTEVALID | PTETABLE;
			for(; pa < pe && ((va|pa) & PGLSZ(1)-1) != 0; pa += PGLSZ(0), va += PGLSZ(0)){
				assert(l1[PTLX(va, 0)] == 0);
				l1[PTLX(va, 0)] = pa | PTEVALID | PTEPAGE | attr;
			}
			break;
		}
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;
	}

	if(PTLEVELS > 2)
	for(va = KSEG0; va != 0; va += PGLSZ(2))
		l1[PTL1X(va, 2)] = (uintptr)&l1[L1TABLEX(va, 1)] | PTEVALID | PTETABLE;

	if(PTLEVELS > 3)
	for(va = KSEG0; va != 0; va += PGLSZ(3))
		l1[PTL1X(va, 3)] = (uintptr)&l1[L1TABLEX(va, 2)] | PTEVALID | PTETABLE;
	return;






	/* VDRAM */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTESH(SHARE_INNER);
	pe = -KZERO;
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(1), va += PGLSZ(1)){
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;
		l1[PTL1X(pa, 1)] = pa | PTEVALID | PTEBLOCK | attr;
	}

	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTEPXN | PTESH(SHARE_OUTER) | PTEDEVICE;
	pe = PHYSIO + IOSIZE;

	for(pa = PHYSIO, va = VIRTIO + PHYSIO; pa < pe; pa += PGLSZ(1), va += PGLSZ(1)){
		if(((pa|va) & PGLSZ(1)-1) != 0){
			l1[PTL1X(va, 1)] = (uintptr)l1 | PTEVALID | PTETABLE;
			for(; pa < pe && ((va|pa) & PGLSZ(1)-1) != 0; pa += PGLSZ(0), va += PGLSZ(0)){
				assert(l1[PTLX(va, 0)] == 0);
				l1[PTLX(va, 0)] = pa | PTEVALID | PTEPAGE | attr;
			}
			break;
		}
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;
	}

	if(PTLEVELS > 2)
	for(va = KSEG0; va != 0; va += PGLSZ(2))
		l1[PTL1X(va, 2)] = (uintptr)&l1[L1TABLEX(va, 1)] | PTEVALID | PTETABLE;
	if(PTLEVELS > 3)
	for(va = KSEG0; va != 0; va += PGLSZ(3))
		l1[PTL1X(va, 3)] = (uintptr)&l1[L1TABLEX(va, 2)] | PTEVALID | PTETABLE;
}

void
mmu0clear(uintptr *l1)
{
	uintptr va, pa, pe;

	pe = -VDRAM;
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(1), va += PGLSZ(1))
		if(PTL1X(pa, 1) != PTL1X(va, 1))
			l1[PTL1X(pa, 1)] = 0;

	if(PTLEVELS > 2)
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(2), va += PGLSZ(2))
		if(PTL1X(pa, 2) != PTL1X(va, 2))
			l1[PTL1X(pa, 2)] = 0;
	if(PTLEVELS > 3)
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(3), va += PGLSZ(3))
		if(PTL1X(pa, 3) != PTL1X(va, 3))
			l1[PTL1X(pa, 3)] = 0;
}

void
meminit(void)
{
	uintptr va, pa;
	int i;

	conf.mem[0].base = PGROUND((uintptr)end - KZERO);
	conf.mem[0].limit = UCRAMBASE;
	conf.mem[1].base = UCRAMBASE + UCRAMSIZE;
	conf.mem[1].limit = PHYSDRAM+DRAMSIZE;

	/*
	 * now we know the real memory regions, unmap
	 * everything above INITMAP and map again with
	 * the proper sizes.
	 */
	coherence();
	for(va = INITMAP+KZERO; va != 0; va += PGLSZ(1)){
		pa = va-KZERO;
		((uintptr*)L1)[PTL1X(pa, 1)] = 0;
		((uintptr*)L1)[PTL1X(va, 1)] = 0;
	}
	flushtlb();

	pa = PGROUND((uintptr)end)-KZERO;
	for(i=0; i<nelem(conf.mem); i++){
		if(conf.mem[i].limit >= KMAPEND-KMAP)
			conf.mem[i].limit = KMAPEND-KMAP;

		if(conf.mem[i].limit <= conf.mem[i].base){
			conf.mem[i].limit = conf.mem[i].base = 0;
			continue;
		}

		if(conf.mem[i].base < PHYSDRAM + DRAMSIZE
		&& conf.mem[i].limit > PHYSDRAM + DRAMSIZE)
			conf.mem[i].limit = PHYSDRAM + DRAMSIZE;

		/* take kernel out of allocatable space */
		if(pa > conf.mem[i].base && pa < conf.mem[i].limit)
			conf.mem[i].base = pa;

		kmapram(conf.mem[i].base, conf.mem[i].limit);
	}
	flushtlb();

	/* rampage() is now done, count up the pages for each bank */
	for(i=0; i<nelem(conf.mem); i++)
		conf.mem[i].npage = (conf.mem[i].limit - conf.mem[i].base)/BY2PG;


}


static void*
ucramalloc(usize size, uintptr align, uint attr)
{
	static uintptr top = UCRAMBASE + UCRAMSIZE;
	static Lock lk;
	uintptr va, pg;

	lock(&lk);
	top -= size;
	size += top & align-1;
	top &= -align;
	if(top < UCRAMBASE)
		panic("ucramalloc: need %zd bytes", size);
	va = KZERO + top;
	pg = va & -BY2PG;
	if(pg != ((va+size) & -BY2PG))
		mmukmap(pg | attr, pg - KZERO, PGROUND(size));
	unlock(&lk);

	return (void*)va;
}

void*
ucalloc(usize size)
{
	return ucramalloc(size, 8, PTEUNCACHED);
}