#include <assert.h>
#include <stddef.h>
#include <stdio.h>
void *styleos_test_memmove(void *,const void *,size_t);
int main(void){
 unsigned char buf[64],expected[64],snapshot[64];
 for(size_t src=0;src<64;src++)for(size_t dst=0;dst<64;dst++){
  size_t max=(src>dst?64-src:64-dst);
  for(size_t n=0;n<=max;n++){
   for(size_t i=0;i<64;i++)buf[i]=expected[i]=snapshot[i]=(unsigned char)(i*37u+13u);
   for(size_t i=0;i<n;i++)expected[dst+i]=snapshot[src+i];
   assert(styleos_test_memmove(buf+dst,buf+src,n)==buf+dst);
   for(size_t i=0;i<64;i++)assert(buf[i]==expected[i]);
  }
 }
 puts("STYLEFS 39 RUNTIME MOVE PASS: all in-buffer overlap, disjoint, identical and zero-length cases");
 return 0;
}
