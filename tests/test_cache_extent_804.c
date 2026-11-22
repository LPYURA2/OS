#include <styleos/cache_extent_804.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
 struct cache804_span s={0};uint64_t stop=0;
 assert(cache804_plan(UINT64_C(0x80000000),4096000,UINT64_C(0x1000000),&s));
 assert(s.start==UINT64_C(0x80000000)&&s.end==UINT64_C(0x803e8000)&&s.pages==1000);
 assert(cache804_plan(UINT64_C(0x81000000),4096,UINT64_C(0x200000),&s)&&s.pages==1);
 assert(cache804_plan(UINT64_C(0xc000000000),16384,UINT64_C(0x20000000),&s)&&s.pages==4);
 assert(!cache804_plan(0,0,4096,&s));
 assert(!cache804_plan(0,8192,4096,&s));
 assert(!cache804_plan(1,4096,4096,&s));
 assert(!cache804_plan(UINT64_MAX-4095,8192,8192,&s));
 assert(!cache804_plan(0,4096,UINT64_C(0x40000001),&s));
 assert(!cache804_plan(0,4096,4096,0));
 assert(cache804_leaf_end(UINT64_C(0x80000000),UINT64_C(0x803e8000),UINT64_C(0x200000),&stop));
 assert(stop==UINT64_C(0x80200000));
 assert(cache804_leaf_end(stop,UINT64_C(0x803e8000),UINT64_C(0x200000),&stop));
 assert(stop==UINT64_C(0x803e8000));
 assert(!cache804_leaf_end(0,0,4096,&stop));
 assert(!cache804_leaf_end(1,4096,4096,&stop));
 assert(!cache804_leaf_end(0,4096,8192,&stop));
 assert(!cache804_leaf_end(0,4096,4096,0));
 puts("STYLEOS S804 span/leaf host PASS");return 0;
}
