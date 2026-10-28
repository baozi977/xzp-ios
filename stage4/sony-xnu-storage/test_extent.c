// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include "SonyExtentRead.h"
static unsigned char Disk[16*4096],Logical[3*4096],Got[3*4096];
static int calls,fail;
static int read_disk(SU64 off,unsigned char *to,SU64 len){calls++;if(fail)return 77;if(off+len>sizeof(Disk))return 78;memcpy(to,Disk+off,len);return 0;}
int main(void){
 SONY_UFS_CONTEXT C={0};SU64 i;C.Magic=SONY_UFS_MAGIC;C.Version=1;C.Size=sizeof(C);
 C.FileCount=1;C.ExtentCount=2;C.PartitionFirst=1;C.PartitionBlocks=15;C.DiskBlocks=16;
 C.File[0].Bytes=sizeof(Logical);C.File[0].Count=2;strcpy(C.File[0].Name,"fixture");
 C.Extent[0]=(SONY_UFS_EXTENT){0,2,1};C.Extent[1]=(SONY_UFS_EXTENT){4096,9,2};
 for(i=0;i<sizeof(Logical);i++)Logical[i]=(i*31+(i>>8))&255;
 memcpy(Disk+2*4096,Logical,4096);memcpy(Disk+9*4096,Logical+4096,8192);
 if(!SonyExtentValidate(&C))return 1;
 if(SonyExtentRead(&C,0,1001,Got,9999,read_disk)||memcmp(Got,Logical+1001,9999)||calls!=2)return 2;
 if(SonyExtentRead(&C,0,sizeof(Logical)-2,Got,3,read_disk)!=-1)return 3;
 fail=1;if(SonyExtentRead(&C,0,0,Got,5,read_disk)!=77)return 4;fail=0;
 C.Extent[1].Logical++;if(SonyExtentValidate(&C))return 5;C.Extent[1].Logical--;
 C.Extent[1].Lba=15;if(SonyExtentValidate(&C))return 6;
 puts("SONY_EXTENT_TEST_PASS scatter/unaligned/bounds/error/gap/partition");return 0;
}
