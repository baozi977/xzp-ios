// SPDX-License-Identifier: MIT
// Observer READ adapter reused from the OnePlus5 BoardRuntime implementation.
// Sony live DT independently identifies SID0 PON, GPIO5/down and GPIO6/up.
#include <Uefi.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/DevicePath.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL mInput;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL *mPrevious;
STATIC EFI_HANDLE mPreviousHandle,mInputHandle;
STATIC EFI_EVENT mPoll;
STATIC UINTN mObservers[3];
STATIC EFI_INPUT_KEY mQueue[16];
STATIC UINTN mHead,mTail,mFailures,mEdges;
STATIC UINT32 mCandidate,mLast;
STATIC UINTN mStable;
STATIC BOOLEAN mReleased,mEnabled;
STATIC struct {VENDOR_DEVICE_PATH Vendor;EFI_DEVICE_PATH_PROTOCOL End;} mPath={
  {{HARDWARE_DEVICE_PATH,HW_VENDOR_DP,{sizeof(VENDOR_DEVICE_PATH),0}},{0x71ff8966,0x2add,0x4274,{0xaf,0x11,0x10,0x28,0x20,0x26,0,1}}},
  {END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{sizeof(EFI_DEVICE_PATH_PROTOCOL),0}}
};

STATIC VOID TableCrc(VOID) {gST->Hdr.CRC32=0;gBS->CalculateCrc32(gST,gST->Hdr.HeaderSize,&gST->Hdr.CRC32);}
STATIC BOOLEAN ReadStatus(UINTN Base,UINT8 Offset,UINT8 *Value) {
  UINTN I;UINT32 Status;
  MmioWrite32(Base,(1U<<27)|((UINT32)Offset<<4));__asm__ volatile("dsb sy":::"memory");
  for(I=0;I<100;I++) {Status=MmioRead32(Base+8);if(Status&1){if(Status&14)return FALSE;__asm__ volatile("dmb sy":::"memory");*Value=(UINT8)MmioRead32(Base+0x18);return TRUE;}MicroSecondDelay(1);}
  return FALSE;
}
STATIC VOID Enqueue(UINT16 Scan,CHAR16 Ch) {
  UINTN Next=(mTail+1)%16;if(Next==mHead)return;
  mQueue[mTail].ScanCode=Scan;mQueue[mTail].UnicodeChar=Ch;mTail=Next;mEdges++;
  DEBUG((DEBUG_INFO,"UI_KEY_S4F edge=%u scan=%x char=%x\n",(UINT32)mEdges,Scan,Ch));gBS->SignalEvent(mInput.WaitForKey);
}
STATIC VOID EFIAPI Poll(EFI_EVENT Event,VOID *Context) {
  UINTN I;UINT8 Raw[3];UINT32 Mask,Pressed;(VOID)Event;(VOID)Context;
  if(!mEnabled)return;
  for(I=0;I<3;I++)if(!ReadStatus(mObservers[I],I?8:0x10,&Raw[I])){mFailures++;if(mFailures>=3){mEnabled=FALSE;DEBUG((DEBUG_ERROR,"UI_INPUT_DISABLED_S4F read_failures=%u\n",(UINT32)mFailures));}return;}
  Mask=((Raw[0]&1)?2:0)|((Raw[2]&1)?0:4)|((Raw[1]&1)?0:8);
  if(Mask!=mCandidate){mCandidate=Mask;mStable=1;return;}if(mStable<2)mStable++;
  if(mStable<2)return;
  if(!mReleased){if(!Mask)mReleased=TRUE;return;}
  if(Mask==mLast)return;
  Pressed=Mask&~mLast;mLast=Mask;
  if(Pressed&4)Enqueue(SCAN_UP,0);
  if(Pressed&8)Enqueue(SCAN_DOWN,0);
  if(Pressed&2)Enqueue(0,CHAR_CARRIAGE_RETURN);
}
STATIC EFI_STATUS EFIAPI Reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,BOOLEAN Extended) {(VOID)This;(VOID)Extended;mHead=mTail=0;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Read(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,EFI_INPUT_KEY *Key) {
  EFI_TPL Old;(VOID)This;if(!Key)return EFI_INVALID_PARAMETER;Old=gBS->RaiseTPL(TPL_NOTIFY);
  if(mHead==mTail){gBS->RestoreTPL(Old);return EFI_NOT_READY;}*Key=mQueue[mHead];mHead=(mHead+1)%16;gBS->RestoreTPL(Old);return EFI_SUCCESS;
}
STATIC VOID EFIAPI Wait(EFI_EVENT Event,VOID *Context) {(VOID)Context;if(mHead!=mTail)gBS->SignalEvent(Event);}

EFI_STATUS SonyInputStart(BOOLEAN Native) {
  UINT32 Version,Entry;UINTN I,J;EFI_STATUS Status;UINT32 Ppids[3]={0x008,0x0c4,0x0c5};UINT8 Raw[3];
  if(!Native)return EFI_UNSUPPORTED;
  Version=MmioRead32(0x0800f000);DEBUG((DEBUG_INFO,"UI_SPMI_VERSION_S4F value=%x\n",Version));if(Version<0x20010000 || Version>=0x50000000)return EFI_UNSUPPORTED;
  for(I=0;I<512;I++){Entry=MmioRead32(0x0800f800+4*I);if(!Entry)continue;for(J=0;J<3;J++)if(!mObservers[J] && ((Entry>>8)&0xfff)==Ppids[J])mObservers[J]=0x09400000UL+0x8000*I;}
  for(I=0;I<3;I++)if(!mObservers[I] || !ReadStatus(mObservers[I],I?8:0x10,&Raw[I]))return EFI_DEVICE_ERROR;
  DEBUG((DEBUG_INFO,"UI_INPUT_HARDWARE_S4F pon=%lx down=%lx up=%lx raw=%x,%x,%x\n",(UINT64)mObservers[0],(UINT64)mObservers[1],(UINT64)mObservers[2],Raw[0],Raw[1],Raw[2]));
  mInput.Reset=Reset;mInput.ReadKeyStroke=Read;
  Status=gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_NOTIFY,Wait,NULL,&mInput.WaitForKey);if(EFI_ERROR(Status))return Status;
  Status=gBS->InstallMultipleProtocolInterfaces(&mInputHandle,&gEfiSimpleTextInProtocolGuid,&mInput,&gEfiDevicePathProtocolGuid,&mPath,NULL);if(EFI_ERROR(Status))return Status;
  Status=gBS->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Poll,NULL,&mPoll);if(EFI_ERROR(Status))return Status;
  mPrevious=gST->ConIn;mPreviousHandle=gST->ConsoleInHandle;gST->ConIn=&mInput;gST->ConsoleInHandle=mInputHandle;TableCrc();mEnabled=TRUE;
  Status=gBS->SetTimer(mPoll,TimerPeriodic,100000);if(EFI_ERROR(Status))Status=EFI_SUCCESS;DEBUG((DEBUG_INFO,"UI_INPUT_READY_S4F status=%r debounce_ms=20 mapping=volume-up/down,power-enter\n",Status));return Status;
}

VOID SonyInputStop(VOID) {
  if(mPoll){gBS->SetTimer(mPoll,TimerCancel,0);gBS->CloseEvent(mPoll);mPoll=NULL;}
  if(gST->ConIn==&mInput){gST->ConIn=mPrevious;gST->ConsoleInHandle=mPreviousHandle;TableCrc();}
  if(mInputHandle)gBS->UninstallMultipleProtocolInterfaces(mInputHandle,&gEfiSimpleTextInProtocolGuid,&mInput,&gEfiDevicePathProtocolGuid,&mPath,NULL);
  if(mInput.WaitForKey)gBS->CloseEvent(mInput.WaitForKey);
  DEBUG((DEBUG_INFO,"UI_INPUT_STOP_S4F edges=%u failures=%u\n",(UINT32)mEdges,(UINT32)mFailures));
}

VOID SonyInputPoll(VOID) {Poll(NULL,NULL);}
