#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char user_page[UC772_PAGE];
int main(void)
{
    unsigned char out[UC772_MAX_COPY];
    size_t offset = 777;
    const uint64_t page = 0x11000000u, root = 0x1000u;
    memset(user_page, 0, sizeof(user_page));
    for (unsigned i = 0; i < 8; ++i) user_page[UC772_PAGE-16+i] = (unsigned char)(0xa0+i);
    assert(uc772_plan(root,root,page,page+UC772_PAGE-16,8,&offset)==UC772_OK);
    assert(offset==UC772_PAGE-16);
    memset(out,0xcc,sizeof(out));
    assert(uc772_copy_page(out,sizeof(out),user_page,offset,8)==UC772_OK);
    for (unsigned i=0;i<8;++i) assert(out[i]==0xa0+i);
    for (unsigned i=8;i<sizeof(out);++i) assert(out[i]==0xcc);
    size_t saved=offset;
    assert(uc772_plan(root+0x1000,root,page,page+UC772_PAGE-16,8,&offset)==UC772_ROOT);
    assert(offset==saved);
    assert(uc772_plan(root,root,page,0x12010000u,8,&offset)==UC772_RANGE);
    assert(uc772_plan(root,root,page,page+UC772_PAGE-2,8,&offset)==UC772_RANGE);
    assert(uc772_plan(root,root,page,page-1,1,&offset)==UC772_RANGE);
    assert(uc772_plan(root,root,page,UINT64_MAX,8,&offset)==UC772_RANGE);
    assert(uc772_plan(root,root,page,page,0,&offset)==UC772_BAD);
    assert(uc772_plan(root,root,page,page,UC772_MAX_COPY+1,&offset)==UC772_BAD);
    assert(uc772_plan(root,root,page+1,page+UC772_PAGE-16,8,&offset)==UC772_BAD);
    assert(uc772_plan(0,root,page,page,1,&offset)==UC772_BAD);
    assert(offset==saved);
    memset(out,0xcc,sizeof(out));
    assert(uc772_copy_page(out,sizeof(out),user_page,UC772_PAGE-2,8)==UC772_BAD);
    assert(uc772_copy_page(out,7,user_page,0,8)==UC772_BAD);
    assert(uc772_copy_page(out,sizeof(out),user_page,0,0)==UC772_BAD);
    assert(uc772_copy_page(out,sizeof(out),user_page,0,UC772_MAX_COPY+1)==UC772_BAD);
    assert(uc772_copy_page(out,sizeof(out),0,0,8)==UC772_BAD);
    for (unsigned i=0;i<sizeof(out);++i) assert(out[i]==0xcc);
    puts("USER COPY 772 HOST PASS: root, page bounds, no overflow, no partial output");
}
