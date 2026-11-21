#include <styleos/frame_grant_proof.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct boot_memory_map map;
static struct boot_memory_evidence evidence;
static struct fg_claims claims;

static void reset(void)
{
    memset(&map,0,sizeof(map));
    memset(&evidence,0,sizeof(evidence));
    memset(&claims,0,sizeof(claims));
    evidence.map=&map;
    evidence.live.image=(struct boot_range){0x100000,0x300000};
    evidence.live.stack=(struct boot_range){0x1a0000,0x1b0000};
    evidence.live.handoff=(struct boot_range){0x1c0000,0x1c1000};
    evidence.live.source=(struct boot_range){0x1e0000,0x1f0000};
    evidence.workspace=(struct boot_range){0x1d0000,0x1d1000};
    map.reserved[0]=evidence.live.image;map.nr_reserved=1;
    map.candidates[0]=(struct boot_range){0x400000,0x500000};map.nr_candidates=1;
    claims.tables=0x110000;
    for(unsigned s=0;s<DVM_USER_SLOTS;++s)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            claims.user[s][j]=0x160000+(s*DVM_USER_PAGES+j)*4096;
}
int main(void)
{
    reset();assert(fg_check_policy(&evidence,&claims)==FG_OK);
    struct fg_claims saved=claims;
    claims.user[1][2]=claims.user[0][0];
    assert(fg_check_policy(&evidence,&claims)==FG_CONFLICT);
    claims=saved;claims.user[0][0]++;
    assert(fg_check_policy(&evidence,&claims)==FG_LAYOUT);
    claims=saved;claims.user[0][0]=claims.tables;
    assert(fg_check_policy(&evidence,&claims)==FG_CONFLICT);
    claims=saved;claims.user[0][0]=evidence.live.stack.start;
    assert(fg_check_policy(&evidence,&claims)==FG_CONFLICT);
    claims=saved;claims.user[0][0]=evidence.workspace.start;
    assert(fg_check_policy(&evidence,&claims)==FG_CONFLICT);
    claims=saved;claims.user[0][0]=0x400000;
    assert(fg_check_policy(&evidence,&claims)==FG_NOT_IMAGE);
    claims=saved;claims.tables=0x300000;
    assert(fg_check_policy(&evidence,&claims)==FG_NOT_IMAGE);
    claims=saved;map.reserved[0].end=0x145000;
    assert(fg_check_policy(&evidence,&claims)==FG_NOT_RESERVED);
    reset();map.candidates[0]=(struct boot_range){0x170000,0x175000};
    claims.user[0][0]=0x170000;
    assert(fg_check_policy(&evidence,&claims)==FG_CANDIDATE);
    reset();map.nr_reserved=BOOT_MEMORY_INPUT_MAX+1u;
    assert(fg_check_policy(&evidence,&claims)==FG_ARGUMENT);
    reset();map.nr_candidates=BOOT_MEMORY_RANGE_MAX+1u;
    assert(fg_check_policy(&evidence,&claims)==FG_ARGUMENT);
    reset();assert(fg_check_policy(0,&claims)==FG_ARGUMENT);
    assert(fg_check_policy(&evidence,0)==FG_ARGUMENT);
    puts("FRAME GRANT POLICY HOST PASS: reserved image, aliases, bounds, excluded pages, candidates");
    return 0;
}
