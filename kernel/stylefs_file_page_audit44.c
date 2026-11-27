#include <styleos/stylefs_file_page_audit44.h>
bool fs44_audit_page(void *ctx,const struct rt862_selection *t,const uint8_t *page){
 const struct fs44_audit *a=ctx;
 if(!a||!t||!page||!a->safe||!a->cache||!a->arena||!a->leases||!a->guard||!a->roots||
    t->slot>=S846_ROOT_COUNT||!t->pid||!t->dispatch||!t->handle||
    !a->observed_safe_root||a->observed_safe_root!=t->safe_root||
    a->safe->built.root!=t->safe_root||t->child_root==t->safe_root)return false;
 const struct s844_lease *l=a->leases+t->slot;
 const struct s846_root *child=a->roots+t->slot;
 if(l->owner!=S846_OWNER_A+t->slot||l->pages!=3||l->generation!=t->handle||
    !s844_lease_live(a->arena,*l)||l->pa>UINT64_MAX-PG_PAGE||
    (uintptr_t)page!=l->pa+PG_PAGE||!child->ready||
    child->built.root!=t->child_root)return false;
 /* Reuse the full existing page-table/lease reconstruction and audit.
  * In particular it checks user data RW/NX, no other arena alias, and
  * the exact supervisor-only identity mapping under the safe root. */
#if defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865)
 if(!s865_root_audit(a->safe,a->cache,a->arena,a->leases,t->slot,a->guard,child))return false;
#else
 if(!s846_audit(a->safe,a->cache,a->arena,a->leases,t->slot,a->guard,child))return false;
#endif
 struct pg_lookup x;
 return pg_lookup(&a->safe->plan,l->pa+PG_PAGE,&x)==PG_OK&&
 x.pa==l->pa+PG_PAGE&&x.flags==PG_WRITE;
}
