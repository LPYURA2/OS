/* Host-only v2 disk image creator; never linked into the guest. */
#include <styleos/stylefs.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
struct medium { FILE *out; };
static bool read_disk(void *ctx, uint32_t lba, uint8_t dst[512]) {
    FILE *f = ((struct medium *)ctx)->out;
    return fseek(f,(long)lba*512L,SEEK_SET)==0 && fread(dst,1,512,f)==512;
}
static bool write_disk(void *ctx, uint32_t lba, const uint8_t src[512]) {
    FILE *f = ((struct medium *)ctx)->out;
    return fseek(f,(long)lba*512L,SEEK_SET)==0 && fwrite(src,1,512,f)==512;
}
static bool flush_disk(void *ctx) { return fflush(((struct medium *)ctx)->out)==0; }
int main(int argc,char **argv) {
    if (argc != 3) return 2;
    FILE *f = fopen(argv[1],"wb+"); if (!f) return 3;
    if (fseek(f,(long)SFS_V2_MIN_SECTORS*512L-1,SEEK_SET)!=0 ||
        fputc(0,f)==EOF || fflush(f)!=0) return 4;
    struct medium m={f};
    struct sfs_io io={&m,SFS_V2_MIN_SECTORS,read_disk,write_disk,flush_disk};
    static struct sfs_volume volume;
    static struct sfs_workspace work;
    if (sfs_format(&volume,&work,&io)!=SFS_OK ||
        sfs_mount(&volume,&work,&io)!=SFS_OK) return 5;
    const char *name=strcmp(argv[2],"data1")==0?"DATA1.TXT":"DATA2.TXT";
    unsigned id; const uint8_t marker[]={'d','a','t','a','-','p','r','o','b','e'};
    if (sfs_create(&volume,&work,SFS_ROOT,name,marker,sizeof(marker),&id)!=SFS_OK) return 6;
    if (fflush(f)!=0 || fclose(f)!=0) return 7;
    return 0;
}
