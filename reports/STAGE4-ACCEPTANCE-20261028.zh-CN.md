# 阶段四验收：SO-04J 原生 Apple 根环境与基本服务

2026-10-28，S4J 实机通过。手机已从原生 XNU 只读挂载 Apple APFS 系统镜像、切换根目录，运行 Apple dyld / 共享缓存和原始 Apple launchd，并完成通知与偏好设置服务的实际调用。

## 验收证据

| 项目 | SO-04J 实机结果 |
|---|---|
| 存储 | 2,427次UFS硬件读取、56,209,408字节（约53.6 MiB）、0错误 |
| 根环境 | System / 工具APFS两卷只读挂载；Apple launchd与init文件FNV均匹配；`kern.redwood_switch_root`成功 |
| 静态init | 36项检查、0失败；内存、文件、进程与设备检查通过 |
| 动态链接 | `/redwood/bin/hello_dyn`通过Apple dyld和共享缓存运行，按预期退出7 |
| Apple launchd | Darwin Bootstrapper Version 7.0.0；输出 `Early boot complete. Continuing system boot.` |
| Apple服务 | xpcproxy拉起notifyd（PID12）、cfprefsd（PID13）；两服务均进入running |
| 通知功能 | 注册通知、写入/读取状态、发送通知并收到通知均通过 |
| 偏好设置 | 经cfprefsd写入偏好并读取一致；`svc_test: 0 failed` |
| IOSurface接口 | `surf_test: 80 checks, 0 failed`；物理显示接入留给阶段五 |
| 往返 | 90秒定时返回，没有kernel panic；待机队列、原固件队列和显示均恢复 |
| Android | 正常启动，boot/FOTAKernel哈希与基线一致 |

[独立验收JSON](../captures/sony-stage4-20261028-s4j/verified-result.json)记录严格条件、日志摘要、哈希及恢复结果；[完整实机日志](../captures/sony-stage4-20261028-s4j/phone/entry-log.txt)保留原始输出。`phone/state.json`中的通用返回结果与Apple验收分开核对，最终结论由 `verify_stage4j_native.py`生成。

## 接入路线与修复

Android将项目镜像放入未加密的 `/data/xzpios-stage4/`，经SHA-256、FIEMAP和每段首/中/尾原始块抽样核对。系统镜像20段，工具卷1段，测试文件1段。XNU通过Sony实测UFS控制器 `0x01da4000`建立只读块设备，直接读取这些文件。

bootstrap先进入devfs，再用相对路径创建独立目录，在此挂载System卷。随后复用现有 `kern.redwood_switch_root`，将原mockfs根移到 `/System/Volumes/iOS`，devfs仍在 `/dev`；工具卷挂载于 `/redwood`，最后exec固定的 `/redwood/sbin/init`。Data使用init建立的ramfs。

S4G/H定位出mockfs路径限制：其 `/sbin`是根目录别名，挂载会变成重挂载mockfs；创建操作穿过mockfs的路径也被拒绝。S4I修复后已挂载并运行init，但动态链接失败：恰为1 GiB的启动内存使XNU选择4 KiB软件页，代码签名校验拒绝共享缓存的16 KiB签名单元。

S4J沿用原内核的页策略，将内存设为1088 MiB，范围 `[0xa4000000,0xe8000000)`，经Sony实测保留区核对通过。既有 `arm_init.c`因此选择16 KiB软件页；硬件页仍为4 KiB，实机TCR=`0x22a51a251a`（TG0=4 KiB、TG1=4 KiB），APFS日志确认软件页16384字节。交接context按16 KiB对齐，并为完整映射预留物理空间。XNU和Apple APFS镜像字节均与上一轮相同。

## 固定成果与构建位置

| 成果 | SHA-256 |
|---|---|
| S4J临时boot镜像 | `9aa2631c10324c829ce63f691045697167e59b3057eba69011aea53a758ef6e8` |
| 源码构建XNU | `070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c` |
| 原始Apple launchd | `4c61512d19b9a55a6ddad27e23b88fc5008fbf77d0c0b5e6bdd7c7d90cef3e06` |
| root.apfs | `dc4b1100f258b33da135b1fb2067e27e1a6de13def91838cd85eee83ce606364` |
| system.apfs | `431d46220bd4d6e7d04095781ea27a24ea36bc29edbbb62131bed9fea96f7bbb` |

固件、PID1、APFS构建和受限A72仿真均在阿里云现有私有lab完成，本轮未调用Codex Cloud。APFS构建在640 MiB / 单CPU限额内完成；单进程64 KiB分块压缩及磁盘暂存减少内存占用，六组样本与上游编码逐字节一致，两个完整镜像通过7-Zip全卷读取/解压校验。阿里云四个既有服务PID/NRestarts保持基线，实验单元均已结束，余约8.24 GB磁盘空间。

源码：[S4J固件](../stage4/sony-uefi-s4j/)、[原生存储适配](../stage4/sony-xnu-storage/)、[bootstrap PID1](../stage4/pid1-apfs-j/)。产物：[S4J启动镜像](../artifacts/sony-stage4-20261028-s4j/)、[APFS镜像及manifest](../artifacts/stage4-apfs-20261028/)。

## 下一阶段

阶段五目标是让Apple图形服务向手机显示，并逐步进入锁屏 / SpringBoard。当前显示已验收的是UEFI，Apple图形帧尚未接到Sony framebuffer。Data仍是易失ramfs，存储路径为只读系统卷；当前验收为单核、90秒运行。

先收敛启动服务集合：日志中项目辅助任务 `thermald`、`var_store`缺少程序，退出78；下一轮按Sony需要补齐或移除对应任务。随后核对IOSurface/显示服务与Sony帧缓冲的接口，先让Apple用户态画面稳定显示，再补齐锁屏 / SpringBoard所需服务。触摸与长期运行验收继续按原阶段六推进。
