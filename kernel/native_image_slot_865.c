#include <styleos/native_image_slot_865.h>
static bool span(const void *p,size_t n){return p&&n&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool apart(const void *a,size_t n,const void *b,size_t m){
 return span(a,n)&&span(b,m)&&((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
enum nte_status s865_load_owned(const struct s844_arena *arena,struct s844_lease lease,
 const struct s865_image_source *source,uint8_t *scratch,size_t capacity,struct nte_loaded *out){
 if(!arena||!source||!out||!scratch||!capacity||lease.pages!=3||
    (uintptr_t)source%_Alignof(struct s865_image_source)||
    (uintptr_t)out%_Alignof(struct nte_loaded)||!s844_lease_live(arena,lease))return NTE_ARGUMENT;
 const void *mem=(const void *)(uintptr_t)arena->base;
 const void *ptr[]={arena,source,scratch,out,mem};
 const size_t size[]={sizeof *arena,sizeof *source,capacity,sizeof *out,S844_BYTES};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<i;j++)
  if(!apart(ptr[i],size[i],ptr[j],size[j]))return NTE_ARGUMENT;
 struct s865_image_source request=*source;
 struct nte_loaded result={0};
 uint8_t *page=(void *)(uintptr_t)lease.pa;
 enum nte_status status=nte_load_x64_reader(request.read,request.opaque,request.lba,
     request.sectors,request.bytes,request.sha256,scratch,capacity,page,NTE_PAGE,&result);
 if(status!=NTE_OK)return status;
 /* No fallible step after successful code validation. Publish only the
  * validated extent and a clean data/stack; caller performs PID admission. */
 volatile uint8_t *p=page;
 for(size_t i=NTE_PAGE;i<3*NTE_PAGE;i++)p[i]=0;
 *out=result;return NTE_OK;
}
