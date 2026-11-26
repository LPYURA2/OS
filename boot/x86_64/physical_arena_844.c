#include "efi.h"
#include <styleos/arch.h>
#include <styleos/paging.h>
#include <styleos/physical_arena_844.h>
static struct boot_range reserved844;
static bool admitted844;
static struct s844_arena arena844;
#if defined(STYLEOS_TEST_TASK_STACKS_845) || defined(STYLEOS_TEST_USER_ROOTS_846) || (defined(STYLEOS_TEST_RUNTIME_ARENA_858) || defined(STYLEOS_OWNED_SHELL_865))
/* Query is read-only before own CR3. Return the allocator only after the
 * existing live test has completed and the reserved area is empty. */
bool s845_extent(struct boot_range *out){
 if(!out||!admitted844||arena844.ready)return false;
 *out=reserved844;return true;
}
struct s844_arena *s845_arena(void){
 unsigned used;
 return admitted844&&s844_count(&arena844,&used)&&!used?&arena844:0;
}
#endif
static void fld(const char *key,uint64_t v){arch_putc(' ');arch_puts(key);arch_putc('=');arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
#ifndef STYLEOS_OWNED_SHELL_865
static _Noreturn void fail(const char *s){arch_puts("STYLEOS S844 FAIL ");arch_puts(s);arch_putc('\n');
#if defined(STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW) || defined(STYLEOS_PROVISIONED_STORAGE_864)
 __asm__ volatile("cli":::"memory");
#else
 __asm__ volatile("cli;outl %0,%1"::"a"(0x24u),"Nd"((uint16_t)0xf4));
#endif
arch_halt();}
#endif
typedef EFI_STATUS (EFIAPI *alloc_pages844)(uint32_t,uint32_t,UINTN,uint64_t *);
EFI_STATUS s844_reserve(struct efi_boot_services *bs){
 if(!bs||!bs->allocate_pages||reserved844.start)return EFI_INVALID_PARAMETER;
 uint64_t address=0x07ffffffu;
 EFI_STATUS status=((alloc_pages844)bs->allocate_pages)(1,EFI_LOADER_DATA,S844_PAGES,&address);
 if(status!=EFI_SUCCESS)return status;
 if(address<0x100000u||(address&4095)||address>0x08000000u-S844_BYTES)return EFI_ABORTED;
 reserved844=(struct boot_range){address,address+S844_BYTES};
 arch_puts("STYLEOS S844 RESERVE");fld("base",address);fld("pages",S844_PAGES);arch_putc('\n');
 return EFI_SUCCESS;
}
bool s844_admit_efi(const struct boot_memory_evidence *e,const struct boot_info *b,struct boot_range *out){
 struct boot_range desc;uint64_t attr;
 if(!out||admitted844||!s844_efi_proof(e,b,reserved844,&desc,&attr))return false;
 admitted844=true;*out=reserved844;
 arch_puts("STYLEOS S844 ADMIT");fld("base",reserved844.start);fld("end",reserved844.end);
 fld("desc-start",desc.start);fld("desc-end",desc.end);fld("type",2);fld("attr",attr);arch_putc('\n');
 return true;
}
static bool is_zero(struct s844_lease v){
 const volatile unsigned char *p=(const volatile unsigned char *)(uintptr_t)v.pa;
 for(size_t i=0;i<(size_t)v.pages*BOOT_MEMORY_PAGE;i++)if(p[i])return false;
 return true;
}
static void fill(struct s844_lease v,unsigned char byte){
 volatile unsigned char *p=(volatile unsigned char *)(uintptr_t)v.pa;
 for(size_t i=0;i<(size_t)v.pages*BOOT_MEMORY_PAGE;i++)p[i]=byte;
}
#ifndef STYLEOS_OWNED_SHELL_865
static bool matches(struct s844_lease v,unsigned char byte){
 const volatile unsigned char *p=(const volatile unsigned char *)(uintptr_t)v.pa;
 for(size_t i=0;i<(size_t)v.pages*BOOT_MEMORY_PAGE;i++)if(p[i]!=byte)return false;
 return true;
}
#endif
#if (defined(STYLEOS_TEST_DESKTOP_COLD_863) || defined(STYLEOS_OWNED_SHELL_865))
/* The caller already audited the entire reserved range under the live root.
 * Cold startup initializes metadata and scrubs RAM without fake leases. */
bool s844_runtime_start(uint64_t expected_root){
 uint64_t root,flags;
 __asm__ volatile("mov %%cr3,%0;pushfq;popq %1":"=r"(root),"=r"(flags));
 if(!admitted844||!expected_root||root!=expected_root||(flags&0x200)||
    !s844_init(&arena844,reserved844.start))return false;
 struct s844_lease whole={.pa=reserved844.start,.pages=S844_PAGES};
 fill(whole,0);
 unsigned count;
 return is_zero(whole)&&s844_count(&arena844,&count)&&count==0&&arena844.next==1;
}
#endif
/* Called only after S842 full detached-root audit and guarded transition.
 * No inherited translation is used to read or write the allocated pages. */
#ifndef STYLEOS_OWNED_SHELL_865
void s844_guest_run(uint64_t expected_root){
 uint64_t cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));
 if(!admitted844||cr3!=expected_root||!s844_init(&arena844,reserved844.start))fail("admission");
 struct s844_lease a,b,c,extra={0};unsigned count;
 if(!s844_acquire(&arena844,1,4,&a)||!s844_acquire(&arena844,2,12,&b)||
 !is_zero(a)||!is_zero(b)||!s844_count(&arena844,&count)||count!=16||
 s844_acquire(&arena844,3,1,&extra))fail("allocation");
 fill(a,0xa5);fill(b,0x5a);
 if(!matches(a,0xa5)||!matches(b,0x5a)||s844_release(&arena844,2,a)||
 !matches(a,0xa5)||!s844_release(&arena844,1,a)||!is_zero(a)||
 s844_release(&arena844,1,a)||!matches(b,0x5a)||
 !s844_acquire(&arena844,1,4,&c)||c.pa!=a.pa||c.generation==a.generation||!is_zero(c)||
 s844_release(&arena844,1,a)||!matches(b,0x5a))fail("reuse-or-ownership");
 fill(c,0x3c);
 if(!matches(c,0x3c)||!s844_release(&arena844,1,c)||!is_zero(c)||
 !s844_release(&arena844,2,b)||!is_zero(b)||!s844_count(&arena844,&count)||count!=0)fail("release");
 arch_puts("STYLEOS S844 LIVE");fld("root",cr3);fld("a",a.pa);fld("b",b.pa);fld("reuse",c.pa);
 fld("gen-a",a.generation);fld("gen-b",b.generation);fld("gen-reuse",c.generation);
 fld("used",count);fld("zero",1);fld("refusals",4);arch_putc('\n');
 arch_puts("STYLEOS S844 POLICY bounded-efi-arena=yes candidate-grants=no reclaim=no dma-grants=no production-cr3=deny\n");
}

#endif
