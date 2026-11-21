#include <assert.h>
#include <stdio.h>
#include <styleos/kernel_mapping_inventory.h>
int main(void)
{
    struct kmi_inventory a={0};
    struct kmi_observation m={.va=0x100000,.pa=0x100000,
        .cache_type=6,.writable=true,.executable=true,.user=false};
    assert(kmi_account(&a,KMI_IMAGE,&m,true)==KMI_OK);
    assert(a.pages[KMI_IMAGE]==1 && a.rwx_pages==1 &&
           a.checked_pages==1 && !a.root_activation_permitted);
    m.pa+=4096;
    assert(kmi_account(&a,KMI_IMAGE,&m,true)==KMI_TRANSLATION);
    m.pa-=4096;
    m.user=true;
    assert(kmi_account(&a,KMI_IMAGE,&m,true)==KMI_PRIVILEGE);
    m.user=false;
    m.cache_type=0;
    assert(kmi_account(&a,KMI_IMAGE,&m,true)==KMI_CACHE);
    m.cache_type=6;
    m.va++; m.pa++;
    assert(kmi_account(&a,KMI_IMAGE,&m,true)==KMI_PAGE);
    m.va--; m.pa--;
    assert(kmi_account(&a,(enum kmi_kind)KMI_KIND_COUNT,&m,true)==KMI_ARGUMENT);
    assert(kmi_account(NULL,KMI_IMAGE,&m,true)==KMI_ARGUMENT);
    assert(kmi_account(&a,KMI_IMAGE,NULL,true)==KMI_ARGUMENT);
    m.va=0x300000; m.pa=0x400000; m.writable=false; m.executable=false;
    assert(kmi_account(&a,KMI_GDT,&m,false)==KMI_OK);
    assert(a.pages[KMI_GDT]==1 && a.rwx_pages==1 && !a.root_activation_permitted);
    a.checked_pages=KMI_MAX_ACCOUNTED;
    assert(kmi_account(&a,KMI_GDT,&m,false)==KMI_LIMIT);
    a.checked_pages=2;
    a.root_activation_permitted=true;
    assert(kmi_account(&a,KMI_GDT,&m,false)==KMI_OK);
    assert(!a.root_activation_permitted);
    puts("STYLEOS 779 INHERITED INVENTORY HOST PASS fail-closed=1");
    return 0;
}
