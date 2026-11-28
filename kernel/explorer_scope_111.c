#include <styleos/explorer_scope_111.h>
static bool identity(const struct usr_runtime *u,uint32_t pid,uint64_t handle,uint64_t root,uint32_t *session){
 if(!u||!u->ready||!u->processes||!pid||!handle||!root||(root&4095u))return false;
 const struct pr_entry *p=pr_find(u->processes,pid);
 if(!p||p->type!=PR_NATIVE_USER||!pr_is_live(u->processes,pid)||
    p->scheduler_handle!=handle||p->page_root!=root)return false;
 for(unsigned i=0;i<PR_MAX_PROCESSES;++i){
  const struct usr_owner *o=&u->owners[i];
  if(o->pid!=pid||o->scheduler_handle!=handle)continue;
  const struct usr_session *v=usr_find(u,o->session_id);
  if(!v||v->state!=USR_ACTIVE||u->selected_session!=v->id)return false;
  *session=v->id;return true;
 }
 return false;
}
static bool separated(const void *a,size_t n,const void *b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&n<=UINTPTR_MAX-x&&m<=UINTPTR_MAX-y&&(x+n<=y||y+m<=x);
}
void ex111_close(struct ex111_scope *s){if(s)*s=(struct ex111_scope){0};}
bool ex111_authorized(const struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root){
 uint32_t session=0;
 return s&&s->active&&s->fs&&s->depth&&s->depth<=EX111_DEPTH&&
  identity(s->users,pid,handle,root,&session)&&pid==s->owner.pid&&
  handle==s->owner.handle&&root==s->owner.root&&session==s->owner.session;
}
static enum po64_status check(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root){
 if(!s||!s->active||!s->fs||!s->users||!s->depth||s->depth>EX111_DEPTH)return PO64_INVALID;
 uint32_t session=0;
 if(!identity(s->users,pid,handle,root,&session)||pid!=s->owner.pid||
    handle!=s->owner.handle||root!=s->owner.root||session!=s->owner.session)return PO64_DENIED;
 struct po64_snapshot now;
 enum po64_status rc=po64_snapshot(s->fs,&now);
 if(rc!=PO64_OK)return rc;
 return now.mount==s->snapshot.mount&&now.sequence==s->snapshot.sequence?PO64_OK:PO64_STALE;
}
enum po64_status ex111_open(struct ex111_scope *s,struct po64_service *fs,const struct usr_runtime *u,
 uint32_t pid,uint64_t handle,uint64_t root,uint64_t approved_directory){
 if(!s||!fs||!u||!approved_directory||s->active||
    !separated(s,sizeof *s,fs,sizeof *fs)||!separated(s,sizeof *s,u,sizeof *u)||
    !separated(s,sizeof *s,u->processes,sizeof *u->processes))return PO64_INVALID;
 uint32_t session=0;
 if(!identity(u,pid,handle,root,&session))return PO64_DENIED;
 struct po64_snapshot snap;
 enum po64_status rc=po64_snapshot(fs,&snap);
 if(rc!=PO64_OK)return rc;
 struct po64_row row;
 rc=po64_child_at(fs,snap,approved_directory,0,&row);
 if(rc!=PO64_OK&&rc!=PO64_END)return rc;
 *s=(struct ex111_scope){.fs=fs,.users=u,.owner={pid,session,handle,root},
  .snapshot=snap,.dirs={approved_directory},.depth=1,.active=true};
 return PO64_OK;
}
enum po64_status ex111_row(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t ordinal,struct po64_row *out){
 enum po64_status rc=check(s,pid,handle,root);
 if(rc!=PO64_OK)return rc;
 if(!separated(out,sizeof *out,s,sizeof *s)||!separated(out,sizeof *out,s->fs,sizeof *s->fs)||
    !separated(out,sizeof *out,s->users,sizeof *s->users)||
    !separated(out,sizeof *out,s->users->processes,sizeof *s->users->processes))return PO64_INVALID;
 return po64_child_at(s->fs,s->snapshot,s->dirs[s->depth-1],ordinal,out);
}
enum po64_status ex111_enter(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t ordinal){
 struct po64_row row;
 enum po64_status rc=ex111_row(s,pid,handle,root,ordinal,&row);
 if(rc!=PO64_OK)return rc;
 if(row.entry.kind!=SD64_DIRECTORY)return PO64_DENIED;
 if(s->depth==EX111_DEPTH)return PO64_FULL;
 for(unsigned i=0;i<s->depth;++i)if(s->dirs[i]==row.entry.target)return PO64_CORRUPT;
 s->dirs[s->depth++]=row.entry.target;return PO64_OK;
}
enum po64_status ex111_up(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root){
 enum po64_status rc=check(s,pid,handle,root);
 if(rc!=PO64_OK)return rc;
 if(s->depth==1)return PO64_DENIED;
 s->dirs[--s->depth]=0;return PO64_OK;
}

enum po64_status ex119_allow_edit(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root){
 enum po64_status rc=check(s,pid,handle,root);
 if(rc!=PO64_OK)return rc;
 if(!s->fs->writable||!s->fs->pair.id_floor)return PO64_DENIED;
 s->edit_allowed=true;return PO64_OK;
}
enum po64_status ex119_refresh(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root){
 if(!ex111_authorized(s,pid,handle,root))return PO64_DENIED;
 struct po64_snapshot now;
 enum po64_status rc=po64_snapshot(s->fs,&now);
 if(rc!=PO64_OK)return rc;
 if(now.mount!=s->snapshot.mount||!s->fs->pair.id_floor||now.sequence<s->snapshot.sequence)return PO64_STALE;
 struct po64_row row;
 rc=po64_child_at(s->fs,now,s->dirs[0],0,&row);
 if(rc!=PO64_OK&&rc!=PO64_END)return rc;
 s->snapshot=now;
 for(unsigned i=1;i<EX111_DEPTH;++i)s->dirs[i]=0;
 s->depth=1;return PO64_OK;
}
enum po64_status ex119_edit(struct ex111_scope *s,uint32_t pid,uint64_t handle,uint64_t root,
 unsigned op,uint64_t ordinal,uint64_t expected_id,uint64_t expected_revision,
 unsigned ancestor,const uint8_t *name,size_t name_bytes){
 if(op<EX119_FILE||op>EX119_MOVE_UP)return PO64_INVALID;
 enum po64_status rc=check(s,pid,handle,root);
 if(rc!=PO64_OK)return rc;
 if(!s->edit_allowed||!s->fs->writable||!s->fs->pair.id_floor)return PO64_DENIED;
 bool create=op==EX119_FILE||op==EX119_DIRECTORY;
 bool remove=op==EX119_DELETE;
 bool move=op==EX119_MOVE_UP;
 if(create?(ordinal||expected_id||expected_revision):expected_id<2)return PO64_INVALID;
 if(move?(ancestor>=s->depth-1):ancestor!=0)return PO64_INVALID;
 if(remove?(name||name_bytes):(!name||!name_bytes||name_bytes>SD64_NAME))return PO64_INVALID;
 struct sd64_key dest={.parent=s->dirs[move?ancestor:s->depth-1]};
 if(!remove){
  dest.length=(uint8_t)name_bytes;
  for(size_t i=0;i<name_bytes;++i)dest.name[i]=name[i];
  if(!sd64_key_valid(&dest))return PO64_INVALID;
 }
 if(create){
  rc=op==EX119_FILE?po64_create_empty(s->fs,s->snapshot,&dest,0):
     po64_mkdir(s->fs,s->snapshot,&dest,0);
 }else{
  struct po64_row row;
  rc=ex111_row(s,pid,handle,root,ordinal,&row);
  if(rc!=PO64_OK)return rc;
  if(row.entry.target!=expected_id||row.revision!=expected_revision)return PO64_STALE;
  if(row.entry.kind==SD64_FILE)rc=po64_change_file(s->fs,s->snapshot,&row,remove?0:&dest);
  else if(row.entry.kind==SD64_DIRECTORY)rc=po64_change_directory(s->fs,s->snapshot,&row,remove?0:&dest);
  else return PO64_CORRUPT;
 }
 /* A successful broker commit already published the validated new pair.
  * Current directory and ancestors are not editable from this operation.
  * No extra I/O that could turn a durable success into an apparent failure. */
 if(rc==PO64_OK)s->snapshot.sequence=s->fs->pair.sequence;
 return rc;
}
