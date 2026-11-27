/* Same tested LBA48 guard; distinct tagged image and bounded ranges.
 * Terminal diagnostic only. No new hardware driver or ordinary mount. */
struct s38_workspace {
#ifdef STYLEOS_FILE_GUEST_45
 struct vm64_mount mount;
 struct fbk64_broker broker;
 struct pf64_service service;
 uint8_t elf[4608];
#else
 struct wv64_state volume;
#endif
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
#ifdef STYLEOS_FILE_GUEST_45
#define S38_VOLUME S38.mount.volume
#else
#define S38_VOLUME S38.volume
#endif
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
 if(wv64_stat(&S38_VOLUME,&S38.work,path,length,&f)!=WV64_OK||f.id!=id||f.bytes!=1041||f.revision!=1)return false;
 for(uint64_t off=0;off<f.bytes;){
  size_t done=0;
  if(wv64_read(&S38_VOLUME,&S38.work,path,length,id,1,off,buf,sizeof buf,&done)!=WV64_OK||!done)return false;
  for(size_t i=0;i<done;i++)if(buf[i]!=s38_byte(off+i))return false;
  off+=done;
 }
 return true;
}

#ifdef STYLEOS_FILE_GUEST_45
/* Immutable diagnostic ELF fixture built in kernel-owned memory, not disk
 * executable authority. Existing native loader still checks ELF and SHA. */
static void s45_put(size_t at,uint64_t v,unsigned n){
 for(unsigned i=0;i<n;i++)S38.elf[at+i]=(uint8_t)(v>>(8u*i));
}
static bool s45_reader(void *unused,uint32_t sector,uint8_t p[512]){
 (void)unused;if(sector>=9)return false;
 for(unsigned i=0;i<512;i++)p[i]=S38.elf[sector*512u+i];
 return true;
}
static size_t s45_elf(void){
 static const uint8_t code[]={
  0xcd,0x80,                  /* int 80: read file */
  0x48,0x89,0x42,0xc0,        /* [rdx-64] = return value */
  0x0f,0xb6,0x02,             /* eax = first byte read */
  0x48,0x89,0x42,0xc8,        /* [rdx-56] = observed byte */
  0xc6,0x42,0xd0,1,           /* [rdx-48] = completion marker */
  0xb8,1,0,0,0,0xcd,0x80,     /* yield; supervisor checks actual data */
  0xb8,2,0,0,0,0xcd,0x80,     /* exit */
  0x0f,0x0b
 };
 native_wipe(S38.elf,sizeof S38.elf);
 S38.elf[0]=127;S38.elf[1]='E';S38.elf[2]='L';S38.elf[3]='F';
 S38.elf[4]=2;S38.elf[5]=1;S38.elf[6]=1;
 s45_put(16,2,2);s45_put(18,62,2);s45_put(20,1,4);
 s45_put(24,S846_CODE,8);s45_put(32,64,8);
 s45_put(52,64,2);s45_put(54,56,2);s45_put(56,1,2);
 s45_put(64,1,4);s45_put(68,5,4);s45_put(72,4096,8);
 s45_put(80,S846_CODE,8);s45_put(88,S846_CODE,8);
 s45_put(96,sizeof code,8);s45_put(104,sizeof code,8);s45_put(112,4096,8);
 for(size_t i=0;i<sizeof code;i++)S38.elf[4096+i]=code[i];
 return 4096+sizeof code;
}
static void s45_run_apps(void){
 if(!native_ready||!native_audit(0)||pr_snapshot(pr_global(),0,0,true)||S38.broker.serial)fail("sfs45-start");
 S38.service=(struct pf64_service){pr_global(),&S38.broker};
 file_service45=&S38.service;file_mount45=&S38.mount;file_work45=&S38.work;
 struct rt865_launch_request req={.source={.read=s45_reader,.lba=0,.sectors=9},.name="file-read-test"};
 req.source.bytes=(uint32_t)s45_elf();
 nte_sha256(S38.elf,req.source.bytes,req.source.sha256);
 req.gpr[0]=FS43_CALL_READ;req.gpr[1]=1; /* verify issued token below */
 req.gpr[2]=511;req.gpr[3]=S846_DATA+64;req.gpr[4]=32;
 uint32_t pid[2]={0};
 for(unsigned i=0;i<2;i++)if(s865_native_launch(&req,&pid[i])!=PS_OK)fail("sfs45-launch");
 const struct pf64_identity id={pid[0],native_leases[0].generation,native_roots[0].built.root};
 uint64_t token=0;
 if(pf64_issue(&S38.service,&S38.mount,&S38.work,&id,(const uint8_t *)"/a",2,&token)!=WV64_OK||token!=1)fail("sfs45-grant");
 bool seen[2]={false,false};
 for(unsigned step=0;step<256&&pr_snapshot(pr_global(),0,0,true);step++){
  s865_native_pump();
  if(!native_audit(0))fail("sfs45-post-pump");
  for(unsigned i=0;i<2;i++){
   const struct pr_entry *p=pr_find(pr_global(),pid[i]);
   if(!p){if(!seen[i])fail("sfs45-exit-before-result");continue;}
   const uint8_t *page=(const uint8_t *)(uintptr_t)(native_leases[i].pa+PG_PAGE);
   if(seen[i]||page[16]!=1)continue;
   uint64_t want=i?UINT64_C(0)-(uint64_t)WV64_LOCKED:32;
   if(bytes64(page)!=want||bytes64(page+8)!=(i?0:s38_byte(511)))fail("sfs45-user-result");
   for(unsigned j=0;j<32;j++)if(page[64+j]!=(i?0:s38_byte(511+j)))fail("sfs45-user-data");
   for(unsigned j=17;j<64;j++)if(page[j])fail("sfs45-neighbor");
   if(page[96])fail("sfs45-neighbor-end");
   seen[i]=true;
   arch_puts(i?"STYLEOS S45 FOREIGN TOKEN DENIED\n":"STYLEOS S45 CPL3 READ VERIFIED\n");
  }
 }
 if(!seen[0]||!seen[1]||pr_snapshot(pr_global(),0,0,true)||
    fbk64_revoke(&S38.broker,pid[0],token)!=WV64_LOCKED||
    !native_audit(0))fail("sfs45-retirement");
 for(unsigned i=0;i<FBK64_SLOTS;i++)if(S38.broker.slots[i].token)fail("sfs45-grant-leak");
 arch_puts("STYLEOS S45 PROCESS CLEANUP VERIFIED\n");
 file_service45=0;file_mount45=0;file_work45=0;
 if(vm64_close(&S38.mount)!=WV64_OK)fail("sfs45-close");
}
#endif

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
#ifdef STYLEOS_FILE_GUEST_45
 if(bs_attach(svc,&backend,&gen)!=BS_OK)fail("sfs45-attach");
 struct vm64_geometry geo={gen,BLOCK_SHELL_OWNER,0,2,2+S38_DIR,S38_META,S38_DIR,S38_INO,S38_DATA,S38_START};
 struct ow64_inventory inv[2]={{S38.entries[0],S38.runs[0],32,0},{S38.entries[1],S38.runs[1],32,0}};
 enum wv64_status rc=vm64_open(&S38.mount,svc,&geo,true,S38.nodes,32,&S38.work,inv);
#else
 struct wv64_layout l={.owner=BLOCK_SHELL_OWNER,.dir_sectors=S38_DIR,.inode_sectors=S38_INO,.data_sectors=S38_DATA,.data_start=S38_START};
 if(bs_attach(svc,&backend,&gen)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,0,2,BS_READ|BS_WRITE,&l.receipt)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2,S38_DIR,BS_READ|BS_WRITE,&l.dir)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2+S38_DIR,S38_INO,BS_READ|BS_WRITE,&l.inode)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,S38_META,S38_DATA,BS_READ|BS_WRITE,&l.data)!=BS_OK)fail("sfs38-grant");
 struct ow64_inventory inv[2]={{S38.entries[0],S38.runs[0],32,0},{S38.entries[1],S38.runs[1],32,0}};
 enum wv64_status rc=wv64_open(&S38_VOLUME,svc,&l,true,S38.nodes,32,&S38.work,inv);
#endif
 if(rc!=WV64_OK)s20_end("SFS38 REFUSE volume");
 static const uint8_t a[]="/a",b[]="/d/b";
 if(S38_VOLUME.sequence==4){
  if(!s38_verify(a,2,2)||!s38_verify(b,4,3))fail("sfs38-cold-content");
  struct wv64_cursor cursor;unsigned count;
  if(wv64_list_open(&S38_VOLUME,&S38.work,(const uint8_t *)"/d",2,&cursor)!=WV64_OK||
     wv64_list_next(&S38_VOLUME,&S38.work,&cursor,S38.listing,&count)!=WV64_OK||count!=1||S38.listing[0].target!=3)fail("sfs38-list");
#ifdef STYLEOS_FILE_GUEST_45
  s45_run_apps();s863_shell_guard();s20_end("SFS45 COLD PASS");
#else
  s863_shell_guard();s20_end("SFS38 COLD PASS");
#endif
 }
 if(S38_VOLUME.sequence!=1||S38_VOLUME.dir.count)s20_end("SFS38 REFUSE volume");
 struct sd64_record file={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE};
 struct dc64_edit mkdir={.op=DC64_INSERT,.entry={.key={.parent=1,.length=1,.name={'d'}},.target=10,.kind=SD64_DIRECTORY}};
 struct sd64_record copy={.key={.parent=10,.length=1,.name={'b'}},.target=3,.kind=SD64_FILE};
 if(wv64_create(&S38_VOLUME,&S38.work,inv,&file,1041,s38_source,0)!=WV64_OK||
    wv64_edit(&S38_VOLUME,&S38.work,inv,&mkdir)!=WV64_OK||
    wv64_copy(&S38_VOLUME,&S38.work,inv,a,2,2,1,&copy,0,0)!=WV64_OK||
    S38_VOLUME.sequence!=4||!s38_verify(a,2,2)||!s38_verify(b,4,3))fail("sfs38-operations");
#ifdef STYLEOS_FILE_GUEST_45
 s45_run_apps();s863_shell_guard();s20_end("SFS45 WRITE PASS");
#else
 s863_shell_guard();s20_end("SFS38 WRITE PASS");
#endif
}
