/* StyleOS S823 firmware AML in an isolated QEMU test only.
 * Real UEFI DSDT/SSDT are checksummed and copied BEFORE ExitBootServices.
 * Hardware OperationRegions, MMIO, PCI I/O and DMA remain deny-all.
 * NOT production ACPICA, NOT safe for untrusted physical hardware.
 */
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <styleos/acpi_firmware_823.h>
#include <styleos/acpica_efi_prelude_821.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <acpi.h>

#define HEAP_822 (16u*1024u*1024u)
static _Alignas(16) unsigned char heap[HEAP_822];
static size_t used;
static unsigned allocated, maps, denied_maps, denied_hw;
static unsigned long long ticks;
static unsigned char rsdp[36] __attribute__((aligned(16)));
static unsigned char xsdt[36u+8u*(1u+STYLEOS823_MAX_SSDT)] __attribute__((aligned(16)));
static unsigned char fadt[276] __attribute__((aligned(16)));
static struct styleos823_firmware fw;
struct table822 {unsigned char *data;unsigned size;};
static struct table822 tab[4u+STYLEOS823_MAX_SSDT];
static unsigned tab_count;
static int ready;

static void put32(unsigned char *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8u*i));}
static void put64(unsigned char *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(8u*i));}
static void zero(unsigned char *p,size_t n){for(size_t i=0;i<n;i++)p[i]=0;}
static void sum(unsigned char *p,unsigned n,unsigned index){
 unsigned v=0;p[index]=0;for(unsigned i=0;i<n;i++)v+=p[i];p[index]=(unsigned char)(0u-v);
}
static void head(unsigned char *p,const char *sig,unsigned size){
 zero(p,size);for(unsigned i=0;i<4;i++)p[i]=(unsigned char)sig[i];put32(p+4,size);
 p[8]=2;for(unsigned i=0;i<6;i++)p[10+i]="STYLOS"[i];
 for(unsigned i=0;i<8;i++)p[16+i]="TEST0822"[i];
 put32(p+24,1);put32(p+28,0x534f5354u);put32(p+32,822);
}
static int build_tables(void){
 if(!styleos823_get_firmware(&fw)||!fw.dsdt||fw.dsdt_len<37u||
    fw.ssdt_count>STYLEOS823_MAX_SSDT||!fw.root_count||!fw.mcfg_len)return 0;
 /* The FADT/XSDT/RSDP are local, with hardware-reduced ACPI selected.
  * Raw firmware DSDT+SSDT remain byte-for-byte image-owned snapshots. */
 unsigned xlen=44u+8u*fw.ssdt_count;
 head(fadt,"FACP",276);put64(fadt+140,(uint64_t)(uintptr_t)fw.dsdt);
 put32(fadt+112,1u<<20);sum(fadt,276,9);
 head(xsdt,"XSDT",xlen);put64(xsdt+36,(uint64_t)(uintptr_t)fadt);
 for(unsigned i=0;i<fw.ssdt_count;i++)
  put64(xsdt+44u+8u*i,(uint64_t)(uintptr_t)fw.ssdt[i]);
 sum(xsdt,xlen,9);
 zero(rsdp,36);
 for(unsigned i=0;i<8;i++)rsdp[i]="RSD PTR "[i];
 for(unsigned i=0;i<6;i++)rsdp[9+i]="STYLOS"[i];
 rsdp[15]=2;put32(rsdp+20,36);
 put64(rsdp+24,(uint64_t)(uintptr_t)xsdt);
 sum(rsdp,20,8);sum(rsdp,36,32);
 tab[0]=(struct table822){rsdp,36};tab[1]=(struct table822){xsdt,xlen};
 tab[2]=(struct table822){fadt,276};tab[3]=(struct table822){(unsigned char*)fw.dsdt,fw.dsdt_len};
 for(unsigned i=0;i<fw.ssdt_count;i++)
  tab[4+i]=(struct table822){(unsigned char*)fw.ssdt[i],fw.ssdt_len[i]};
 tab_count=4u+fw.ssdt_count;ready=1;
 return 1;
}
static void *take(size_t n){
 if(!n||n>HEAP_822||allocated>=65536u)return 0;
 n=(n+15u)&~15u;if(n>HEAP_822-used)return 0;
 void *p=heap+used;used+=n;allocated++;zero(p,n);return p;
}
ACPI_STATUS AcpiOsInitialize(void){return AE_OK;}
ACPI_STATUS AcpiOsTerminate(void){return AE_OK;}
void *AcpiOsAllocate(ACPI_SIZE n){return take((size_t)n);}
/* Upstream ACPICA core provides AcpiOsAllocateZeroed; do not shadow it. */
void AcpiOsFree(void *p){(void)p;}
ACPI_STATUS AcpiOsCreateCache(char *name,UINT16 size,UINT16 depth,ACPI_CACHE_T **out){
 (void)name;(void)depth;if(!out||!size)return AE_BAD_PARAMETER;
 size_t *p=take(sizeof(size_t));if(!p)return AE_NO_MEMORY;
 *p=size;*out=(ACPI_CACHE_T *)p;return AE_OK;
}
ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *p){return p?AE_OK:AE_BAD_PARAMETER;}
ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *p){return p?AE_OK:AE_BAD_PARAMETER;}
void *AcpiOsAcquireObject(ACPI_CACHE_T *p){return p?take(*(size_t *)p):0;}
ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T *p,void *x){return p&&x?AE_OK:AE_BAD_PARAMETER;}
struct sem822{unsigned max,count;};
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max,UINT32 initial,ACPI_SEMAPHORE *out){
 if(!out||!max||initial>max)return AE_BAD_PARAMETER;
 struct sem822 *p=take(sizeof *p);if(!p)return AE_NO_MEMORY;
 p->max=max;p->count=initial;*out=(ACPI_SEMAPHORE)p;return AE_OK;
}
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE p){return p?AE_OK:AE_BAD_PARAMETER;}
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE p,UINT32 n,UINT16 timeout){
 (void)timeout;if(!p||!n)return AE_BAD_PARAMETER;
 struct sem822 *s=(struct sem822 *)p;
 if(s->count<n)return AE_TIME;s->count-=n;return AE_OK;
}
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE p,UINT32 n){
 if(!p||!n)return AE_BAD_PARAMETER;
 struct sem822 *s=(struct sem822 *)p;
 if(n>s->max-s->count)return AE_LIMIT;s->count+=n;return AE_OK;
}
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *p){
 if(!p)return AE_BAD_PARAMETER;*p=(ACPI_SPINLOCK)take(16);
 return *p?AE_OK:AE_NO_MEMORY;
}
void AcpiOsDeleteLock(ACPI_SPINLOCK p){(void)p;}
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK p){(void)p;return 0;}
void AcpiOsReleaseLock(ACPI_SPINLOCK p,ACPI_CPU_FLAGS flags){(void)p;(void)flags;}
ACPI_THREAD_ID AcpiOsGetThreadId(void){return 1;}
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *p,ACPI_STRING *v){
 (void)p;if(!v)return AE_BAD_PARAMETER;*v=0;return AE_OK;
}
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *p,ACPI_TABLE_HEADER **out){
 (void)p;if(!out)return AE_BAD_PARAMETER;*out=0;return AE_OK;
}
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *p,ACPI_PHYSICAL_ADDRESS *a,UINT32 *n){
 (void)p;if(!a||!n)return AE_BAD_PARAMETER;*a=0;*n=0;return AE_OK;
}
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void){
 return ready?(ACPI_PHYSICAL_ADDRESS)(uintptr_t)rsdp:0;
}
void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS address,ACPI_SIZE size){
 if(!ready||!size||size>65536u||address>UINT64_MAX-size){denied_maps++;return 0;}
 for(unsigned i=0;i<tab_count;i++){
  uint64_t b=(uintptr_t)tab[i].data;
  if(address>=b&&address-b<=tab[i].size&&size<=tab[i].size-(address-b)){
   maps++;return tab[i].data+(size_t)(address-b);
  }
 }
 denied_maps++;return 0;
}
void AcpiOsUnmapMemory(void *p,ACPI_SIZE n){(void)p;(void)n;}
ACPI_STATUS AcpiOsGetPhysicalAddress(void *p,ACPI_PHYSICAL_ADDRESS *out){
 if(!p||!out)return AE_BAD_PARAMETER;
 for(unsigned i=0;i<tab_count;i++){
  uintptr_t b=(uintptr_t)tab[i].data,v=(uintptr_t)p;
  if(v>=b&&v-b<tab[i].size){*out=v;return AE_OK;}
 }
 return AE_ACCESS;
}
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS a,UINT64 *v,UINT32 w){
 (void)a;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS a,UINT64 v,UINT32 w){
 (void)a;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS a,UINT32 *v,UINT32 w){
 (void)a;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS a,UINT32 v,UINT32 w){
 (void)a;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *p,UINT32 reg,UINT64 *v,UINT32 w){
 (void)p;(void)reg;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *p,UINT32 reg,UINT64 v,UINT32 w){
 (void)p;(void)reg;(void)v;(void)w;denied_hw++;return AE_ACCESS;
}
UINT64 AcpiOsGetTimer(void){return ++ticks;}
void AcpiOsSleep(UINT64 ms){(void)ms;}
void AcpiOsStall(UINT32 us){(void)us;}
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq,ACPI_OSD_HANDLER f,void *p){
 (void)irq;(void)f;(void)p;return AE_ACCESS;
}
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 irq,ACPI_OSD_HANDLER f){
 (void)irq;(void)f;return AE_ACCESS;
}
ACPI_STATUS AcpiOsSignal(UINT32 f,void *p){(void)f;(void)p;return AE_SUPPORT;}
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE t,ACPI_OSD_EXEC_CALLBACK f,void *p){
 (void)t;(void)f;(void)p;return AE_SUPPORT;
}
void AcpiOsWaitEventsComplete(void){}
ACPI_STATUS AcpiOsEnterSleep(UINT8 x,UINT32 a,UINT32 b){(void)x;(void)a;(void)b;return AE_ACCESS;}
BOOLEAN AcpiOsReadable(void *p,ACPI_SIZE n){return p!=0&&n!=0;}
BOOLEAN AcpiOsWritable(void *p,ACPI_SIZE n){return p!=0&&n!=0;}
int styleos819_dma_grant(uint16_t requester){(void)requester;return 0;}

static void hex(uint64_t v){
 arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);
}
static _Noreturn void fail(const char *stage,ACPI_STATUS result){
 arch_puts("STYLEOS S823 FAIL ");arch_puts(stage);arch_putc(' ');hex(result);
 arch_puts(" alloc-count=");hex(allocated);
 arch_puts(" heap-bytes=");hex(used);
 arch_puts(" hardware-denials=");hex(denied_hw);
 arch_putc('\n');arch_halt();
}
/* Never evaluate an arbitrary path. Only specific PCI0 identity and resource
 * objects are eligible. Methods requiring SystemIO/PCI OperationRegion fail. */
static unsigned eval_number(const char *name,uint64_t *out){
 ACPI_OBJECT obj;ACPI_BUFFER b={sizeof obj,&obj};
 ACPI_STATUS s=AcpiEvaluateObject(NULL,(ACPI_STRING)name,NULL,&b);
 if(s==AE_NOT_FOUND)return 0;
 if(s!=AE_OK)fail("eval-number",s);
 if(obj.Type!=ACPI_TYPE_INTEGER)fail("number-type",AE_TYPE);
 *out=obj.Integer.Value;return 1;
}
static unsigned eval_crs(void){
 _Alignas(16) unsigned char buf[2048]={0};ACPI_BUFFER b={sizeof buf,buf};
 ACPI_STATUS s=AcpiEvaluateObject(NULL,(ACPI_STRING)"\\_SB.PCI0._CRS",NULL,&b);
 if(s!=AE_OK)fail("eval-crs",s);
 ACPI_OBJECT *o=(ACPI_OBJECT *)(void *)buf;
 if(o->Type!=ACPI_TYPE_BUFFER||!o->Buffer.Pointer||
    o->Buffer.Length<4||o->Buffer.Length>1024)fail("crs-type",AE_TYPE);
 uintptr_t p=(uintptr_t)o->Buffer.Pointer,v=(uintptr_t)buf;
 if(p<v||p-v>sizeof buf||o->Buffer.Length>sizeof buf-(p-v))
  fail("crs-bounds",AE_ACCESS);
#ifdef STYLEOS_TEST_ACPI_RESOURCES_824
 /* Capture the actual returned buffer, before the local stack buffer expires. */
 arch_puts("STYLEOS S824 CRS data=");
 for(unsigned i=0;i<o->Buffer.Length;i++){
  unsigned x=o->Buffer.Pointer[i];
  arch_putc("0123456789abcdef"[x>>4]);arch_putc("0123456789abcdef"[x&15u]);
 }
 arch_putc('\n');
#endif
 /* A resource buffer is evidence only, NEVER permission for ECAM/MMIO/DMA. */
 return o->Buffer.Length;
}
void acpi823_terminal(const struct boot_info *boot){
 if(!boot||boot->magic!=STYLEOS_BOOT_MAGIC||
    boot->flags!=STYLEOS_BOOT_EXITED_SERVICES)fail("handoff",AE_BAD_PARAMETER);
 if(!build_tables())fail("snapshot",AE_BAD_PARAMETER);
 ACPI_STATUS s=AcpiInitializeSubsystem();if(s!=AE_OK)fail("init-subsystem",s);
 s=AcpiInitializeTables(NULL,16,TRUE);if(s!=AE_OK)fail("init-tables",s);
 /* Bound AML While loops before any firmware namespace load. */
 extern UINT32 AcpiGbl_MaxLoopIterations;
 AcpiGbl_MaxLoopIterations=64u;
 s=AcpiLoadTables();if(s!=AE_OK)fail("load-tables",s);
 uint64_t hid=0,seg=0,bbn=0;
 unsigned hidp=eval_number("\\_SB.PCI0._HID",&hid);
 unsigned segp=eval_number("\\_SB.PCI0._SEG",&seg);
 unsigned bbnp=eval_number("\\_SB.PCI0._BBN",&bbn);
 if(!hidp||(hid!=UINT64_C(0x080ad041)&&hid!=UINT64_C(0x030ad041)))
  fail("hid",AE_TYPE);
 if((segp&&seg)||(bbnp&&bbn))fail("segment-or-bus",AE_ACCESS);
 unsigned crs=eval_crs();
 if(denied_hw||!maps||styleos819_dma_grant(0))fail("policy",AE_ACCESS);
 UINT64 sentinel=UINT64_MAX;
 if(AcpiOsReadMemory(0xe0000000u,&sentinel,32)!=AE_ACCESS||sentinel!=UINT64_MAX)
  fail("mmio-deny",AE_ACCESS);
 arch_puts("STYLEOS S823 FIRMWARE dsdt-bytes=");hex(fw.dsdt_len);
 arch_puts(" ssdt-count=");hex(fw.ssdt_count);
 arch_puts(" image-owned-snapshot=yes original-aml=yes\n");
 arch_puts("STYLEOS S823 HOSTBRIDGE hid=");hex(hid);
 arch_puts(" crs-bytes=");hex(crs);
 arch_puts(" seg-present=");hex(segp);
 arch_puts(" bbn-present=");hex(bbnp);
 arch_puts(" real-AcpiEvaluateObject=yes\n");
 arch_puts("STYLEOS S823 POLICY firmware-methods=limited region=deny-all pci=deny-all ");
 arch_puts("dma-grants=no ecam-reservation=no production-cr3=deny foundation-complete=no\n");
 arch_puts("STYLEOS S823 PASS\n");
}
