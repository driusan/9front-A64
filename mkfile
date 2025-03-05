CONF=pine64
CONFLIST=pine64

loadaddr=0xFFFFFFFFC0100000
kzero=0xffffffff80000000

objtype=arm64
</$objtype/mkfile
p=9

DEVS=`{rc ../port/mkdevlist $CONF}

PORT=\
	alarm.$O\
	alloc.$O\
	allocb.$O\
	auth.$O\
	cache.$O\
	chan.$O\
	dev.$O\
	edf.$O\
	fault.$O\
	mul64fract.$O\
	page.$O\
	parse.$O\
	pgrp.$O\
	portclock.$O\
	print.$O\
	proc.$O\
	qio.$O\
	qlock.$O\
	rebootcmd.$O\
	rdb.$O\
	segment.$O\
	syscallfmt.$O\
	sysfile.$O\
	sysproc.$O\
	taslock.$O\
	tod.$O\
	xalloc.$O\
	userinit.$O\

OBJ=\
	l.$O\
	archA64.$O\
	cache.v8.$O\
	clock.$O\
	fpu.$O\
	gic.$O\
	main.$O\
	mmu.$O\
	mem.$O\
	keyadc.$O\
	sysreg.$O\
	random.$O\
	trap.$O\
	$CONF.root.$O\
	$CONF.rootc.$O\
	$DEVS\
	$PORT\

# HFILES=

LIB=\
	/$objtype/lib/libmemlayer.a\
	/$objtype/lib/libmemdraw.a\
	/$objtype/lib/libdraw.a\
	/$objtype/lib/libip.a\
	/$objtype/lib/libsec.a\
	/$objtype/lib/libmp.a\
	/$objtype/lib/libc.a\
#	/$objtype/lib/libdtracy.a\


9:V:	$p$CONF

$p$CONF.u:D:	$p$CONF
	aux/aout2uimage -Z $kzero $p$CONF

$p$CONF:D:	$OBJ $CONF.$O $LIB
	$LD -a -o $target -H6 -R0x10000 -T$loadaddr -l $prereq >DEBUG

#$p$CONF:D:	$OBJ $CONF.$O $LIB
#	$LD -o $target -t$loadaddr -l $prereq

$OBJ: $HFILES

install:V: /$objtype/$p$CONF

/$objtype/$p$CONF:D: $p$CONF
	cp -x $p$CONF /$objtype/

ARM64FILES=`{../port/mkfilelist ../arm64}
^($ARM64FILES)\.$O:R:	'../arm64/\1.c'
	$CC $CFLAGS -I. -. ../arm64/$stem1.c

cache.v8.$O:	../arm64/cache.v8.s
	$AS $AFLAGS -I. -. ../arm64/cache.v8.s
init9.$O:	../arm64/init9.s
	$AS $AFLAGS -I. -. ../arm64/init9.s
rebootcode.$O:	../arm64/rebootcode.s
	$AS $AFLAGS -I. -. ../arm64/rebootcode.s

<../boot/bootmkfile
<../port/portmkfile
<|../port/mkbootrules $CONF

main.$O: rebootcode.i
cache.v8.$O: ../arm64/sysreg.h

#mmu.$O: /$objtype/include/ureg.h
#l.$O mmu.$O: mem.h
#l.$O mmu.$O: sysreg.h

devusb.$O:	../port/usb.h
usbehci.$O usbohci.$O usbuhci.$O: ../port/usb.h usbehci.h

initcode.out:		init9.$O initcode.$O /$objtype/lib/libc.a
	$LD -l -R1 -s -o $target $prereq

rebootcode.out:		rebootcode.$O cache.v8.$O
	$LD -l -H6 -R1 -T0x40020000 -s -o $target $prereq

$CONF.clean:
	rm -rf $p$CONF s$p$CONF $p$CONF.u errstr.h $CONF.c boot$CONF.c
