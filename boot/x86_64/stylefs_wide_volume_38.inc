/* Same tested LBA48 guard; distinct tagged image and bounded ranges.
 * Terminal diagnostic only. No new hardware driver or ordinary mount. */
struct s38_workspace {
 struct wv64_state volume;
 struct wv64_workspace work;
 struct ns64_node nodes[32];
 struct ow64_extent entries[2][32];
 struct sfs64_run runs[2][32];
 struct sd64_record listing[SD64_FANOUT];
};
static _Alignas(4096) union {
 struct s38_workspace value;
 uint8_t pages[S20_PAGE_ROUND(sizeof(struct s38_workspace))];
} s38_storage;
_Static_assert(sizeof s38_storage%4096==0,"whole pages");
#define S38 s38_storage.value
static _Noreturn void s20_end(const char *s){
 arch_puts(s);arch_putc('\n');arch_halt();for(;;)__asm__ volatile("hlt");
}
static uint8_t s38_byte(uint64_t i){return (uint8_t)(i*37u+i/512u*91u+13u);}
static bool s38_source(void *ctx,uint64_t off,uint8_t *p,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)p[i]=s38_byte(off+i);
 return true;
}
static bool s38_verify(const uint8_t *path,size_t length,uint64_t id){
 struct sfs64_file f;uint8_t buf[512];
 if(wv64_stat(&S38.volume,&S38.work,path,length,&f)!=WV64_OK||f.id!=id||f.bytes!=1041||f.revision!=1)return false;
 for(uint64_t off=0;off<f.bytes;){
  size_t done=0;
  if(wv64_read(&S38.volume,&S38.work,path,length,id,1,off,buf,sizeof buf,&done)!=WV64_OK||!done)return false;
  for(size_t i=0;i<done;i++)if(buf[i]!=s38_byte(off+i))return false;
  off+=done;
 }
 return true;
}
static _Noreturn void s20_run(void){
 s863_shell_guard();
 struct s20_guard g={.live=&disk863.s.live};
 uint8_t buf[512];uint64_t gen=0;
 if(!s20_begin(&g,S20_IDENT,0)||!ata48_bind(&g.ata,&s20_ops,&g,S20_SERIAL,S20_N))s20_end("SFS38 REFUSE identity");
 if(!s20_begin(&g,S20_READ,0)||!ata48_read(&g.ata,0,buf))fail("sfs38-tag-read");
 static const char tag[]=S20_TAG;
 for(unsigned i=0;i<512;i++)if(buf[i]!=(i<sizeof(tag)-1?(uint8_t)tag[i]:0))s20_end("SFS38 REFUSE tag");
 g.tagged=true;
 struct bs_service *svc=&disk863.s.block;
 const struct bs_backend backend={&g,UINT64_C(0x701080862838),S20_CAP,true,false,s20_read,s20_write,s20_flush};
 struct wv64_layout l={.owner=BLOCK_SHELL_OWNER,.dir_sectors=S38_DIR,.inode_sectors=S38_INO,.data_sectors=S38_DATA,.data_start=S38_START};
 if(bs_attach(svc,&backend,&gen)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,0,2,BS_READ|BS_WRITE,&l.receipt)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2,S38_DIR,BS_READ|BS_WRITE,&l.dir)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2+S38_DIR,S38_INO,BS_READ|BS_WRITE,&l.inode)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,S38_META,S38_DATA,BS_READ|BS_WRITE,&l.data)!=BS_OK)fail("sfs38-grant");
 struct ow64_inventory inv[2]={{S38.entries[0],S38.runs[0],32,0},{S38.entries[1],S38.runs[1],32,0}};
 enum wv64_status rc=wv64_open(&S38.volume,svc,&l,true,S38.nodes,32,&S38.work,inv);
 if(rc!=WV64_OK)s20_end("SFS38 REFUSE volume");
 static const uint8_t a[]="/a",b[]="/d/b";
 if(S38.volume.sequence==4){
  if(!s38_verify(a,2,2)||!s38_verify(b,4,3))fail("sfs38-cold-content");
  struct wv64_cursor cursor;unsigned count;
  if(wv64_list_open(&S38.volume,&S38.work,(const uint8_t *)"/d",2,&cursor)!=WV64_OK||
     wv64_list_next(&S38.volume,&S38.work,&cursor,S38.listing,&count)!=WV64_OK||count!=1||S38.listing[0].target!=3)fail("sfs38-list");
  s863_shell_guard();s20_end("SFS38 COLD PASS");
 }
 if(S38.volume.sequence!=1||S38.volume.dir.count)s20_end("SFS38 REFUSE volume");
 struct sd64_record file={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE};
 struct dc64_edit mkdir={.op=DC64_INSERT,.entry={.key={.parent=1,.length=1,.name={'d'}},.target=10,.kind=SD64_DIRECTORY}};
 struct sd64_record copy={.key={.parent=10,.length=1,.name={'b'}},.target=3,.kind=SD64_FILE};
 if(wv64_create(&S38.volume,&S38.work,inv,&file,1041,s38_source,0)!=WV64_OK||
    wv64_edit(&S38.volume,&S38.work,inv,&mkdir)!=WV64_OK||
    wv64_copy(&S38.volume,&S38.work,inv,a,2,2,1,&copy,0,0)!=WV64_OK||
    S38.volume.sequence!=4||!s38_verify(a,2,2)||!s38_verify(b,4,3))fail("sfs38-operations");
 s863_shell_guard();s20_end("SFS38 WRITE PASS");
}
