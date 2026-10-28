// SPDX-License-Identifier: MIT
// Cold-boot GIC preparation matching Sony Linux's distributor-before-redistributor order.
#ifndef GIC_STAGE_TAG
#error Set GIC_STAGE_TAG to the experiment revision
#endif
STATIC UINT64 BootGicD,BootGicR,BootGicFrequency;
STATIC UINT32 BootTimerId;
typedef struct {
  UINT32 Ctrl,Groups,Mod,Enabled,Config,Pending,Spi[32],Words;
  UINT8 Priority;
  UINT64 TimerCtl,Cval,CpuGroup,Pmr,Apr0,CpuCtlr;
} GIC_BOOT_SAVED;
STATIC UINT32 BootGicRead(UINT64 A) { return *(volatile UINT32 *)(UINTN)A; }
STATIC VOID BootGicWrite(UINT64 A,UINT32 V) { *(volatile UINT32 *)(UINTN)A=V;__asm__ volatile("dsb sy":::"memory"); }
STATIC BOOLEAN BootGicWait(UINT64 A,UINT32 Mask) {
  UINT64 Start,Now;__asm__ volatile("mrs %0,cntvct_el0":"=r"(Start));
  do { if (!(BootGicRead(A)&Mask)) return TRUE;__asm__ volatile("mrs %0,cntvct_el0":"=r"(Now)); } while(Now-Start<BootGicFrequency/50);
  return FALSE;
}
STATIC VOID BootTimerOff(VOID) { __asm__ volatile("msr cntv_ctl_el0,%0\nisb"::"r"((UINT64)2):"memory"); }
STATIC BOOLEAN BootGicPulse(UINT32 Attempt,UINT64 CpuCtlr) {
  UINT64 Start,Now,Compare,Ctl=0,Hppir=1023,Ack=0xffffffff;UINT32 Mask=1U<<BootTimerId;
  BootTimerOff();BootGicWrite(BootGicR+0x10280,Mask);
  __asm__ volatile("mrs %0,cntvct_el0":"=r"(Start));Compare=Start+BootGicFrequency/1000;
  __asm__ volatile("msr cntv_cval_el0,%0\nmsr cntv_ctl_el0,%1\nisb"::"r"(Compare),"r"((UINT64)1):"memory");
  do {
    __asm__ volatile("mrs %0,cntv_ctl_el0":"=r"(Ctl));
    __asm__ volatile("mrs %0,S3_0_C12_C12_2":"=r"(Hppir));Hppir&=0xffffff;
    __asm__ volatile("mrs %0,cntvct_el0":"=r"(Now));
    if ((Ctl&4)&&Hppir==BootTimerId) break;
  } while(Now-Start<BootGicFrequency/10);
  if ((Ctl&4)&&Hppir==BootTimerId) {
    // Acknowledge only after HPPIR identifies the timer owned by this test.
    __asm__ volatile("mrs %0,S3_0_C12_C12_0":"=r"(Ack));
    BootTimerOff();
    if ((Ack&0xffffff)<1020) {
      __asm__ volatile("msr S3_0_C12_C12_1,%0\nisb"::"r"(Ack):"memory");
      if (CpuCtlr&2) __asm__ volatile("msr S3_0_C12_C11_1,%0\nisb"::"r"(Ack):"memory");
    }
  } else BootTimerOff();
  BootGicWrite(BootGicR+0x10280,Mask);
  DEBUG((DEBUG_INFO,"S2_GIC_GATE" GIC_STAGE_TAG " attempt=%u expected=%u expired=%u hppir=%lu iar=%lu ctrl=%x enabled=%x cfg1=%x pending=%x\n",Attempt,BootTimerId,(UINT32)((Ctl&4)!=0),Hppir,Ack&0xffffff,BootGicRead(BootGicD),BootGicRead(BootGicR+0x10100),BootGicRead(BootGicR+0x10c04),BootGicRead(BootGicR+0x10200)));
  return (Ctl&4)&&Hppir==BootTimerId&&(Ack&0xffffff)==BootTimerId;
}
STATIC EFI_STATUS PrepareGicForKernel(VOID) {
  GIC_BOOT_SAVED S;UINT64 Midr,Daif,Sre;UINT32 Mask,Shift,Base,I,Attempt,Config,Nonzero=0;BOOLEAN Good=FALSE;
  __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
  if ((Midr&0xff00fff0)==0x4100d080) {BootGicD=0x08000000;BootGicR=0x080a0000;BootTimerId=27;}
  else if(Midr==0x51af8014) {BootGicD=0x17a00000;BootGicR=0x17b00000;BootTimerId=19;}
  else return EFI_UNSUPPORTED;
  __asm__ volatile("mrs %0,daif":"=r"(Daif));__asm__ volatile("mrs %0,S3_0_C12_C12_5":"=r"(Sre));
  __asm__ volatile("mrs %0,cntfrq_el0":"=r"(BootGicFrequency));
  if ((Daif&0xc0)!=0xc0||!(Sre&1)||BootGicFrequency<100000) return EFI_UNSUPPORTED;
  Mask=1U<<BootTimerId;Shift=(BootTimerId-16)*2;ZeroMem(&S,sizeof(S));
  S.Ctrl=BootGicRead(BootGicD);S.Groups=BootGicRead(BootGicR+0x10080);S.Mod=BootGicRead(BootGicR+0x10d00);S.Enabled=BootGicRead(BootGicR+0x10100);S.Config=BootGicRead(BootGicR+0x10c04);S.Pending=BootGicRead(BootGicR+0x10200);S.Priority=*(volatile UINT8 *)(UINTN)(BootGicR+0x10400+BootTimerId);
  S.Words=(BootGicRead(BootGicD+4)&31)+1;
  __asm__ volatile("mrs %0,cntv_ctl_el0":"=r"(S.TimerCtl));__asm__ volatile("mrs %0,cntv_cval_el0":"=r"(S.Cval));
  __asm__ volatile("mrs %0,S3_0_C12_C12_7":"=r"(S.CpuGroup));__asm__ volatile("mrs %0,S3_0_C4_C6_0":"=r"(S.Pmr));
  __asm__ volatile("mrs %0,S3_0_C12_C9_0":"=r"(S.Apr0));__asm__ volatile("mrs %0,S3_0_C12_C12_4":"=r"(S.CpuCtlr));
  for(I=1;I<S.Words;I++) {S.Spi[I]=BootGicRead(BootGicD+0x100+4*I);if(S.Spi[I])Nonzero++;}
  DEBUG((DEBUG_INFO,"S2_GIC_INIT" GIC_STAGE_TAG " ctrl_old=%x timer=%u cfg_old=%x enabled_old=%x pending_old=%x spi_words=%u spi_nonzero=%u apr0=%lx\n",S.Ctrl,BootTimerId,S.Config,S.Enabled,S.Pending,S.Words,Nonzero,S.Apr0));
  if(S.Pending&Mask) return EFI_NOT_READY;
  BootTimerOff();Base=S.Ctrl&~0x80000013U;
  BootGicWrite(BootGicD,S.Ctrl&~3U);if(!BootGicWait(BootGicD,0x80000000))goto Restore;
  BootGicWrite(BootGicD,Base|0x10);if(!BootGicWait(BootGicD,0x80000000))goto Restore;
  // Quiesce non-secure SPIs, as Linux does before enabling the distributor.
  for(I=1;I<S.Words;I++)BootGicWrite(BootGicD+0x180+4*I,0xffffffff);
  if(!BootGicWait(BootGicD,0x80000000))goto Restore;
  BootGicWrite(BootGicD,Base|0x13);if(!BootGicWait(BootGicD,0x80000000))goto Restore;
  if((BootGicRead(BootGicD)&0x12)!=0x12)goto Restore;
  // Redistributor writes now occur after affinity routing has become active.
  BootGicWrite(BootGicR+0x10180,0xffffffff);if(!BootGicWait(BootGicR,8))goto Restore;
  BootGicWrite(BootGicR+0x10080,S.Groups|Mask);BootGicWrite(BootGicR+0x10d00,S.Mod&~Mask);
  *(volatile UINT8 *)(UINTN)(BootGicR+0x10400+BootTimerId)=0xa0;
  __asm__ volatile("msr S3_0_C12_C9_0,%0\nmsr S3_0_C4_C6_0,%1\nmsr S3_0_C12_C12_7,%2\nisb"::"r"((UINT64)0),"r"((UINT64)0xff),"r"((UINT64)1):"memory");
  for(Attempt=0;Attempt<2;Attempt++) {
    BootGicWrite(BootGicR+0x10180,Mask);if(!BootGicWait(BootGicR,8))break;
    Config=S.Config;if(Attempt)Config^=2U<<Shift;
    BootGicWrite(BootGicR+0x10c04,Config);BootGicWrite(BootGicR+0x10100,Mask);if(!BootGicWait(BootGicR,8))break;
    if(!(BootGicRead(BootGicR+0x10100)&Mask))break;
    if(BootGicPulse(Attempt,S.CpuCtlr)) {Good=TRUE;break;}
  }
Restore:
  BootTimerOff();BootGicWrite(BootGicR+0x10280,Mask);
  __asm__ volatile("msr cntv_cval_el0,%0\nmsr cntv_ctl_el0,%1\nmsr S3_0_C4_C6_0,%2\nmsr S3_0_C12_C12_7,%3\nisb"::"r"(S.Cval),"r"(S.TimerCtl),"r"(S.Pmr),"r"(S.CpuGroup):"memory");
  if(!Good) {
    BootGicWrite(BootGicR+0x10180,0xffffffff);BootGicWait(BootGicR,8);
    BootGicWrite(BootGicR+0x10080,S.Groups);BootGicWrite(BootGicR+0x10d00,S.Mod);BootGicWrite(BootGicR+0x10c04,S.Config);
    *(volatile UINT8 *)(UINTN)(BootGicR+0x10400+BootTimerId)=S.Priority;
    if(S.Enabled)BootGicWrite(BootGicR+0x10100,S.Enabled);
    for(I=1;I<S.Words;I++)if(S.Spi[I])BootGicWrite(BootGicD+0x100+4*I,S.Spi[I]);
    BootGicWrite(BootGicD,BootGicRead(BootGicD)&~3U);BootGicWait(BootGicD,0x80000000);BootGicWrite(BootGicD,S.Ctrl&~3U);BootGicWait(BootGicD,0x80000000);BootGicWrite(BootGicD,S.Ctrl);BootGicWait(BootGicD,0x80000000);
    __asm__ volatile("msr S3_0_C12_C9_0,%0\nisb"::"r"(S.Apr0):"memory");
    DEBUG((DEBUG_ERROR,"S2_GIC_GATE_FAILED" GIC_STAGE_TAG " ctrl=%x enabled=%x pending=%x cfg1=%x\n",BootGicRead(BootGicD),BootGicRead(BootGicR+0x10100),BootGicRead(BootGicR+0x10200),BootGicRead(BootGicR+0x10c04)));
    return EFI_DEVICE_ERROR;
  }
  DEBUG((DEBUG_INFO,"S2_GIC_READY" GIC_STAGE_TAG " timer=%u ctrl=%x enabled=%x pending=%x cfg1=%x\n",BootTimerId,BootGicRead(BootGicD),BootGicRead(BootGicR+0x10100),BootGicRead(BootGicR+0x10200),BootGicRead(BootGicR+0x10c04)));
  return EFI_SUCCESS;
}
