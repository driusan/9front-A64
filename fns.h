#include "../port/portfns.h"

/* l.s */
extern void sev(void);
extern int tas(void *);
extern int cmpswap(long*, long, long);
extern void coherence(void);
extern void idlehands(void);
extern uvlong vcycles(void);
#define cycles(ip) *(ip) = vcycles()
extern int splfhi(void);
extern void splflo(void);
extern void touser(uintptr sp);
extern void forkret(void);
extern void noteret(void);
extern void returnto(void*);
extern void fpsaveregs(void*);
extern void fploadregs(void*);
extern void smccall(Ureg*);

extern void zoot(void);
extern void voot(void);

extern void setttbr(uintptr pa);
extern uintptr getfar(void);

extern void flushasidva(uintptr asidva);
extern void tlbivae1is(uintptr asidva);

extern void flushasidvall(uintptr asidva);
extern void tlbivale1is(uintptr asidva);

extern void flushasid(uintptr asid);
extern void tlbiaside1is(uintptr asid);

extern void flushtlb(void);
extern void tlbivmalle1(void);

extern void flushlocaltlb(void);
extern void tlbivmalle1(void);

/* cache */
extern ulong cachesize(int level);

extern void cacheiinvse(void*, int);
extern void cacheuwbinv(void);
extern void cacheiinv(void);

extern void cachedwbse(void*, int);
extern void cacheduwbse(void*, int);
extern void cachedinvse(void*, int);
extern void cachedwbinvse(void*, int);

extern void cachedwb(void);
extern void cachedinv(void);
extern void cachedwbinv(void);

extern void l2cacheuwb(void);
extern void l2cacheuinv(void);
extern void l2cacheuwbinv(void);

/* mmu */
#define	getpgcolor(a)	0
extern uintptr paddr(void*);
#define PADDR(a) paddr((void*)(a))
extern uintptr cankaddr(uintptr);
extern void* kaddr(uintptr);
#define KADDR(a) kaddr(a)
extern void kmapinval(void);
#define	VA(k)	((uintptr)(k))
extern KMap *kmap(Page*);
extern void kunmap(KMap*);
extern void kmapram(uintptr, uintptr);
extern uintptr mmukmap(uintptr, uintptr, usize);
extern void* vmap(uvlong, vlong);
extern void vunmap(void*, vlong);

extern void mmu0init(uintptr*);
extern void mmu0clear(uintptr*);
extern void mmuidmap(uintptr*);
extern void mmu1init(void);
extern void meminit(void);

extern void putasid(Proc*);
extern void* ucalloc(usize);


/* clock */
extern void clockinit(void);
extern void synccycles(void);
extern void armtimerset(int);
extern void clockshutdown(void);

/* fpu */
extern void fpuinit(void);
extern void fpon(void);
extern void fpoff(void);
// extern void fpinit(void);
extern void fpclear(void);
// extern void fpsave(FPsave*);
// extern void fprestore(FPsave*);
extern void fpukenter(Ureg*);
extern void fpukexit(Ureg*);
extern void fpuprocsave(Proc*);
extern void fpuprocfork(Proc*);
extern void fpuprocsetup(Proc*);
extern void fpuprocrestore(Proc*);
extern void fpunotify(Proc*);
extern void fpunoted(Proc*);
extern void mathtrap(Ureg*);

/* trap */
extern void trapinit(void);
extern int userureg(Ureg*);
extern void evenaddr(uintptr);
extern void setkernur(Ureg*, Proc*);
extern void procfork(Proc*);
extern void procsetup(Proc*);
extern void procsave(Proc*);
extern void procrestore(Proc *);
extern void trap(Ureg*);
extern void syscall(Ureg*);
extern void faultarm64(Ureg*);
extern void dumpstack(void);
extern void dumpregs(Ureg*);

/* irq */
extern void intrcpushutdown(void);
extern void intrsoff(void);
extern void intrenable(int, void (*)(Ureg*, void*), void*, int, char*);
extern void intrdisable(int, void (*)(Ureg*, void*), void*, int, char*);
extern int irq(Ureg*);
extern void fiq(Ureg*);
extern void intrinit(void);

extern int isintrenable(int);
extern int isintrpending(int);
extern int isintractive(int);


/* sysreg */
extern uvlong	sysrd(ulong);
extern void	syswr(ulong, uvlong);

/* main */
extern char *getconf(char *name);
extern void setconfenv(void);
extern void writeconf(void);
extern int isaconfig(char*, int, ISAConf*);
extern void links(void);
extern void dmaflush(int, void*, ulong);


/* uart */
extern void _uartputs(char*, int);
extern void uartconsinit(void);
// extern int i8250console(void);


/* devarch */
extern uint getkeyadc(void);
extern char* getkeyadc_event(void);
extern void keyadcinit(void);
extern Dirtab* addarchfile(char *name, int perm, Rdwrfn *rdfn, Rdwrfn *wrfn);

/* ccu */
extern char* listgates(int);
extern void	debuggates(void);
extern char* getgatename(int);
extern int getgatestate(int);
extern char* getresetname(int);
extern int getresetstate(int);
extern u32int getcpuclk_n(void);
extern u32int getcpuclk_k(void);
extern u32int getcpuclk_m(void);
extern u32int getcpuclk_p(void);
extern int setcpuclk(uint);
extern int setcpuclk_n(u32int);
extern int openthegate(char*);
extern int closethegate(char*);
extern ulong getclkrate(int clkid);
extern void setclkrate(int clkid, ulong hz);
extern void clkenable(int clkid);
extern void clkdisable(int clkid);

extern void turnonths(void);	//needs to go

/* thermal */
extern void thermalinit(void);
extern int gettemp0(void);
extern int gettemp1(void);
extern int gettemp2(void);

/* rsb */
extern u32int rsb_read(u8int, u16int, u8int, uint);
extern u32int rsb_write(u8int, u16int, u8int, u32int, uint);
extern void rsbinit(void);

/* axp803 */
extern u8int pmic_id(void);
extern int pmic_acin(void);
extern int pmic_vbat(void);
extern char* getpmicname(int);
extern int getpmicstate(int);
extern int getpmicvolt(int);
extern int setpmicstate(char*, int);
extern int setpmicvolt(char*, int);

/* archA64 */
extern void arch_rsbsetup(void);

/* display */
extern void deinit(void);

/* lcd */
extern void lcdinit(void);

/* backlight */
extern void backlightinit(void);
extern void backlight(int);

/* modem */
extern void modeminit(void);

/* pio */
extern int piocfg(char *name, int val);
extern int pioset(char *name, int on);
extern int pioget(char *name);
extern void pioeintcfg(char *name, int val);

/* touch */
void touchwait(void);
