/* Only for the experimental host-ABI-independent ELF package. This is NOT
 * the StyleOS system allocator and never executes in the 744 bootloader. */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
void *memset(void *dest,int ch,size_t n){unsigned char *p=dest;for(size_t i=0;i<n;i++)p[i]=(unsigned char)ch;return dest;}
void *memcpy(void *dest,const void *src,size_t n){unsigned char *p=dest;const unsigned char *q=src;for(size_t i=0;i<n;i++)p[i]=q[i];return dest;}
void *memmove(void *dest,const void *src,size_t n){unsigned char *p=dest;const unsigned char *q=src;if(p>q && p<q+n){for(size_t i=n;i;i--)p[i-1]=q[i-1];}else{for(size_t i=0;i<n;i++)p[i]=q[i];}return dest;}
int memcmp(const void *aa,const void *bb,size_t n){const unsigned char *a=aa,*b=bb;for(size_t i=0;i<n;i++)if(a[i]!=b[i])return (int)a[i]-(int)b[i];return 0;}
size_t strlen(const char *s){size_t n=0;while(s[n])n++;return n;}
void *memchr(const void *buf,int ch,size_t n){const unsigned char *p=buf;for(size_t i=0;i<n;i++)if(p[i]==(unsigned char)ch)return (void*)(uintptr_t)(p+i);return 0;}
/* Small static first-fit arena for bounded tracked-change text; no external
 * allocator. Alignment is at least 16 bytes, blocks are reclaimed on free. */
typedef struct block {size_t capacity;struct block *next;unsigned used;unsigned pad;} block;
static union{max_align_t align;unsigned char b[262144];} arena;
static block *head=0;
static size_t size_align(size_t n){return (n+15u)&~(size_t)15u;}
void *malloc(size_t n){
 if(!n || n>65536)return 0;
 n=size_align(n);if(!head){head=(block*)arena.b;head->capacity=sizeof(arena.b)-sizeof(block);head->next=0;head->used=0;}
 for(block *p=head;p;p=p->next){
  if(p->used||p->capacity<n)continue;
  if(p->capacity>=n+sizeof(block)+16){block *q=(block*)((unsigned char*)p+sizeof(block)+n);q->capacity=p->capacity-n-sizeof(block);q->next=p->next;q->used=0;p->next=q;p->capacity=n;}
  p->used=1;return (unsigned char*)p+sizeof(block);
 }return 0;
}
void free(void *x){
 if(!x||!head)return;
 block *p=head;for(;p;p=p->next)if((unsigned char*)p+sizeof(block)==(unsigned char*)x)break;
 if(!p)return;p->used=0;
 for(block *q=head;q&&q->next;){if(!q->used&&!q->next->used){q->capacity+=sizeof(block)+q->next->capacity;q->next=q->next->next;}else q=q->next;}
}
static void out(char *buf,size_t size,size_t *pos,char c){if(size && *pos<size-1)buf[*pos]=c;(*pos)++;}
static void unum(char *buf,size_t size,size_t *pos,unsigned n){char tmp[16];size_t c=0;do{tmp[c++]=(char)('0'+n%10);n/=10;}while(n);while(c)out(buf,size,pos,tmp[--c]);}
int snprintf(char *buf,size_t size,const char *fmt,...){
 if(!buf||!fmt||!size)return -1;
 va_list ap;va_start(ap,fmt);size_t pos=0;
 for(const char *p=fmt;*p;p++){
   if(*p=='%'&&p[1]){
    p++;if(*p=='u'){unum(buf,size,&pos,va_arg(ap,unsigned));continue;}
    if(*p=='d'){int n=va_arg(ap,int);if(n<0){out(buf,size,&pos,'-');unum(buf,size,&pos,0u-(unsigned)n);}else unum(buf,size,&pos,(unsigned)n);continue;}
    if(*p=='%'){out(buf,size,&pos,'%');continue;}
    out(buf,size,&pos,'%');
   }
   out(buf,size,&pos,*p);
 }
 va_end(ap);buf[pos<size?pos:size-1]=0;return (int)pos;
}
