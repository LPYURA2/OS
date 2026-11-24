#include <styleos/fx_context_849.h>
#define BIT(n) (UINT64_C(1)<<(n))
static bool region(const void *p,size_t n,size_t a){return p&&!((uintptr_t)p%a)&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool separate(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a;}
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static void put32(uint8_t *b,uint32_t x){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(x>>(8*i));}
static uint32_t effective_mask(uint32_t reported){return reported?(reported&UINT32_C(0xffff)):S849_MXCSR_FALLBACK;}
static bool mask_ok(uint32_t x){return x&&!(x&~UINT32_C(0xffff))&&(x&0x1f80)==0x1f80;}
static bool root_ok(uint64_t r){return r&&!(r&0xfff)&&!(r>>52);}
static void zero(void *p,size_t n){volatile uint8_t *b=p;while(n--)*b++=0;}
bool s849_admit(const struct s849_caps *c,uint32_t *out){
 if(!region(c,sizeof *c,_Alignof(struct s849_caps))||!region(out,sizeof *out,_Alignof(uint32_t))||!separate(c,sizeof *c,out,sizeof *out))return false;
 /* x87, FXSR, SSE, SSE2; CR0.MP/NE on, EM/TS off; CR4.OSFXSR and OSXMMEXCPT on. Reject OSXSAVE (untracked state). CPUID XSAVE support alone is fine. */
 const uint32_t need=(1u<<0)|(1u<<24)|(1u<<25)|(1u<<26);
 if((c->leaf1_edx&need)!=need||(c->cr0&(BIT(1)|BIT(5)))!=(BIT(1)|BIT(5))||(c->cr0&(BIT(2)|BIT(3)))||(c->cr4&(BIT(9)|BIT(10)))!=(BIT(9)|BIT(10))||(c->cr4&BIT(18))||(c->leaf1_ecx&(1u<<27)))return false;
 uint32_t mask=effective_mask(c->mxcsr_mask);if(!mask_ok(mask))return false;*out=mask;return true;
}
bool s849_image_valid(const struct s849_image *i,uint32_t mask){
 if(!region(i,sizeof *i,_Alignof(struct s849_image))||!mask_ok(mask))return false;
 /* MXCSR_MASK is evidence from FXSAVE, not a permission grant. */
 uint32_t reported=get32(i->bytes+28);if(effective_mask(reported)!=mask)return false;
 return !(get32(i->bytes+24)&~mask);
}
static void clean_image(struct s849_image *i,uint32_t mask){zero(i,sizeof *i);i->bytes[0]=0x7f;i->bytes[1]=3;put32(i->bytes+24,0x1f80);put32(i->bytes+28,mask);}
static bool bank_ok(const struct s849_bank *b){
 if(!region(b,sizeof *b,_Alignof(struct s849_bank))||!b->admitted||!mask_ok(b->mxcsr_mask)||b->active< -1||b->active>1)return false;
 for(unsigned i=0;i<2;i++)if(b->slot[i].live&&(!root_ok(b->slot[i].root)||!b->slot[i].generation||!s849_image_valid(&b->slot[i].image,b->mxcsr_mask)))return false;
 if(b->slot[0].live&&b->slot[1].live&&b->slot[0].root==b->slot[1].root)return false;
 return b->active<0||b->slot[b->active].live;
}
bool s849_init(struct s849_bank *b,const struct s849_caps *c,uint64_t a,uint64_t z){
 uint32_t mask;if(!region(b,sizeof *b,_Alignof(struct s849_bank))||!region(c,sizeof *c,_Alignof(struct s849_caps))||!separate(b,sizeof *b,c,sizeof *c)||!root_ok(a)||!root_ok(z)||a==z||!s849_admit(c,&mask))return false;
 /* Caller owns a fresh, inactive object. Refuse reinitialization of live bank. */
 if(b->admitted)return false;
 zero(b,sizeof *b);b->active=-1;b->mxcsr_mask=mask;b->slot[0].root=a;b->slot[1].root=z;
 for(unsigned i=0;i<2;i++){clean_image(&b->slot[i].image,mask);b->slot[i].generation=1;b->slot[i].live=true;}b->admitted=true;return true;
}
static bool match(const struct s849_bank *b,unsigned id,uint64_t root,uint64_t gen){return bank_ok(b)&&id<2&&b->slot[id].live&&b->slot[id].root==root&&b->slot[id].generation==gen;}
bool s849_dispatch(struct s849_bank *b,unsigned id,uint64_t root,uint64_t gen,struct s849_image *out){
 if(!match(b,id,root,gen)||b->active!=-1||!region(out,sizeof *out,_Alignof(struct s849_image))||!separate(b,sizeof *b,out,sizeof *out))return false;
 *out=b->slot[id].image;b->active=(int)id;return true;
}
bool s849_suspend(struct s849_bank *b,unsigned id,uint64_t root,uint64_t gen,const struct s849_image *in){
 if(!match(b,id,root,gen)||b->active!=(int)id||!s849_image_valid(in,b->mxcsr_mask)||!separate(b,sizeof *b,in,sizeof *in))return false;
 b->slot[id].image=*in;b->active=-1;return true;
}
bool s849_retire(struct s849_bank *b,unsigned id,uint64_t root,uint64_t gen){
 if(!match(b,id,root,gen)||b->active!=-1)return false;
 zero(&b->slot[id],sizeof b->slot[id]);return true;
}
