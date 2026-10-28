// SPDX-License-Identifier: MIT
#include "UniXNU/Internal.h"
#include <PiDxe.h>
#include <Library/DxeServicesLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>

EFI_STATUS SonyGopStart(BOOLEAN Native);
EFI_GRAPHICS_OUTPUT_PROTOCOL *SonyGop(VOID);
VOID SonyGopPresent(VOID);
EFI_STATUS SonyGopStop(VOID);
BOOLEAN SonyGopHealthy(VOID);
EFI_STATUS SonyInputStart(BOOLEAN Native);
VOID SonyInputStop(VOID);
VOID SonyInputPoll(VOID);
VOID SonyUiPoll(VOID) {SonyInputPoll();SonyGopPresent();}

VOID EFIAPI UniXNULog(CONST CHAR8 *Format,...) {
  VA_LIST Args;CHAR8 Line[512];UINTN N;
  VA_START(Args,Format);N=AsciiVSPrint(Line,sizeof(Line)-2,Format,Args);VA_END(Args);
  Line[N++]='\n';Line[N]=0;DEBUG((DEBUG_INFO,"UNIXNU: %a",Line));
}

STATIC VOID Pattern(UNIXNU_UI *Ui,UNIXNU_CONFIG *Config) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *G=SonyGop();UINTN I;UINT32 Crc1=0,Crc2=0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Colors[6]={{0,0,255,0},{0,255,0,0},{255,0,0,0},{255,255,255,0},{0,0,0,0},{0,255,255,0}};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel={0x56,0x34,0x12,0},Readback={0};
  UniXNUUiDrawFrame(Ui,Config);UniXNUUiDrawStatus(Ui,0,L"DISPLAY TEST: RED GREEN BLUE WHITE BLACK YELLOW");
  UniXNUUiDrawStatus(Ui,1,L"1080x1920 | BGR888 | stride 3240");UniXNUUiDrawStatus(Ui,2,L"Cache clean + dual command refresh");
  for(I=0;I<6;I++)G->Blt(G,&Colors[I],EfiBltVideoFill,0,0,I*180,700,180,540,0);
  G->Blt(G,&Pixel,EfiBltVideoFill,0,0,1079,1919,1,1,0);G->Blt(G,&Readback,EfiBltVideoToBltBuffer,1079,1919,0,0,1,1,0);
  ASSERT(Readback.Blue==0x56 && Readback.Green==0x34 && Readback.Red==0x12);
  SonyGopPresent();gBS->CalculateCrc32((VOID *)0x9d400000,6220800,&Crc1);gBS->Stall(2000000);
  UniXNUUiDrawStatus(Ui,0,L"DISPLAY TEST: CACHE REFRESH 2 / MOVING MARKER");
  for(I=0;I<6;I++) {
    if(I)G->Blt(G,&Colors[4],EfiBltVideoFill,0,0,(I-1)*180,1280,180,100,0);
    G->Blt(G,&Colors[3],EfiBltVideoFill,0,0,I*180,1280,180,100,0);SonyGopPresent();gBS->Stall(400000);
  }
  gBS->CalculateCrc32((VOID *)0x9d400000,6220800,&Crc2);
  DEBUG((DEBUG_INFO,"UI_PATTERN_PASS_S4J crc1=%x crc2=%x changed=%u pixel_roundtrip=1\n",Crc1,Crc2,Crc1!=Crc2));gBS->Stall(2000000);
}

STATIC EFI_STATUS RunXnu(EFI_HANDLE Parent) {
  EFI_GUID Guid={0x96aa8849,0x73b5,0x47d7,{0xb8,0x56,0x12,0x5a,0x1a,0xa3,0x72,0x78}};
  VOID *Data=NULL;UINTN Size=0;EFI_HANDLE App=NULL;EFI_STATUS Status;
  Status=GetSectionFromAnyFv(&Guid,EFI_SECTION_PE32,0,&Data,&Size);if(EFI_ERROR(Status))return Status;
  Status=gBS->LoadImage(FALSE,Parent,NULL,Data,Size,&App);FreePool(Data);if(EFI_ERROR(Status))return Status;
  Status=gBS->StartImage(App,NULL,NULL);gBS->UnloadImage(App);return Status;
}

EFI_STATUS EFIAPI SonyUiEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status,InputStatus;UINT64 Midr;BOOLEAN Native;UINTN Choice;UNIXNU_UI Ui;UNIXNU_CONFIG Config;UINT32 Before=0,After=0;
  UNIXNU_ENTRY Entries[3];(VOID)SystemTable;
  __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));Native=Midr==0x51af8014;
  if(!Native && (Midr&0xff00fff0)!=0x4100d080)return EFI_UNSUPPORTED;
  DEBUG((DEBUG_INFO,"UI_APP_ENTER_S4J midr=%lx native=%u\n",Midr,Native));
  Status=SonyGopStart(Native);if(EFI_ERROR(Status)){DEBUG((DEBUG_ERROR,"UI_GOP_FAILED_S4J status=%r\n",Status));return Status;}
  InputStatus=EFI_UNSUPPORTED;DEBUG((DEBUG_INFO,"UI_INPUT_START_S4J status=%r\n",InputStatus));
  ZeroMem(&Config,sizeof(Config));ZeroMem(Entries,sizeof(Entries));
  Config.Title=L"XZPIOS";Config.SourceLabel=L"SO-04J | S4J | 2026-10-28";Config.Console=UniXNUConsoleGraphics;
  Config.Layout=UniXNULayoutPortrait;Config.PortraitBelow=800;Config.DefaultIndex=0;Config.Timeout=3;Config.FallbackTimeout=3;Config.Entries=Entries;Config.EntryCount=3;
  Entries[0].Id="xnu-pid1";Entries[0].Title=L"XNU / PID1 test";Entries[0].Type=UniXNUEntryXnu;
  Entries[1].Id="android";Entries[1].Title=L"Return to Android";Entries[1].Type=UniXNUEntryAction;Entries[1].Action=UniXNUActionReset;
  Entries[2].Id="display";Entries[2].Title=L"Display test";Entries[2].Type=UniXNUEntryAction;
  UniXNUUiInit(&Config,&Ui);if(Ui.Mode!=UniXNUUiGraphics){SonyInputStop();SonyGopStop();return EFI_DEVICE_ERROR;}
  UniXNUUiDrawFrame(&Ui,&Config);UniXNUUiDrawStatus(&Ui,0,L"UEFI ready - preparing boot menu");SonyGopPresent();
  for(;;) {
    Choice=UniXNUMenuRun(&Ui,&Config,TRUE);SonyGopPresent();DEBUG((DEBUG_INFO,"UI_MENU_SELECTED_S4J index=%u\n",(UINT32)Choice));
    if(!SonyGopHealthy()){Status=EFI_DEVICE_ERROR;break;}
    if(Choice==2){Pattern(&Ui,&Config);continue;}
    if(Choice==0) {
      UniXNUUiDrawStatus(&Ui,0,L"Loading Apple APFS; timed return in 90 s");SonyGopPresent();gBS->CalculateCrc32((VOID *)0x9d400000,6220800,&Before);
      Status=RunXnu(ImageHandle);gBS->CalculateCrc32((VOID *)0x9d400000,6220800,&After);
      DEBUG((DEBUG_INFO,"UI_XNU_RETURN_S4J status=%r display_crc_before=%x after=%x equal=%u\n",Status,Before,After,Before==After));
      UniXNUUiDrawStatus(&Ui,0,EFI_ERROR(Status)?L"XNU test failed; see retained log":L"XNU returned successfully");SonyGopPresent();gBS->Stall(3000000);
    }
    break;
  }
  if(!SonyGopHealthy())Status=EFI_DEVICE_ERROR;
  SonyInputStop();{EFI_STATUS RestoreStatus=SonyGopStop();if(EFI_ERROR(RestoreStatus))Status=RestoreStatus;}DEBUG((DEBUG_INFO,"UI_APP_RETURN_S4J status=%r\n",Status));return Status;
}
