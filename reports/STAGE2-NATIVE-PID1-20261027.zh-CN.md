# 阶段二：SO-04J 原生 XNU 与自有 PID1

2026-10-27，同一 R21 镜像已连续两次在 Xperia XZ Premium SO-04J 实机运行 Darwin 22.5.0 / XNU 8796.121.2；每次均输出自有 PID1 的 `pid=1 uid=0` 和 14 次连续心跳，按 `loader_return_after=15` 返回 UEFI，再自动启动 Android。阶段二的最小原生启动验收已通过，可以进入阶段三。

## 验收证据

首轮原始日志位于 [entry-log.txt](../captures/stage2-xnu-20261027-r21/phone/entry-log.txt)，独立验收为 [verified-result.json](../captures/stage2-xnu-20261027-r21/verified-result.json)。验收脚本直接检查返回后的原始 pstore，要求实验标记在启动前不存在，并核对镜像 / PID1 哈希、原生中断号和 Android 状态。

同一镜像复测前先完成一次普通 Android 重启，取得新的启动 ID 和无实验标记的 pstore 基线。第二次仍有 14 次心跳：[复测验收](../captures/stage2-xnu-20261027-r21-repeat1/verified-result.json)、[复测原始日志](../captures/stage2-xnu-20261027-r21-repeat1/phone/entry-log.txt)。总验收记录为 [stage2-acceptance.json](../captures/stage2-xnu-20261027-r21/stage2-acceptance.json)，两轮原始 pstore SHA-256 不同，boot.img / Image 哈希相同。

```text
S2_GIC_GATE_R21 attempt=0 expected=19 expired=1 hppir=19 iar=19
XZPIOS PID1 START pid=1 uid=0
XZPIOS PID1 HEARTBEAT 1
...
XZPIOS PID1 HEARTBEAT 14
--- loader_return_after expired ---
Darwin Kernel Version 22.5.0
S2_POST_RETURN_ALLOC status=Success
XZPIOS_S2_XNU_RETURN_PASS_R21
```

PID1 源码为 [pid1.c](../stage2/pid1-heartbeat/pid1.c)。它直接使用 Darwin 的 open / write / getpid / getuid / select 系统调用，不依赖 libc 或 dyld；每轮 select 请求一秒超时。静态 Mach-O 为 17,024 字节，SHA-256：`b4d305cd37a676ff6bfd5c8c67fc0ca49d7fa678d787b02df40d2acb3d92c16e`。

## 已打通的路径

Sony 临时启动 → 原生 UEFI / 4 KiB MMU / DXE → 独立 XNU 装载窗口 → CPU / VM / 平台初始化 → GIC / 虚拟计时器 → BSD → md0 最小根环境 → 自有 PID1 → 系统调用与定时睡眠 → 内核定时返回 → UEFI 恢复与页面分配 → Android。

| 项目 | 实机值 / 结果 |
|---|---|
| 启动核 | CPU0，MIDR `0x51af8014` |
| 页表粒度 | 4 KiB |
| XNU 内存窗 | `[0xa4000000,0xb4000000)`，256 MiB |
| GICD / GICR | `0x17a00000` / `0x17b00000` |
| 虚拟计时器 | INTID19，19.2 MHz |
| CPU 接口验证 | HPPIR=19、IAR=19，EOI 完成 |
| 用户态 | 自有 PID1，PID=1、UID=0，14 次连续心跳 |
| 返回 | UEFI 控制寄存器恢复，Boot Services 页面分配成功 |
| Android | `4.4.302-xzpios-fit`，`boot_completed=1` |

## 关键改动

- 使用实测 Sony ADT、保留区、GIC 和 timer 值；内核在独立 256 MiB 窗口装载，18 个 Mach-O 段复制 / BSS 清零。
- 修正加载器栈清理越界；返回路径按 CPU 特性处理 PAN / UAO。
- 补齐非零 SoC parent base、`/defaults`、panic 保留区、随机种子和 NVRAM 数据。
- 手机进入时 GIC 尚未启用 ARE。改为先启用 ARE / Group1，再配置 PPI19，并在 CPU IRQ / FIQ 屏蔽状态下实际验证到期和 HPPIR / IAR。
- 固定内核输入校验后，按 CPU0 PAN=0 适配 24 处 PAN 写和 14 处 PAN 读取，并逐指令核对 5 处 timer IRQ19 适配。原内核文件保持固定 SHA。
- 增加不可恢复用户异常的寄存器诊断；仿真通过正常启动及 PC=0 故障注入两项对照。R21 首轮实机未触发该诊断路径。

## R20 与 R21 的差异记录

R20 已到达根文件系统挂载和 PID1 加载，但 PID1 因信号 10 退出；[原始结果](../captures/stage2-xnu-20261027-r20/native-result.json) 保留。R21 增加用户异常现场诊断后正常启动，首轮没有触发该诊断。当前尚未证明 R20 的退出原因。后续源内核整理和持续运行测试需保留这项回归检查。

R21 的正常 QEMU 对照通过，坏地址注入也能报告正确的用户 PC=0 / ESR / FAR 并返回：[negative-gate-result.json](../captures/stage2-xnu-20261027-r21/negative-gate-result.json)。两次成功实机均未触发异常诊断。

## 产物与构建位置

- 源码：[xnu-abort-r21](../stage2/xnu-abort-r21/)。
- 临时镜像：[boot.img](../artifacts/stage2-xnu-20261027-r21/boot.img)。
- boot.img SHA-256：`1e1235c8675d166378bbc24529db854ac374e3fd5f1b05039b68ce7ac38dd165`。
- Image SHA-256：`969e8c1c58b3f84316d25e0e256aafb70eca13ac9c7f1b33220249827c0887b3`。
- 本轮构建、反汇编、正常 / 故障注入 QEMU 均在阿里云既有私有 lab 完成；构建上限 640 MiB / 单任务，QEMU 为 512 MiB / CPU50% / 离线，PID1 编译为 128 MiB / CPU50%。沿用私有 EDK2 / GCC9。
- 归档时重新核对编译输入哈希、阿里云四项原业务服务的状态 / PID / 重启次数；均与基线一致，没有运行中的本轮构建或 QEMU 单元：[服务器记录](../captures/stage2-xnu-20261027-r21/final-server-inventory.json)。
- 手机使用 `fastboot boot` 临时启动；boot 分区 SHA 保持 `4d3dd1eee8df36bdb6b85198bb478e1f3173af89d791f12ace257ec63cb731e1`，FOTAKernel 保持 `019d60fbef413647f2a49c1391d12d64771a63813d8cbcbbbaa30429f112b985`。

## 进入阶段三后的工作

1. 将已验证的无 PAN、IRQ19、Sony GIC 顺序和 4 KiB 用户态策略整理为可重建的源内核配置，持续检查 R20 类用户态异常。
2. 固定目标 iOS 用户态与内核 ABI，核对 dyld、共享缓存、代码签名和关键服务依赖，先在阿里云最小环境验证，再转入手机。
3. 接入 Apple launchd 和基础服务，然后推进根卷、显示 / 锁屏 / SpringBoard、触摸输入。

阶段二的用户进程为自有最小 PID1。Apple 用户态、锁屏 / SpringBoard 和触摸是后续阶段的验收项。
