#include <styleos/acpi_namespace_816.h>
#include <stddef.h>
#include <stdint.h>
static size_t textlen(const char *s){size_t n=0;while(n<ACPI816_PATH_SIZE&&s[n])n++;return n;}
static int texteq(const char *a,const char *b){size_t n=0;while(n<ACPI816_PATH_SIZE&&a[n]&&b[n]){if(a[n]!=b[n])return 0;n++;}return n<ACPI816_PATH_SIZE&&a[n]==b[n];}
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t r64(const uint8_t *p){return (uint64_t)r32(p)|((uint64_t)r32(p+4)<<32);}
static int checked(const uint8_t *p,size_t n,const char *sig){
 if(!p||n<36||n>ACPI816_MAX_TABLE||r32(p+4)!=n)return 0;
 for(unsigned i=0;i<4;i++)if(p[i]!=(uint8_t)sig[i])return 0;
 uint8_t sum=0;for(size_t i=0;i<n;i++)sum=(uint8_t)(sum+p[i]);return sum==0;
}
/* AML PkgLength includes the bytes of its own length. */
static int package(const uint8_t *p,size_t n,size_t *head,size_t *total){
 if(!p||!head||!total||!n)return 0;
 unsigned following=p[0]>>6;
 if(following>3||n<following+1u)return 0;
 size_t v=following?(size_t)(p[0]&0x0f):(size_t)(p[0]&0x3f);
 for(unsigned k=0;k<following;k++)v|=(size_t)p[k+1]<<(4u+8u*k);
 if(v<following+1u||v>n)return 0;
 *head=following+1u;*total=v;return 1;
}
static int seg_char(uint8_t x,int first){
 return x=='_'||(x>='A'&&x<='Z')||(!first&&x>='0'&&x<='9');
}
/* Fully qualified path using only AML NameString. No unchecked string ops. */
static int pathname(const uint8_t *p,size_t n,const char *parent,
                    char out[ACPI816_PATH_SIZE],size_t *used){
 if(!p||!n||!parent||!out||!used)return 0;
 size_t k=0,pos=0,base=0;
 if(p[0]=='\\'){k++;}else{
  while(parent[base]&&base<ACPI816_PATH_SIZE-1u){out[base]=parent[base];base++;}
  if(parent[base])return 0;
 }
 while(k<n&&p[k]=='^'){
  if(!base)return 0;
  while(base&&out[base-1]!='.')base--;
  if(base)base--;
  k++;
 }
 if(k>=n)return 0;
 unsigned parts=1;
 if(p[k]==0x2e){parts=2;k++;}
 else if(p[k]==0x2f){if(k+2>n)return 0;parts=p[k+1];k+=2;}
 else if(p[k]==0){if(base>=ACPI816_PATH_SIZE)return 0;out[base]=0;*used=k+1;return 1;}
 if(!parts||parts>8||parts>(n-k)/4)return 0;
 pos=base;
 for(unsigned j=0;j<parts;j++){
  if(pos){if(pos+1>=ACPI816_PATH_SIZE)return 0;out[pos++]='.';}
  if(pos+4>=ACPI816_PATH_SIZE)return 0;
  for(unsigned b=0;b<4;b++){
   uint8_t c=p[k++];if(!seg_char(c,b==0))return 0;
   out[pos++]=(char)c;
  }
 }
 out[pos]=0;*used=k;return 1;
}
static int literal(const uint8_t *p,size_t n,uint8_t *kind,uint64_t *value,
                   const uint8_t **bytes,uint16_t *bytes_len,size_t *used){
 if(!p||!n||!kind||!value||!bytes||!bytes_len||!used)return 0;
 *kind=1;*bytes=0;*bytes_len=0;
 if(p[0]==0||p[0]==1||p[0]==0xff){*value=p[0]==0xff?UINT64_MAX:p[0];*used=1;return 1;}
 if(p[0]==0x0a&&n>=2){*value=p[1];*used=2;return 1;}
 if(p[0]==0x0b&&n>=3){*value=(uint64_t)p[1]|((uint64_t)p[2]<<8);*used=3;return 1;}
 if(p[0]==0x0c&&n>=5){*value=r32(p+1);*used=5;return 1;}
 if(p[0]==0x0e&&n>=9){*value=r64(p+1);*used=9;return 1;}
 if(p[0]==0x11){
  size_t head,total,sz_used;uint8_t sk;uint64_t sz;const uint8_t *dummy;uint16_t dl;
  if(n<=1||!package(p+1,n-1,&head,&total)||head>=total||
     !literal(p+1+head,total-head,&sk,&sz,&dummy,&dl,&sz_used)||
     sk!=1||sz>ACPI816_MAX_RESOURCE||sz_used>total-head||
     total-head-sz_used!=(size_t)sz)return 0;
  /* No padding, names, runtime expressions or external buffers. */
  *kind=2;*value=0;*bytes=p+1+head+sz_used;*bytes_len=(uint16_t)sz;
  *used=1+total;return 1;
 }
 return 0;
}
static int append(struct acpi816_result *r,const char *path,uint8_t kind,uint64_t num,
                  const uint8_t *src,uint16_t size,unsigned method){
 if(r->objects>=ACPI816_MAX_OBJECTS)return 0;
 for(unsigned j=0;j<r->objects;j++){
  if(texteq(r->object[j].path,path)){r->duplicates++;return 0;}
 }
 struct acpi816_object *x=&r->object[r->objects];
 size_t n=textlen(path);if(n>=ACPI816_PATH_SIZE)return 0;
 for(size_t i=0;i<=n;i++)x->path[i]=path[i];
 x->kind=kind;x->number=num;x->resource_len=size;x->method=(uint8_t)method;
 if(src)for(size_t i=0;i<size;i++)x->resource[i]=src[i];
 r->objects++;return 1;
}
/* Structural walker. Unknown object makes the complete-namespace claim false.
 * In particular NEVER search raw binary payloads for Name or Method bytes. */
static void walk(const uint8_t *p,size_t n,const char *parent,
                 unsigned depth,struct acpi816_result *r){
 if(depth>ACPI816_MAX_DEPTH){r->unsupported++;r->complete=0;return;}
 for(size_t off=0;off<n;){
  uint8_t op=p[off];unsigned headop=0;
  if(op==0x10)headop=1;
  if(op==0x5b&&off+2<=n&&p[off+1]==0x82)headop=2;
  if(headop){
   size_t head,total,used;char path[ACPI816_PATH_SIZE];
   if(off+headop>=n||!package(p+off+headop,n-off-headop,&head,&total)||
      !pathname(p+off+headop+head,total-head,parent,path,&used)||
      used>total-head){r->unsupported++;r->complete=0;return;}
   if(headop==1)r->scopes++;else r->devices++;
   walk(p+off+headop+head+used,total-head-used,path,depth+1,r);
   off+=headop+total;continue;
  }
  if(op==0x08){
   char path[ACPI816_PATH_SIZE];size_t nameused,dataused;uint8_t kind;uint64_t value;
   const uint8_t *data;uint16_t size;
   if(!pathname(p+off+1,n-off-1,parent,path,&nameused)||
      nameused>=n-off-1||
      !literal(p+off+1+nameused,n-off-1-nameused,&kind,&value,&data,&size,&dataused)||
      !append(r,path,kind,value,data,size,0)){
    r->unsupported++;r->complete=0;return;
   }
   off+=1+nameused+dataused;continue;
  }
  if(op==0x14){
   size_t head,total,used;char path[ACPI816_PATH_SIZE];
   if(off+1>=n||!package(p+off+1,n-off-1,&head,&total)||
      !pathname(p+off+1+head,total-head,parent,path,&used)||
      total-head-used<1){r->unsupported++;r->complete=0;return;}
   const uint8_t *body=p+off+1+head+used;size_t remaining=total-head-used;
   uint8_t kind=0;uint64_t value=0;const uint8_t *data=0;uint16_t size=0;size_t dataused=0;
   /* MethodFlags=0: serialized=no, sync=0, args=0; one Return(literal) only. */
   if(remaining>=2&&body[0]==0&&body[1]==0xa4&&
      literal(body+2,remaining-2,&kind,&value,&data,&size,&dataused)&&
      dataused==remaining-2){
    if(!append(r,path,kind,value,data,size,1)){
     r->unsupported++;r->complete=0;return;
    }
   }else {r->unsupported++;r->complete=0;}
   off+=1+total;continue;
  }
  if(op==0x5b&&off+2<=n&&p[off+1]==0x80)r->regions_denied++;
  /* Unknown AML may contain payload bytes that look like namespace code.
   * Refuse the rest of this list rather than desynchronizing. */
  r->unsupported++;r->complete=0;return;
 }
}
static const struct acpi816_object *get(const struct acpi816_result *r,
                                         const char *prefix,const char *suffix){
 char path[ACPI816_PATH_SIZE];size_t n=textlen(prefix),k=textlen(suffix);
 if(n+1+k>=sizeof path)return 0;
 for(size_t i=0;i<n;i++)path[i]=prefix[i];
 path[n]='.';for(size_t i=0;i<k;i++)path[n+i+1]=suffix[i];path[n+k+1]=0;
 for(unsigned j=0;j<r->objects;j++)if(texteq(r->object[j].path,path))return &r->object[j];
 return 0;
}
static int resource(const uint8_t *p,size_t n){
 if(!p||n<2||p[n-2]!=0x79||p[n-1]!=0)return 0;
 size_t i=0;while(i<n){
  unsigned op=p[i];size_t sz;
  if(op&0x80){if(n-i<3)return 0;sz=3u+(size_t)p[i+1]+((size_t)p[i+2]<<8);}
  else sz=1u+(op&7u);
  if(!sz||sz>n-i)return 0;
  if(op==0x79)return i+sz==n&&sz==2;
  i+=sz;
 }
 return 0;
}
static struct acpi816_result scratch;
int acpi816_inspect(const struct acpi816_table *tables,size_t count,
                    struct acpi816_result *out){
 if(!tables||!out||!count||count>1+ACPI816_MAX_SSDT)return 0;
 scratch=(struct acpi816_result){0};
 struct acpi816_result *r=&scratch;r->complete=1;
 for(size_t i=0;i<count;i++){
  const uint8_t *p=tables[i].bytes;size_t n=tables[i].length;
  if(!checked(p,n,i?"SSDT":"DSDT"))return 0;
  r->tables++;if(i)r->ssdts++;
  walk(p+36,n-36,"",0,r);
 }
 /* Only claim resolved *literal* evaluation if the entire input is parsed.
  * Never infer from MCFG/UEFI, and never grant access to SystemMemory/PCI. */
 if(r->complete&&!r->duplicates&&!r->regions_denied){
  for(unsigned j=0;j<r->objects;j++){
   const struct acpi816_object *hid=&r->object[j];
   const size_t n=textlen(hid->path);
   if(n<5||!texteq(hid->path+n-4,"_HID")||hid->path[n-5]!='.')continue;
   if(hid->kind!=1||hid->number!=UINT64_C(0x080ad041))continue;
   char base[ACPI816_PATH_SIZE];for(size_t k=0;k<n-5;k++)base[k]=hid->path[k];base[n-5]=0;
   const struct acpi816_object *seg=get(r,base,"_SEG"),*bbn=get(r,base,"_BBN"),*crs=get(r,base,"_CRS");
   if(seg&&seg->kind==1&&seg->number<=UINT16_MAX){r->seg_known=1;r->hostbridge_segment=(uint16_t)seg->number;}
   if(bbn&&bbn->kind==1&&bbn->number<=255){r->bbn_known=1;r->hostbridge_bus=(uint16_t)bbn->number;}
   if(crs&&crs->kind==2&&resource(crs->resource,crs->resource_len)){
    r->crs_known=1;r->crs_length=crs->resource_len;
   }
   if(r->seg_known&&r->bbn_known&&r->crs_known)r->hostbridge_literal_ready=1;
  }
 }
 *out=*r;return 1;
}
int acpi816_allow_dma(const struct acpi816_result *out,uint16_t requester){
 (void)out;(void)requester;return 0;
}
