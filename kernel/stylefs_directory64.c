#include <styleos/stylefs_directory64.h>
enum { HEADER=64, RECORD=288, CRC_AT=24 };
static const uint8_t magic[8]={'S','F','6','4','D','I','R','1'};
static uint64_t get(const uint8_t *p,unsigned n){
 uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;
}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static void clear(uint8_t *p,size_t n){for(size_t i=0;i<n;i++)p[i]=0;}
static void copy(uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;i++)p[i]=q[i];}
static uint32_t crc(const uint8_t *p){
 uint32_t c=UINT32_MAX;for(unsigned i=0;i<SD64_PAGE;i++){
  c^=i>=CRC_AT&&i<CRC_AT+4?0:p[i];
  for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);
 }return ~c;
}
static bool separate(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
bool sd64_key_valid(const struct sd64_key *k){
 if(!k||!k->parent||!k->length||k->length>SD64_NAME||
    !zero(k->name+k->length,SD64_NAME-k->length))return false;
 if(k->name[0]=='.'&&(k->length==1||(k->length==2&&k->name[1]=='.')))return false;
 for(unsigned i=0;i<k->length;){
  uint8_t a=k->name[i++];if(a<0x80){if(a<32||a==127||a=='/'||a=='\\')return false;continue;}
  unsigned more;uint32_t cp,min;
  if(a>=0xc2&&a<=0xdf){more=1;cp=a&31;min=0x80;}
  else if(a>=0xe0&&a<=0xef){more=2;cp=a&15;min=0x800;}
  else if(a>=0xf0&&a<=0xf4){more=3;cp=a&7;min=0x10000;}
  else return false;
  if(more>k->length-i)return false;
  while(more--){uint8_t v=k->name[i++];if((v&0xc0)!=0x80)return false;cp=(cp<<6)|(v&63);}
  if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(cp>=0x80&&cp<=0x9f))return false;
 }
 return true;
}
int sd64_compare(const struct sd64_key *a,const struct sd64_key *b){
 if(a->parent!=b->parent)return a->parent<b->parent?-1:1;
 unsigned n=a->length<b->length?a->length:b->length;
 if(n>SD64_NAME)n=SD64_NAME;
 for(unsigned i=0;i<n;i++)if(a->name[i]!=b->name[i])return a->name[i]<b->name[i]?-1:1;
 return a->length==b->length?0:a->length<b->length?-1:1;
}
static bool record_valid(const struct sd64_record *r,unsigned level){
 return sd64_key_valid(&r->key)&&r->target&&
  (level?r->kind==SD64_BRANCH:r->kind==SD64_FILE||r->kind==SD64_DIRECTORY);
}
bool sd64_encode(uint8_t p[SD64_PAGE],uint64_t epoch,unsigned level,const struct sd64_record *r,unsigned n){
 if(!epoch||level>SD64_MAX_LEVEL||!n||n>SD64_FANOUT||
    !separate(p,SD64_PAGE,r,n*sizeof *r))return false;
 for(unsigned i=0;i<n;i++)if(!record_valid(r+i,level)||(i&&sd64_compare(&r[i-1].key,&r[i].key)>=0))return false;
 clear(p,SD64_PAGE);copy(p,magic,8);put(p+8,epoch,8);put(p+16,level,4);put(p+20,n,4);
 for(unsigned i=0;i<n;i++){
  uint8_t *q=p+HEADER+i*RECORD;
  put(q,r[i].key.parent,8);put(q+8,r[i].target,8);put(q+16,r[i].key.length,2);
  q[18]=r[i].kind;copy(q+20,r[i].key.name,SD64_NAME);
 }
 put(p+CRC_AT,crc(p),4);return true;
}
static bool decode(const uint8_t *p,unsigned i,unsigned level,struct sd64_record *r){
 const uint8_t *q=p+HEADER+i*RECORD;
 r->key.parent=get(q,8);r->target=get(q+8,8);r->key.length=(uint16_t)get(q+16,2);r->kind=q[18];
 copy(r->key.name,q+20,SD64_NAME);
 return !q[19]&&zero(q+275,RECORD-275)&&record_valid(r,level);
}
bool sd64_decode_page(const uint8_t p[SD64_PAGE],uint64_t epoch,unsigned level,
                      struct sd64_record out[SD64_FANOUT],unsigned *count){
 if(!epoch||level>SD64_MAX_LEVEL||
    !separate(p,SD64_PAGE,out,SD64_FANOUT*sizeof *out)||
    !separate(p,SD64_PAGE,count,sizeof *count)||
    !separate(out,SD64_FANOUT*sizeof *out,count,sizeof *count))return false;
 for(unsigned i=0;i<8;i++)if(p[i]!=magic[i])return false;
 uint64_t n=get(p+20,4);
 if(get(p+8,8)!=epoch||get(p+16,4)!=level||!n||n>SD64_FANOUT||
    get(p+CRC_AT,4)!=crc(p)||!zero(p+28,HEADER-28)||
    !zero(p+HEADER+n*RECORD,SD64_PAGE-HEADER-n*RECORD))return false;
 struct sd64_record a={0},prev={0};
 for(unsigned i=0;i<n;i++){
  if(!decode(p,i,level,&a)||(i&&sd64_compare(&prev.key,&a.key)>=0))return false;
  prev=a;
 }
 for(unsigned i=0;i<n;i++)(void)decode(p,i,level,out+i);
 *count=(unsigned)n;return true;
}
static bool inside(const struct sd64_root *r,uint64_t s){
 return s>=r->region_start&&s-r->region_start<=r->region_sectors-SD64_SECTORS&&
        (s-r->region_start)%SD64_SECTORS==0;
}
enum sd64_status sd64_lookup(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct sd64_root *r,const struct sd64_key *k,uint8_t p[SD64_PAGE],uint64_t *object,uint8_t *kind){
 if(!b||!r||!sd64_key_valid(k)||!owner||!token||!r->epoch||r->level>SD64_MAX_LEVEL||
    r->region_sectors<SD64_SECTORS||r->region_sectors%SD64_SECTORS||
    r->region_sectors>UINT64_MAX-r->region_start||!inside(r,r->sector)||
    !separate(p,SD64_PAGE,b,sizeof *b)||!separate(p,SD64_PAGE,r,sizeof *r)||
    !separate(p,SD64_PAGE,k,sizeof *k)||!separate(p,SD64_PAGE,object,sizeof *object)||
    !separate(p,SD64_PAGE,kind,sizeof *kind)||!separate(object,sizeof *object,kind,sizeof *kind)||
    !separate(object,sizeof *object,b,sizeof *b)||!separate(kind,sizeof *kind,b,sizeof *b)||
    !separate(object,sizeof *object,r,sizeof *r)||!separate(kind,sizeof *kind,r,sizeof *r)||
    !separate(object,sizeof *object,k,sizeof *k)||!separate(kind,sizeof *kind,k,sizeof *k))return SD64_INVALID;
 /* Preflight the complete metadata range against the live read grant. */
 const struct bs_grant *grant=0;
 for(unsigned i=0;i<BS_GRANTS;i++)if(b->grants[i].token==token)grant=&b->grants[i];
 if(!grant||grant->owner!=owner||!(grant->rights&BS_READ)||
    r->region_start>grant->length||r->region_sectors>grant->length-r->region_start)return SD64_INVALID;
 uint64_t sector=r->sector;
 bool lower=false,upper=false;struct sd64_key lo={0},hi={0};
 for(unsigned level=r->level;;level--){
  for(unsigned i=0;i<SD64_SECTORS;i++)if(bs_read_sector(b,owner,token,sector+i,p+i*512)!=BS_OK)return SD64_IO;
  for(unsigned i=0;i<8;i++)if(p[i]!=magic[i])return SD64_CORRUPT;
  uint64_t count=get(p+20,4);
  if(get(p+8,8)!=r->epoch||get(p+16,4)!=level||!count||count>SD64_FANOUT||
     get(p+CRC_AT,4)!=crc(p)||!zero(p+28,HEADER-28)||
     !zero(p+HEADER+count*RECORD,SD64_PAGE-HEADER-count*RECORD))return SD64_CORRUPT;
  struct sd64_record rec={0},prev={0},selected={0};
  struct sd64_key new_lo={0};bool chosen=false,new_lower=lower;
  if(lower)new_lo=lo;
  for(unsigned i=0;i<count;i++){
   if(!decode(p,i,level,&rec)||(i&&sd64_compare(&prev.key,&rec.key)>=0)||
      (lower&&sd64_compare(&lo,&rec.key)>=0)||(upper&&sd64_compare(&rec.key,&hi)>0)||
      (level&&(!inside(r,rec.target)||rec.target==sector)))return SD64_CORRUPT;
   if(level){
    for(unsigned j=0;j<i;j++)if(get(p+HEADER+j*RECORD+8,8)==rec.target)return SD64_CORRUPT;
   }
   int cmp=sd64_compare(k,&rec.key);
   if(!chosen&&(level?cmp<=0:cmp==0)){
    selected=rec;chosen=true;
    if(i){new_lo=prev.key;new_lower=true;}
   }
   prev=rec;
  }
  if(upper&&sd64_compare(&prev.key,&hi)!=0)return SD64_CORRUPT;
  if(!chosen)return SD64_MISSING;
  if(!level){*object=selected.target;*kind=selected.kind;return SD64_OK;}
  lo=new_lo;lower=new_lower;hi=selected.key;upper=true;sector=selected.target;
 }
}
