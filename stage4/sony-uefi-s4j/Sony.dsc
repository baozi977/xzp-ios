[Defines]
  PLATFORM_NAME = SonyResidentProbe
  PLATFORM_GUID = ae5fabf1-970c-4ea3-8540-d969ab605be7
  PLATFORM_VERSION = 0.1
  DSC_SPECIFICATION = 0x00010019
  OUTPUT_DIRECTORY = Build/SonyResidentProbe
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = DEBUG
  SKUID_IDENTIFIER = DEFAULT
  FLASH_DEFINITION = SonyProbe/Sony.fdf

!include SonyProbe/SonyCommon.dsc.inc

[PcdsFixedAtBuild.common]
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x24000000
  gArmPlatformTokenSpaceGuid.PcdSystemMemoryUefiRegionSize|0x03c00000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000
  gArmPlatformTokenSpaceGuid.PcdCPUCoreSecondaryStackSize|0
  gArmPlatformTokenSpaceGuid.PcdCoreCount|1
  gArmPlatformTokenSpaceGuid.PcdClusterCount|1
  gArmTokenSpaceGuid.PcdArmPrimaryCore|0
  gArmTokenSpaceGuid.PcdArmPrimaryCoreMask|0xffff
  gArmTokenSpaceGuid.PcdVFPEnabled|1
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x27
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x800000c7
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"XZPIOS-Sony-UI-20261028-S4J"

[PcdsFeatureFlag.common]
  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|FALSE
  gArmPlatformTokenSpaceGuid.PcdSendSgiToBringUpSecondaryCores|FALSE

[LibraryClasses.common]
  SerialPortLib|SonyProbe/SonySerialPortLib.inf
  ArmPlatformLib|SonyProbe/SonyArmPlatformLib.inf
  MemoryInitPeiLib|SonyProbe/SonyMemoryInit.inf
  PlatformPeiLib|Silicon/QC/Msm8998/QcomPkg/Library/PlatformPeiLib/PlatformPeiLib.inf
  ArmMmuLib|ArmPkg/Library/ArmMmuLib/ArmMmuBaseLib.inf

[LibraryClasses.common.SEC]
  MemoryInitPeiLib|SonyProbe/SonyMemoryInit.inf
  PlatformPeiLib|Silicon/QC/Msm8998/QcomPkg/Library/PlatformPeiLib/PlatformPeiLib.inf

[Components.common]
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  SonyProbe/SonyUiApp.inf
  SonyProbe/ResidentApp.inf
  ArmPlatformPkg/PrePi/PeiUniCore.inf
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      NULL|MdeModulePkg/Library/DxeCrc32GuidedSectionExtractLib/DxeCrc32GuidedSectionExtractLib.inf
  }
  SonyProbe/SonyDxeProbe.inf
