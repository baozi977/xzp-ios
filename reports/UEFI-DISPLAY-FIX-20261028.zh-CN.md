# Sony UEFI 显示修复：U06

2026-10-28。U06 已在 SO-04J 显示完整、位置正确的测试图，并通过两路实际帧完成、原图 / 刷新配置恢复和 Android 自动返回检查。UniXNU 菜单与实体按键接入继续在 U07 验收。

## 问题与修复

此前固件沿用了 ABL 的 `PP0 AUTOREFRESH=0x80000001`，同时使用软件 `CTL_START`。U02 / U03 的内存画面与同分辨率 QEMU CRC 一致，但面板左右不同步、内容上下错位。U05 只改成主 CTL 触发仍未显示成功。

重新核对本机 Linux 和上游 Qualcomm MDSS 实现后，U06 改为完整的 splash 接管流程：保存 TE / 自动刷新原值，停止 TE 触发并等当前传输空闲，关闭自动刷新，再恢复 TE 原值。之后分别 flush 两路 CTL，仅从主 CTL 提交完整画面。

完成判定也已修正：每次先清 PP0 / PP1 的旧完成位，确认清零，再等本次新出现的 bit8 / bit9。退出时先等空闲、恢复原像素并无条件清缓存到 PoC，再恢复原刷新状态；错误会传播到应用结果。

参考实现：[Google kernel/msm 的 MDSS 命令模式驱动](https://android.googlesource.com/kernel/msm/+/android-msm-wahoo-4.4-p-preview-2/drivers/video/fbdev/msm/mdss_mdp_intf_cmd.c)。该驱动明确处理 splash 自动刷新交接以避免画面损坏。当前 edk2-msm8998 的 [SimpleFbDxe 模板](https://github.com/edk2-porting/edk2-msm8998/blob/ace27110b9b9b6c591387d50cf748ec646402b4a/Platforms/Msm8998FamilyPkg/Drivers/SimpleFbDxe/SimpleFbDxe.c) 没有这一层 Sony 双路交接。

## 实机证据

- 原 framebuffer：`0x9d400000`，1080×1920，BGR888，stride3240；几何保持原生寄存器实测值。
- 接管前 `TE0/1=0x800a6`、`AR0=0x80000001`、`AR1=1`；软件模式 AR0/1 均读回 0。
- **8 次提交均取得清零后新出现的双路 PP_DONE=0x300，0 次超时。** IRQ enable 保持原值 0，实测 raw status 仍正常锁存完成位。
- 原图 CRC `0x05b96629` 恢复，恢复帧也取得双路完成；TE / AR 原值逐项读回相等。
- 机主高清照片中，XZPIOS / 版本 / 状态文字、六色块和测试定位线均完整且位置正确；中间白线属于测试图。
- 自动返回 `4.4.302-xzpios-fit`，Android boot_completed=1，boot / FOTAKernel 哈希不变。

证据：[显示验收](../captures/sony-uefi-ui-20261028-u06/display-acceptance.json)、[传输独立核对](../captures/sony-uefi-ui-20261028-u06/transport-verification.json)、[实机摘要](../captures/sony-uefi-ui-20261028-u06/native-ui-summary.txt)、[高清实机照片](../captures/sony-uefi-ui-20261028-u06/owner-display-photo-hd.jpg)。

源码：[U06 SonyGop.c](../stage3/sony-uefi-ui-u06/SonyGop.c)。编译与软件 GOP 仿真均在阿里云既有限额内完成。原始交接目录和 R22 原生 XNU 基线保持原样。
