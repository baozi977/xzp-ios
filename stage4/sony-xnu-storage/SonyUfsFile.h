// SPDX-License-Identifier: MIT
// Included after redwood_ufs globals: reuse the existing read-only BSD devices.
#include "SonyExtentRead.h"
static SONY_UFS_CONTEXT sony_layout;
static volatile SONY_UFS_CONTEXT *sony_shared;
static struct loader_block sony_surrogate;
static bool sony_offered,sony_active;
static int sony_physical_read(SU64 off,unsigned char *dst,SU64 len){return ufshc_read(0,off,dst,len);}
static int sony_file_read(uint32_t part,uint64_t off,uint8_t *dst,uint64_t len) {
 if(!sony_active || !ufshc_list_in())return ENXIO;
 int error=SonyExtentRead(&sony_layout,part,off,dst,len,sony_physical_read);
 return error<0?EINVAL:error;
}
static bool sony_prepare(uint64_t pa) {
 vm_offset_t va;SU32 i;
 if((pa&4095) || pa<0xa0200000ULL || pa+4096>0xa3fc0000ULL)return false;
 va=ml_io_map_unmappable(pa,4096,VM_WIMG_WCOMB);if(!va)return false;
 sony_shared=(volatile SONY_UFS_CONTEXT *)va;memcpy(&sony_layout,(const void *)va,sizeof(sony_layout));
 if(!SonyExtentValidate(&sony_layout) || sony_layout.Hc!=0x01da4000 || sony_layout.OldRun!=1 ||
    sony_layout.Area<0xa0200000 || sony_layout.AreaSize!=65536 || (sony_layout.Area&4095) ||
    sony_layout.Area+sony_layout.AreaSize>0xa3fc0000 ||
    (sony_layout.Area<pa+4096 && pa<sony_layout.Area+sony_layout.AreaSize) ||
    sony_layout.UcdOffset!=1024 || sony_layout.UcdSize!=1152 || sony_layout.Slots!=32 ||
    sony_layout.PartitionFirst!=270601 || sony_layout.PartitionBlocks!=13296370 || sony_layout.DiskBlocks!=13566976 ||
    sony_layout.File[0].Bytes!=65536 || strcmp(sony_layout.File[0].Name,"fixture") || !sony_layout.File[0].ProofFnv)return false;
 memset(&sony_surrogate,0,sizeof(sony_surrogate));
 sony_surrogate.magic=BLOCK_MAGIC;sony_surrogate.version=BLOCK_VERSION;sony_surrogate.size=sizeof(sony_surrogate);
 sony_surrogate.parts=sony_layout.FileCount;sony_surrogate.buffer_size=65536;
 for(i=0;i<sony_layout.FileCount;i++) {
  strlcpy(sony_surrogate.part[i].name,sony_layout.File[i].Name,sizeof(sony_surrogate.part[i].name));
  sony_surrogate.part[i].block_size=4096;sony_surrogate.part[i].blocks=sony_layout.File[i].Bytes/4096;
  sony_surrogate.part[i].unit=0;sony_surrogate.part[i].lun=0;
 }
 ufs_page=&sony_surrogate;ufs_parts=sony_layout.FileCount;ufs_buffer_size=65536;ufs_buffer=NULL;
 sony_offered=true;
 printf("SONY_UFS_CONTEXT accepted pa=%llx files=%u extents=%u readonly=1\n",pa,sony_layout.FileCount,sony_layout.ExtentCount);
 return true;
}
static bool sony_start_native(void) {
 struct ufshc_loader offer={
  .hc_pa=sony_layout.Hc,.area_pa=sony_layout.Area,.area_size=sony_layout.AreaSize,
  .ucd_offset=sony_layout.UcdOffset,.ucd_size=sony_layout.UcdSize,.slots=sony_layout.Slots,
  .uefi_lo=sony_layout.OldLo,.uefi_hi=sony_layout.OldHi,
  .take_back=&sony_shared->TakeBack,.list_in=&sony_shared->ListIn,
  .switched_in=(uint32_t *)&sony_shared->SwitchIn,.switched_out=(uint32_t *)&sony_shared->SwitchOut,
  .cnr_at_switch=&sony_shared->Cnr};
 uint8_t *buf;uint64_t off=0,h=0xcbf29ce484222325ULL;int error=0;
 ufshc_set_luns(1);
 if(!ufshc_probe(&offer,false) || ufshc_switch_in(true)!=0)return false;
 sony_active=true;buf=kalloc_data(16384,Z_WAITOK);if(!buf)error=ENOMEM;
 while(!error && off<65536) {
  error=sony_file_read(0,off,buf,16384);
  if(!error)for(unsigned i=0;i<16384;i++){h^=buf[i];h*=0x100000001b3ULL;}
  off+=16384;
 }
 if(buf)kfree_data(buf,16384);
 if(error || h!=sony_layout.File[0].ProofFnv){
  printf("SONY_UFS_KERNEL_PROOF failed error=%d bytes=%llu fnv=%llx expected=%llx\n",error,off,h,sony_layout.File[0].ProofFnv);
  sony_active=false;ufshc_switch_out("Sony project-file proof failed");return false;
 }
 printf("SONY_UFS_KERNEL_PROOF_PASS bytes=65536 fnv=%llx\n",h);
 printf("SONY_UFS_KERNEL_READY files=%u readonly=1 mailbox=0\n",sony_layout.FileCount);
 return true;
}
