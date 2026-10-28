# Sony UEFI 图形界面与 UniXNU 菜单验收

2026-10-28（Asia/Hong_Kong）。插入任务已完成：SO-04J 可显示 UEFI 图形界面，并通过实体音量 / 电源键操作现成 UniXNU 菜单。U07 是后续图形启动实验的已验收基线。

## 实机结果

| 项目 | 验收结果 |
|---|---|
| 色块 / 字体 / 几何 | U06 高清照片显示完整六色块、居中项目名、构建版本和状态文字；此前半屏错位已消失 |
| 实际刷新 | U07 共 10 次提交均取得清零后新出现的双路 PP_DONE=0x300，0 次超时 |
| UniXNU 菜单 | 复用固定版本 Menu / Ui / Font / Logo，提供 XNU/PID1 test、Return to Android、Display test |
| 实体按键 | 本机 PM8998 只读输入适配；记录 6 个事件，包含音量上、音量下、电源确认，读取失败 0 |
| 从菜单启动内核 | 实机选中 XNU/PID1，运行原生 Darwin22.5 / 自有 PID1，14 次连续心跳，约15秒定时返回 |
| 返回后显示 | 缓冲区 CRC `28B882C1` 前后一致；UEFI 后续绘制取得新双路帧完成 |
| 清理与恢复 | 原图清缓存后恢复；TE / 自动刷新原值恢复；自动进入 Android，boot / FOTAKernel 哈希不变 |

机主确认“移动正常，电源键正常，界面显示正常”。选中 Test1 后菜单不响应按键的阶段是 XNU 运行窗口；日志确认其正常执行并定时返回。

## 修复依据

ABL 留下 `PP0_AUTOREFRESH=0x80000001`。旧测试版直接软件提交，导致两半屏不同步或不出新图。U06 / U07 保存 TE / 自动刷新状态、停止 TE 触发、等待现有传输空闲、关闭自动刷新、恢复 TE，再分别 flush 两路并从主 CTL 提交。完成判断改为实际的新 PP_DONE，而非仅检查 CTL_START 自清。错误清理路径也保证原图到 PoC 的缓存清理及恢复结果传播。

此流程按本机 Linux 源码核对，并与 [上游 Qualcomm MDSS 实现](https://android.googlesource.com/kernel/msm/+/android-msm-wahoo-4.4-p-preview-2/drivers/video/fbdev/msm/mdss_mdp_intf_cmd.c) 对照。详细显示定位与实测寄存器见 [U06 显示修复报告](UEFI-DISPLAY-FIX-20261028.zh-CN.md)。

## 产物与复用

- [U07 临时启动镜像](../artifacts/sony-uefi-ui-20261028-u07/boot.img)、[打包核对](../artifacts/sony-uefi-ui-20261028-u07/verification.json)、[源码 manifest](../artifacts/sony-uefi-ui-20261028-u07/source-manifest.json)。
- [U07 固件源码](../stage3/sony-uefi-ui-u07/)、[菜单 / 内核独立验收](../captures/sony-uefi-ui-20261028-u07/menu-acceptance.json)、[真实帧完成日志](../captures/sony-uefi-ui-20261028-u07/native-ui-summary.txt)、[机主操作反馈](../captures/sony-uefi-ui-20261028-u07/owner-observation.json)。
- [U06 高清实机照片](../captures/sony-uefi-ui-20261028-u06/owner-display-photo-hd.jpg)、[显示验收](../captures/sony-uefi-ui-20261028-u06/display-acceptance.json)。

Boot SHA-256：`7f2b469556d99a57cd82c41d7485f7ef4484cff4d07d83e127d5cab21049046b`。

所有构建和小型仿真均在阿里云原私有 lab 的限额内执行，本插入任务未使用 Codex Cloud。既有服务状态保持基线。原交接目录和 R22 源内核验收结果保留。

手机当前运行 Android。再次临时启动 U07 镜像即可进入图形菜单；后续 Apple 根环境工作可沿用这套显示 / 按键入口。Test1 当前运行自有 PID1，Apple 用户态的原生接入继续按第四阶段推进。
