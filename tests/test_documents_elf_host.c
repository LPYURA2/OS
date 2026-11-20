/* Linux host-only ELF smoke test. This DOES NOT validate StyleOS 744 ABI, boot,
 * syscall isolation, page-table ownership or QEMU guest execution. */
#define _GNU_SOURCE
#include "styleos/documents_runtime.h"
#include <elf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
static char storage[16384];static size_t stored;
static unsigned frames;
static sdm_document scratch_doc;
static int rd(void *ctx,sda_file_token token,uint8_t *out,size_t cap,size_t *length){
 (void)ctx;if(token!=17||stored>cap)return -1;
 memcpy(out,storage,stored);*length=stored;return 0;
}
static int wr(void *ctx,sda_file_token token,const uint8_t *bytes,size_t n){
 (void)ctx;if(token!=17||n>sizeof(storage))return -1;
 memcpy(storage,bytes,n);stored=n;return 0;
}
static int present(void *ctx,const sdu_draw *draw,size_t n){
 (void)ctx;if(n<20||!draw)return -1;frames++;return 0;
}
static int dialog(void *ctx,int save){(void)ctx;(void)save;return 0;}
static void die(const char *what){perror(what);exit(1);}
static uintptr_t find_symbol(const unsigned char *data,size_t len,const char *name){
 const Elf64_Ehdr *h=(const Elf64_Ehdr *)data;
 if(h->e_shoff+h->e_shnum*sizeof(Elf64_Shdr)>len)exit(3);
 const Elf64_Shdr *sections=(const Elf64_Shdr *)(data+h->e_shoff);
 for(unsigned i=0;i<h->e_shnum;i++){
  if(sections[i].sh_type!=SHT_SYMTAB || sections[i].sh_entsize!=sizeof(Elf64_Sym))continue;
  const Elf64_Shdr *s=sections+i;
  if(s->sh_link>=h->e_shnum || s->sh_offset+s->sh_size>len)exit(4);
  const Elf64_Shdr *strsec=&sections[s->sh_link];
  if(strsec->sh_offset+strsec->sh_size>len)exit(5);
  const char *names=(const char*)(data+strsec->sh_offset);
  const Elf64_Sym *syms=(const Elf64_Sym *)(data+s->sh_offset);
  for(size_t j=0;j<s->sh_size/sizeof(Elf64_Sym);j++){
   if(syms[j].st_name>=strsec->sh_size)exit(6);
   if(strcmp(names+syms[j].st_name,name)==0)return syms[j].st_value;
  }
 }
 return 0;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)die("open");
 if(fseek(f,0,SEEK_END))die("seek");
 long n=ftell(f);
 if(n<256||n>65536)exit(7);
 rewind(f);unsigned char *elf=malloc((size_t)n);if(!elf)die("memory");
 if(fread(elf,1,(size_t)n,f)!=(size_t)n)die("read");
 fclose(f);
 Elf64_Ehdr *h=(Elf64_Ehdr *)elf;
 assert(!memcmp(h->e_ident,ELFMAG,SELFMAG)&&h->e_type==ET_EXEC&&h->e_machine==EM_X86_64);
 assert(h->e_phnum<16&&h->e_phoff+h->e_phnum*sizeof(Elf64_Phdr)<=(size_t)n);
 Elf64_Phdr *ph=(Elf64_Phdr*)(elf+h->e_phoff);
 size_t page=(size_t)sysconf(_SC_PAGESIZE);
 for(unsigned i=0;i<h->e_phnum;i++){
   if(ph[i].p_type!=PT_LOAD)continue;
   assert(ph[i].p_filesz<=ph[i].p_memsz&&ph[i].p_offset+ph[i].p_filesz<=(size_t)n);
   uintptr_t start=ph[i].p_vaddr & ~(page-1);
   uintptr_t end=(ph[i].p_vaddr+ph[i].p_memsz+page-1)&~(page-1);
   /* If a previous PT_LOAD shares the last page, EEXIST is harmless. */
   for(uintptr_t addr=start;addr<end;addr+=page){
     void *r=mmap((void*)addr,page,PROT_READ|PROT_WRITE|PROT_EXEC,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
     if(r==MAP_FAILED && errno!=EEXIST)die("mmap");
   }
   memcpy((void*)(uintptr_t)ph[i].p_vaddr,elf+ph[i].p_offset,ph[i].p_filesz);
 }
 /* Executable pages become RX, not writable during calls. */
 for(unsigned i=0;i<h->e_phnum;i++){
   if(ph[i].p_type!=PT_LOAD || !(ph[i].p_flags&PF_X))continue;
   uintptr_t start=ph[i].p_vaddr & ~(page-1);
   uintptr_t end=(ph[i].p_vaddr+ph[i].p_memsz+page-1)&~(page-1);
   if(mprotect((void*)start,end-start,PROT_READ|PROT_EXEC))die("mprotect");
 }
 uintptr_t event_addr=find_symbol(elf,(size_t)n,"styleos_documents_app_event_v1");
 uintptr_t draw_addr=find_symbol(elf,(size_t)n,"styleos_documents_app_draw_v1");
 uintptr_t grammar_addr=find_symbol(elf,(size_t)n,"styleos_documents_app_grammar_v1");
 assert(event_addr&&draw_addr&&grammar_addr);
 uint32_t (*entry)(uint32_t,const sdr_broker*,int,int)=(void*)h->e_entry;
 int (*event)(const sdr_event*)=(void*)event_addr;
 int (*draw)(void)=(void*)draw_addr;
 int (*grammar)(srev_language,srev_grammar_issue*,size_t,size_t*)=(void*)grammar_addr;
 sdr_broker host={SDR_ABI_VERSION,0,present,dialog,{0,rd,wr}};
 assert(entry(SDR_ABI_VERSION,&host,1200,750)==SDR_ABI_VERSION);
 const char *text="Я редактирую реальный ELF";
 const char *edited="Я редактирую настоящий ELF";
 sdr_event e={0};e.type=SDR_EVENT_TEXT;e.text=text;e.bytes=strlen(text);
 assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_FIND;e.text="реальный";e.bytes=strlen(e.text);
 assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_REPLACE_ALL;e.text="настоящий";e.bytes=strlen(e.text);
 assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_UNDO;assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_REDO;assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_SAVE_AS_GRANTED;e.token=17;e.format=SDA_TXT;
 assert(event(&e)==SDM_OK);
 assert(stored==strlen(edited)&&!memcmp(storage,edited,stored));
 e=(sdr_event){0};e.type=SDR_EVENT_NEW;
 assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_OPEN_GRANTED;e.token=17;e.format=SDA_TXT;
 assert(event(&e)==SDM_OK);
 assert(draw()==SDM_OK&&frames==1);
 e=(sdr_event){0};e.type=SDR_EVENT_SAVE_AS_GRANTED;e.token=17;e.format=SDA_DOCX;
 assert(event(&e)==SDM_OK&&stored>500);
 assert(storage[0]=='P'&&storage[1]=='K');
 {FILE *out=fopen("build/755/elf-host.docx","wb");assert(out);fwrite(storage,1,stored,out);fclose(out);}
 uintptr_t imp_addr=find_symbol(elf,(size_t)n,"sdm_docx_import_stored");
 assert(imp_addr);
 int (*import_file)(const uint8_t*,size_t,sdm_document*)=(void*)imp_addr;
 int irc=import_file((const uint8_t*)storage,stored,&scratch_doc);
 fprintf(stderr,"Import binary direct %d bytes %zu\n",irc,scratch_doc.bytes);
 e=(sdr_event){0};e.type=SDR_EVENT_NEW;assert(event(&e)==SDM_OK);
 e=(sdr_event){0};e.type=SDR_EVENT_OPEN_GRANTED;e.token=17;e.format=SDA_DOCX;
 {int result=event(&e);if(result!=SDM_OK)fprintf(stderr,"DOCX import binary error %d, size %zu\n",result,stored);assert(result==SDM_OK);}
 assert(draw()==SDM_OK&&frames==2);
 srev_grammar_issue issues[SREV_MAX_ISSUES];size_t issue_count=0;
 assert(grammar(SREV_BOTH,issues,SREV_MAX_ISSUES,&issue_count)==SREV_OK);
 puts("ELF HOST EXECUTION PASS: real x86-64 ELF entry/event/draw, TXT+DOCX save/open, grammar API, isolated broker");
 free(elf);return 0;
}
