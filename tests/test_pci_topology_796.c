#include <styleos/pci_topology_796.h>
#include <assert.h>
#include <stdio.h>
int main(void){
    uint32_t w[16]={0};struct pci796_bridge b;
    w[0]=0x10001234u;w[2]=0x06040000u;w[3]=0x00010000u;
    w[6]=0x00020200u; /* primary 0, secondary 2, subordinate 2 */
    w[8]=0x80008000u; /* memory [0x80000000,0x800fffff] */
    w[9]=0x90009000u; /* pref [0x90000000,0x900fffff] */
    assert(pci796_parse_bridge(w,0,&b));
    assert(b.primary==0 && b.secondary==2 && b.subordinate==2);
    assert(b.memory_window && b.memory_base==0x80000000ull && b.memory_end==0x80100000ull);
    assert(b.prefetch_window && b.prefetch_base==0x90000000ull && b.prefetch_end==0x90100000ull);
    w[6]=0x00010100u;assert(pci796_parse_bridge(w,0,&b));
    w[6]=0x00020100u;assert(pci796_parse_bridge(w,0,&b));
    w[6]=0x00010200u;assert(!pci796_parse_bridge(w,0,&b));
    w[6]=0x00020200u;
    assert(!pci796_parse_bridge(w,2,&b));
    w[9]=0x90019000u;assert(!pci796_parse_bridge(w,0,&b)); /* type mismatch */
    w[9]=0x90019001u;w[10]=1;w[11]=1;
    assert(pci796_parse_bridge(w,0,&b));
    assert(b.prefetch_base==0x190000000ull && b.prefetch_end==0x190100000ull);
    w[11]=0xffffffffu;w[9]=0xfff1fff1u;assert(!pci796_parse_bridge(w,0,&b));
    uint8_t queue[256]={0};bool visited[256]={0};uint16_t tail=1;
    queue[0]=0;visited[0]=true;
    assert(pci796_queue_secondary(2,queue,&tail,visited));
    assert(tail==2 && queue[1]==2 && visited[2]);
    assert(!pci796_queue_secondary(2,queue,&tail,visited));
    assert(!pci796_queue_secondary(0,queue,&tail,visited));
    tail=256;assert(!pci796_queue_secondary(3,queue,&tail,visited));
    puts("STYLEOS S796 TOPOLOGY HOST PASS");
    return 0;
}
