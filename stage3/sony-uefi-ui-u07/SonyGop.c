// SPDX-License-Identifier: MIT
// Sony measured ABL BGR888 surface, exported as BLT-only GOP.
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/SerialPortLib.h>

#define FB_BASE 0x9d400000UL
#define WIDTH 1080U
#define HEIGHT 1920U
#define STRIDE 3240U
#define FB_BYTES (STRIDE*HEIGHT)
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo;
STATIC EFI_HANDLE mHandle;
STATIC EFI_EVENT mRefresh;
STATIC UINT8 *mSaved;
STATIC UINTN mDirtyFirst=FB_BYTES,mDirtyEnd;
STATIC BOOLEAN mNative,mRefreshBusy;
STATIC UINT32 mOriginalCrc,mKicks,mTimeouts,mBlts;

STATIC VOID CleanSurface(UINTN First,UINTN End) {
  UINTN P;UINT64 Ctr,Line;
  __asm__ volatile("mrs %0,ctr_el0":"=r"(Ctr));Line=4ULL<<((Ctr>>16)&15);
  for(P=(FB_BASE+First)&~(Line-1);P<FB_BASE+End;P+=Line) __asm__ volatile("dc cvac,%0"::"r"(P):"memory");
  __asm__ volatile("dsb sy":::"memory");
}

#define PP0 0x0c971000UL
#define PP1 0x0c971800UL
#define MDP_STATUS 0x0c901014UL
#define MDP_CLEAR 0x0c901018UL
#define PP_DONE 0x300U
STATIC UINT32 mSavedTe[2],mSavedAr[2],mCompleted;
STATIC BOOLEAN mOwnRefresh,mTransferFailed;

STATIC VOID TracePipe(CONST CHAR8 *Phase) {
  DEBUG((DEBUG_INFO,"UI_PIPE_U07 %a te0=%x te1=%x ar0=%x ar1=%x h0=%x h1=%x line0=%x line1=%x irq=%x en=%x\n",Phase,
    MmioRead32(PP0+4),MmioRead32(PP1+4),MmioRead32(PP0+0x30),MmioRead32(PP1+0x30),
    MmioRead32(PP0+8),MmioRead32(PP1+8),MmioRead32(PP0+0x2c),MmioRead32(PP1+0x2c),
    MmioRead32(MDP_STATUS),MmioRead32(0x0c901010)));
}

STATIC BOOLEAN WaitIdle(VOID) {
  UINTN I;UINT32 A,B;
  for(I=0;I<10000;I++) {
    A=MmioRead32(PP0+0x2c);B=MmioRead32(PP1+0x2c);
    // Linux wait4_autorefresh_pp waits only for 0 < line_count < roi.h.
    if((!A || A>=HEIGHT) && (!B || B>=HEIGHT))return TRUE;
    MicroSecondDelay(10);
  }
  return FALSE;
}

STATIC EFI_STATUS TakeRefresh(VOID) {
  mSavedTe[0]=MmioRead32(PP0+4);mSavedTe[1]=MmioRead32(PP1+4);
  mSavedAr[0]=MmioRead32(PP0+0x30);mSavedAr[1]=MmioRead32(PP1+0x30);
  TracePipe("before");
  MmioWrite32(PP0+4,mSavedTe[0]&~(1U<<20));MmioWrite32(PP1+4,mSavedTe[1]&~(1U<<20));
  __asm__ volatile("dsb sy":::"memory");
  if(!WaitIdle()) {
    TracePipe("handoff-timeout");MmioWrite32(PP0+4,mSavedTe[0]);MmioWrite32(PP1+4,mSavedTe[1]);
    return EFI_TIMEOUT;
  }
  MmioWrite32(PP0+0x30,0);MmioWrite32(PP1+0x30,0);__asm__ volatile("dsb sy":::"memory");
  mOwnRefresh=TRUE;
  MmioWrite32(PP0+4,mSavedTe[0]);MmioWrite32(PP1+4,mSavedTe[1]);__asm__ volatile("dsb sy":::"memory");
  TracePipe("software-mode");
  if(MmioRead32(PP0+0x30) || MmioRead32(PP1+0x30))return EFI_DEVICE_ERROR;
  DEBUG((DEBUG_INFO,"UI_REFRESH_HANDOFF_U07 status=Success\n"));return EFI_SUCCESS;
}

STATIC BOOLEAN RestoreRefresh(VOID) {
  BOOLEAN Idle,Equal;
  if(!mOwnRefresh)return TRUE;
  MmioWrite32(PP0+4,mSavedTe[0]&~(1U<<20));MmioWrite32(PP1+4,mSavedTe[1]&~(1U<<20));
  __asm__ volatile("dsb sy":::"memory");Idle=WaitIdle();
  if(Idle){MmioWrite32(PP0+0x30,mSavedAr[0]);MmioWrite32(PP1+0x30,mSavedAr[1]);}
  MmioWrite32(PP0+4,mSavedTe[0]);MmioWrite32(PP1+4,mSavedTe[1]);__asm__ volatile("dsb sy":::"memory");
  TracePipe("restore");
  Equal=Idle && MmioRead32(PP0+4)==mSavedTe[0] && MmioRead32(PP1+4)==mSavedTe[1] &&
    MmioRead32(PP0+0x30)==mSavedAr[0] && MmioRead32(PP1+0x30)==mSavedAr[1];
  DEBUG((DEBUG_INFO,"UI_REFRESH_RESTORE_U07 idle=%u equal=%u\n",Idle,Equal));
  if(Equal)mOwnRefresh=FALSE;
  return Equal;
}

STATIC VOID Present(VOID) {
  UINTN First,End,I;UINT32 Irq=0,Seen=0,Cleared=0;
  if(mRefreshBusy || mDirtyEnd<=mDirtyFirst || (mNative && mTransferFailed))return;
  mRefreshBusy=TRUE;First=mDirtyFirst;End=mDirtyEnd;mDirtyFirst=FB_BYTES;mDirtyEnd=0;
  CleanSurface(First,End);
  if(mNative) {
    if(!WaitIdle()){mTransferFailed=TRUE;mTimeouts++;TracePipe("pre-frame-busy");mRefreshBusy=FALSE;return;}
    MmioWrite32(MDP_CLEAR,PP_DONE);__asm__ volatile("dsb sy":::"memory");Cleared=MmioRead32(MDP_STATUS)&PP_DONE;
    // Current command-mode route uses separate flushes; trigger the master.
    MmioWrite32(0x0c902018,0x20001);MmioWrite32(0x0c902218,0x20002);__asm__ volatile("dsb sy":::"memory");
    MmioWrite32(0x0c90201c,1);__asm__ volatile("dsb sy":::"memory");
    for(I=0;I<10000;I++) {Irq=MmioRead32(MDP_STATUS);Seen|=Irq&PP_DONE;if(Seen==PP_DONE)break;MicroSecondDelay(10);}
    if(!Cleared && Seen==PP_DONE)mCompleted++;
    else {mTimeouts++;mTransferFailed=TRUE;}
    DEBUG((DEBUG_INFO,"UI_FRAME_U07 submitted=%u cleared=%x seen=%x irq=%x completed=%u line0=%x line1=%x timeout=%u\n",mKicks+1,Cleared,Seen,Irq,mCompleted,MmioRead32(PP0+0x2c),MmioRead32(PP1+0x2c),mTransferFailed));
  }
  mKicks++;mRefreshBusy=FALSE;
}

BOOLEAN SonyGopHealthy(VOID) {return !mTransferFailed;}

STATIC VOID EFIAPI Refresh(EFI_EVENT Event,VOID *Context) {(VOID)Event;(VOID)Context;Present();}

STATIC EFI_STATUS EFIAPI Query(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,UINT32 Number,UINTN *Size,EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info) {
  if(This!=&mGop || Number || !Size || !Info)return EFI_INVALID_PARAMETER;
  *Info=AllocateCopyPool(sizeof(mInfo),&mInfo);if(!*Info)return EFI_OUT_OF_RESOURCES;*Size=sizeof(mInfo);return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Set(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,UINT32 Number) {return This==&mGop && !Number?EFI_SUCCESS:EFI_UNSUPPORTED;}

STATIC EFI_STATUS EFIAPI Blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer,EFI_GRAPHICS_OUTPUT_BLT_OPERATION Op,UINTN Sx,UINTN Sy,UINTN Dx,UINTN Dy,UINTN W,UINTN H,UINTN Delta) {
  UINTN X,Y,Row;EFI_TPL Old;UINT8 *V=(UINT8 *)FB_BASE;
  if(This!=&mGop || Op>=EfiGraphicsOutputBltOperationMax || !W || !H)return EFI_INVALID_PARAMETER;
  if(Op==EfiBltVideoToBltBuffer) {if(Sx>=WIDTH || Sy>=HEIGHT || W>WIDTH-Sx || H>HEIGHT-Sy)return EFI_INVALID_PARAMETER;}
  else if(Dx>=WIDTH || Dy>=HEIGHT || W>WIDTH-Dx || H>HEIGHT-Dy)return EFI_INVALID_PARAMETER;
  if(Op!=EfiBltVideoToVideo && !Buffer)return EFI_INVALID_PARAMETER;
  if(Op==EfiBltVideoToVideo && (Sx>=WIDTH || Sy>=HEIGHT || W>WIDTH-Sx || H>HEIGHT-Sy))return EFI_INVALID_PARAMETER;
  if(!Delta)Delta=W*sizeof(*Buffer);
  if((Op==EfiBltBufferToVideo && Delta<(Sx+W)*sizeof(*Buffer)) || (Op==EfiBltVideoToBltBuffer && Delta<(Dx+W)*sizeof(*Buffer)))return EFI_INVALID_PARAMETER;
  Old=gBS->RaiseTPL(TPL_NOTIFY);
  for(Y=0;Y<H;Y++) {
    Row=(Op==EfiBltVideoToVideo && Dy>Sy)?H-1-Y:Y;
    if(Op==EfiBltVideoToVideo) {CopyMem(V+(Dy+Row)*STRIDE+Dx*3,V+(Sy+Row)*STRIDE+Sx*3,W*3);continue;}
    for(X=0;X<W;X++) {
      UINT8 *P;EFI_GRAPHICS_OUTPUT_BLT_PIXEL *B;
      if(Op==EfiBltVideoToBltBuffer) {P=V+(Sy+Y)*STRIDE+(Sx+X)*3;B=(VOID *)((UINT8 *)Buffer+(Dy+Y)*Delta+(Dx+X)*4);B->Blue=P[0];B->Green=P[1];B->Red=P[2];B->Reserved=0;}
      else {P=V+(Dy+Y)*STRIDE+(Dx+X)*3;B=Op==EfiBltVideoFill?Buffer:(VOID *)((UINT8 *)Buffer+(Sy+Y)*Delta+(Sx+X)*4);P[0]=B->Blue;P[1]=B->Green;P[2]=B->Red;}
    }
  }
  if(Op!=EfiBltVideoToBltBuffer) {mDirtyFirst=MIN(mDirtyFirst,Dy*STRIDE+Dx*3);mDirtyEnd=MAX(mDirtyEnd,(Dy+H-1)*STRIDE+(Dx+W)*3);}
  mBlts++;gBS->RestoreTPL(Old);return EFI_SUCCESS;
}


STATIC VOID DumpOriginal(VOID) {
  STATIC CONST CHAR8 Alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  UINTN X,Y,I=0,N=0;UINT8 Triple[3];CHAR8 Out[4];CHAR8 *Encoded;UINT8 *P;
  STATIC UINT8 Begin[]="UI_ORIGINAL_GRAY_U07_BEGIN width=270 height=480 bytes=172800\n";
  STATIC UINT8 End[]="\nUI_ORIGINAL_GRAY_U07_END\n";
  if(!mNative)return;
  Encoded=AllocatePool(172800);if(!Encoded)return;
  SerialPortWrite(Begin,sizeof(Begin)-1);
  for(Y=0;Y<1920;Y+=4)for(X=0;X<1080;X+=4) {
    P=mSaved+Y*STRIDE+X*3;Triple[I++]=(UINT8)(((UINTN)P[0]+P[1]+P[2])/3);
    if(I==3){Out[0]=Alphabet[Triple[0]>>2];Out[1]=Alphabet[((Triple[0]&3)<<4)|(Triple[1]>>4)];Out[2]=Alphabet[((Triple[1]&15)<<2)|(Triple[2]>>6)];Out[3]=Alphabet[Triple[2]&63];CopyMem(Encoded+N,Out,4);N+=4;I=0;}
  }
  ASSERT(I==0 && N==172800);SerialPortWrite((UINT8 *)Encoded,N);FreePool(Encoded);SerialPortWrite(End,sizeof(End)-1);
}

EFI_STATUS SonyGopStart(BOOLEAN Native) {
  EFI_STATUS Status;UINT32 F0,F1;
  mNative=Native;
  if(Native) {
    F0=MmioRead32(0x0c905030);F1=MmioRead32(0x0c907030);
    DEBUG((DEBUG_INFO,"UI_GOP_CONTRACT_U07 hw=%x f0=%x f1=%x a0=%x a1=%x stride0=%x stride1=%x\n",MmioRead32(0x0c900000),F0,F1,MmioRead32(0x0c905014),MmioRead32(0x0c907014),MmioRead32(0x0c905024),MmioRead32(0x0c907024)));
    if(MmioRead32(0x0c900000)!=0x30000001 || F0!=0x2243f || F1!=0x2243f || MmioRead32(0x0c905014)!=FB_BASE || MmioRead32(0x0c907014)!=FB_BASE || (MmioRead32(0x0c905024)&0xffff)!=STRIDE || (MmioRead32(0x0c907024)&0xffff)!=STRIDE)return EFI_UNSUPPORTED;
  }
  mSaved=AllocateCopyPool(FB_BYTES,(VOID *)FB_BASE);if(!mSaved)return EFI_OUT_OF_RESOURCES;
  gBS->CalculateCrc32(mSaved,FB_BYTES,&mOriginalCrc);
  if(Native) {
    Status=TakeRefresh();
    if(EFI_ERROR(Status)){RestoreRefresh();FreePool(mSaved);mSaved=NULL;return Status;}
  }
  ZeroMem(&mInfo,sizeof(mInfo));mInfo.HorizontalResolution=WIDTH;mInfo.VerticalResolution=HEIGHT;mInfo.PixelFormat=PixelBltOnly;mInfo.PixelsPerScanLine=WIDTH;
  mMode.MaxMode=1;mMode.Info=&mInfo;mMode.SizeOfInfo=sizeof(mInfo);mMode.FrameBufferBase=0;mMode.FrameBufferSize=0;
  mGop.QueryMode=Query;mGop.SetMode=Set;mGop.Blt=Blt;mGop.Mode=&mMode;
  Status=gBS->InstallMultipleProtocolInterfaces(&mHandle,&gEfiGraphicsOutputProtocolGuid,&mGop,NULL);
  if(EFI_ERROR(Status)){RestoreRefresh();FreePool(mSaved);mSaved=NULL;return Status;}
  // The application presents complete frames cooperatively; no mid-BLT refresh.
  if(Native) {
    DEBUG((DEBUG_INFO,"UI_MDSS_ROUTE_U07 split=%x upper=%x lower=%x lm0=%x lm1=%x pp0=%x pp1=%x strategy=master-only,complete-frames\n",MmioRead32(0x0c9012f4),MmioRead32(0x0c9012f8),MmioRead32(0x0c9013f0),MmioRead32(0x0c945004),MmioRead32(0x0c946004),MmioRead32(0x0c971030),MmioRead32(0x0c971830)));
    DEBUG((DEBUG_INFO,"UI_GOP_READY_U07 base=%lx size=%u width=1080 height=1920 stride=3240 bpp=24 original_crc=%x status=Success\n",(UINT64)FB_BASE,FB_BYTES,mOriginalCrc));
    return EFI_SUCCESS;
  }
  Status=gBS->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Refresh,NULL,&mRefresh);if(EFI_ERROR(Status))return Status;
  Status=gBS->SetTimer(mRefresh,TimerPeriodic,500000);
  if(EFI_ERROR(Status)){DEBUG((DEBUG_INFO,"UI_GOP_POLL_REFRESH_U07 status=%r\n",Status));Status=EFI_SUCCESS;}
  DEBUG((DEBUG_INFO,"UI_GOP_READY_U07 base=%lx size=%u width=1080 height=1920 stride=3240 bpp=24 original_crc=%x status=%r\n",(UINT64)FB_BASE,FB_BYTES,mOriginalCrc,Status));return Status;
}

EFI_GRAPHICS_OUTPUT_PROTOCOL *SonyGop(VOID) {return &mGop;}
VOID SonyGopPresent(VOID) {EFI_TPL Old=gBS->RaiseTPL(TPL_CALLBACK);Present();gBS->RestoreTPL(Old);}
EFI_STATUS SonyGopStop(VOID) {
  UINT32 Crc=0,CompletedBefore=mCompleted;BOOLEAN Idle=TRUE,StateRestored=TRUE,Cleaned=FALSE;
  if(mRefresh){gBS->SetTimer(mRefresh,TimerCancel,0);gBS->CloseEvent(mRefresh);mRefresh=NULL;}
  if(mSaved){
    DumpOriginal();Idle=!mNative || WaitIdle();
    if(Idle){
      CopyMem((VOID *)FB_BASE,mSaved,FB_BYTES);
      // Restoration must reach PoC even if a failed frame short-circuits Present.
      CleanSurface(0,FB_BYTES);Cleaned=TRUE;mDirtyFirst=0;mDirtyEnd=FB_BYTES;Present();
      gBS->CalculateCrc32((VOID *)FB_BASE,FB_BYTES,&Crc);
    }
    FreePool(mSaved);mSaved=NULL;
  }
  DEBUG((DEBUG_INFO,"UI_GOP_RESTORE_U07 expected=%x actual=%x equal=%u blts=%u frames=%u kickoff_timeouts=%u\n",mOriginalCrc,Crc,mOriginalCrc==Crc,mBlts,mKicks,mTimeouts));
  if(mNative)StateRestored=RestoreRefresh();
  DEBUG((DEBUG_INFO,"UI_RESTORE_DETAIL_U07 idle=%u clean=%u frame_done=%u refresh_state=%u\n",Idle,Cleaned,mCompleted>CompletedBefore,StateRestored));
  if(mHandle)gBS->UninstallMultipleProtocolInterfaces(mHandle,&gEfiGraphicsOutputProtocolGuid,&mGop,NULL);
  return Idle && Cleaned && Crc==mOriginalCrc && StateRestored?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
