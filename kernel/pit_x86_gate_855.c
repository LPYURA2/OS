#include <styleos/pit_x86_855.h>
bool pit_x86_gate_855(const struct pit_x86_contract_855 *c,uint64_t root,uint64_t flags,uint16_t cs,bool tcg,bool tsc,bool sse2)
{
    return c&&c->terminal&&c->exclusive&&c->post_ebs&&c->reference_attested&&c->root&&!(c->root&4095u)&&root==c->root&&cs&&!(cs&3u)&&!(flags&0x200u)&&tcg&&tsc&&sse2;
}
