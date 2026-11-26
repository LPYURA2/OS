#include <styleos/stylefs_transaction64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum tx64_status cp_status(enum cp64_status rc){
 switch(rc){
 case CP64_OK:return TX64_OK;
 case CP64_IO:return TX64_IO;
 case CP64_CORRUPT:return TX64_CORRUPT;
 case CP64_FULL:return TX64_FULL;
 case CP64_INVALID:return TX64_INVALID;
 default:return TX64_RECOVERY;
 }
}
static enum tx64_status refresh(struct tx64_state *s,struct fb64_workspace *w){
 enum ns64_status nr=ns64_validate(&s->dir,&w->reader,s->nodes,s->slots,&s->proof);
 if(nr!=NS64_OK)return nr==NS64_FULL?TX64_FULL:nr==NS64_IO?TX64_IO:TX64_CORRUPT;
 enum fb64_status fr=fb64_bind(&s->view,&s->dir,&s->proof,&s->data);
 return fr==FB64_OK?TX64_OK:fr==FB64_CORRUPT?TX64_CORRUPT:TX64_RECOVERY;
}
enum tx64_status tx64_open(struct tx64_state *s,struct bs_service *b,const struct cp64_layout *l,
 struct ns64_node *nodes,size_t slots,struct fb64_workspace *w){
 if(!slots||slots>SIZE_MAX/sizeof *nodes)return TX64_INVALID;
 const void *ptr[]={s,b,l,nodes,w};
 const size_t sizes[]={sizeof *s,sizeof *b,sizeof *l,slots*sizeof *nodes,sizeof *w};
 for(unsigned i=0;i<5;i++)for(unsigned k=0;k<i;k++)if(!sep(ptr[i],sizes[i],ptr[k],sizes[k]))return TX64_INVALID;
 *s=(struct tx64_state){0};s->block=b;s->layout=*l;s->nodes=nodes;s->slots=slots;
 enum dc64_status dr=dc64_mount(&s->dir,b,l->owner,l->dir,l->dir_sectors,&w->writer);
 if(dr!=DC64_OK)return dr==DC64_IO?TX64_IO:dr==DC64_CORRUPT?TX64_CORRUPT:TX64_INVALID;
 enum sj_status jr=sfs64_journal_mount(&s->data,b,l->owner,l->data,l->data_sectors,l->data_start);
 if(jr!=SJ_OK)return jr==SJ_IO?TX64_IO:jr==SJ_CORRUPT?TX64_CORRUPT:TX64_RECOVERY;
 enum tx64_status rc=refresh(s,w);
 if(rc!=TX64_OK)return rc;
 rc=cp_status(cp64_current(b,&s->layout,&s->view,&s->receipt));
 if(rc!=TX64_OK){s->view.valid=false;return rc;}
 s->ready=true;return TX64_OK;
}
enum tx64_status tx64_create(struct tx64_state *s,struct fb64_workspace *w,
 const struct sd64_record *e,uint64_t bytes,fb64_source source,void *ctx){
 if(!s||!s->ready||s->busy)return TX64_LOCKED;
 if(!sep(s,sizeof *s,w,sizeof *w)||!sep(s,sizeof *s,e,sizeof *e)||!sep(w,sizeof *w,e,sizeof *e)||
    !sep(w,sizeof *w,s->block,sizeof *s->block)||!sep(e,sizeof *e,s->block,sizeof *s->block)||
    !sep(w,sizeof *w,s->nodes,s->slots*sizeof *s->nodes)||
    !sep(e,sizeof *e,s->nodes,s->slots*sizeof *s->nodes))return TX64_INVALID;
 struct cp64_result current;
 if(cp64_current(s->block,&s->layout,&s->view,&current)!=CP64_OK||
    current.sequence!=s->receipt.sequence||current.epoch!=s->receipt.epoch||
    current.generation!=s->receipt.generation){s->ready=false;s->view.valid=false;return TX64_RECOVERY;}
 /* Refuse known metadata exhaustion before any data write. */
 if(s->dir.count>=s->slots||s->receipt.sequence==UINT64_MAX||
    s->receipt.epoch==UINT64_MAX||s->receipt.generation==UINT64_MAX)return TX64_FULL;
 s->busy=true;s->ready=false;
 enum fb64_status fr=fb64_create(&s->view,w,e,bytes,source,ctx);
 if(fr!=FB64_OK){
  /* Only refusal/cancellation with unchanged live pair may unlock.
   * Never retry uncertain I/O or an orphan publication automatically. */
  enum tx64_status rc=TX64_RECOVERY;
  if(fr==FB64_INVALID||fr==FB64_FULL||fr==FB64_CONFLICT||fr==FB64_CANCELLED){
   if(cp64_current(s->block,&s->layout,&s->view,&current)==CP64_OK&&
      current.sequence==s->receipt.sequence&&current.epoch==s->receipt.epoch&&
      current.generation==s->receipt.generation){
    s->ready=true;rc=fr==FB64_INVALID?TX64_INVALID:fr==FB64_FULL?TX64_FULL:
      fr==FB64_CONFLICT?TX64_CONFLICT:TX64_CANCELLED;
   }
  }
  if(!s->ready)s->view.valid=false;
  s->busy=false;return rc;
 }
 enum tx64_status rc=refresh(s,w);
 if(rc==TX64_OK){
  if(s->view.epoch!=s->receipt.epoch+1||s->view.generation!=s->receipt.generation+1||
     cp64_save(s->block,&s->layout,&s->view)!=CP64_OK||
     cp64_current(s->block,&s->layout,&s->view,&current)!=CP64_OK||
     current.sequence!=s->receipt.sequence+1)rc=TX64_RECOVERY;
 }
 if(rc==TX64_OK){s->receipt=current;s->ready=true;}
 else s->view.valid=false;
 s->busy=false;return rc==TX64_OK?TX64_OK:TX64_RECOVERY;
}
