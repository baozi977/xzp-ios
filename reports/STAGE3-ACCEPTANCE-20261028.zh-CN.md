# 阶段三验收：源内核与 Apple 用户态依赖

更新：2026-10-28（Asia/Hong_Kong）。第三阶段已通过，可以进入第四阶段的手机根卷与 Apple 服务接入。

## 本次结果

| 验收项 | 实测结果 |
|---|---|
| Sony 源内核 | 阿里云私有工具链生成 XNU 8796.121.2，16,309,504 字节；执行段 PAN 指令 0，未解析符号 0 |
| 板级配置 | 编译时关闭 PAN；定时器由 ADT `/arm-io/timer/p8,timer-intid` 读取；GIC ARE 在 PPI 初始化前启用 |
| 加载器 | R22 根据新 Mach-O 自动生成入口 / 段 / 根镜像 / ADT / boot_args 布局，移除 R21 的 PAN、timer 与诊断指令补丁 |
| Cortex-A72 | 阿里云 512 MiB 限额内通过完整启动、PID1 心跳、定时返回和 UEFI 继续运行 |
| SO-04J 实机 | 原生 Darwin 22.5.0、自有 PID1（PID=1 / UID=0）、14 次连续心跳，IRQ19 正常，定时返回 UEFI 后自动进入 Android |
| 分区回读 | boot / FOTAKernel 哈希与测试前基线一致；本轮通过 `fastboot boot` 执行 |
| Apple 用户态 | 云端 hello_dyn 经 Apple dyld / 共享缓存返回预期值 7；原始 Apple launchd 完成早期启动并经 xpcproxy 拉起服务 |

源内核 SHA-256：`93919f7e75100f0d39db35c233ca908e918a7ae74f42f6bd253b641908854c53`。

## 复用内容

源内核和用户态沿用固定 HackiOS 8796 / iOS16.5（20F66）实现；下载、APFS、bootstrap、dyld 和 QEMU 流程已复用。一加五仓库提供 `p8,timer-intid` 绑定以及后续根卷 / 服务 / 显示组件的参考。详细来源和选择依据见 [复用报告](STAGE3-REUSE-20261028.zh-CN.md)。

## 构建与运行证据

- [源内核构建结果](../captures/stage3-20261027/kernel-build-final.json)、[后置目标错误明细](../captures/stage3-20261027/kernel-postlink-errors.json)。`make` 总退出码为 2，失败项为 export / symbolset；内核可执行文件已经完成链接与 strip，并通过指令、符号、QEMU 和实机验收。接入外部 kext 前需重新检查导出面。
- [R22 实机独立验收](../captures/stage3-source-r22-20261028/verified-result.json)、[实机内核日志](../captures/stage3-source-r22-20261028/native-kernel-log.txt)、[QEMU 完整保留日志](../captures/stage3-source-r22-20261028/qemu-console.txt)。
- [Apple 用户态报告](../artifacts/stage3-runtime-cloud-20261028/reports/STAGE3-APPLE-RUNTIME-20261028.md)、[取回与 ABI 核对](../captures/stage3-20261027/userland-abi-verification.json)。完整 Apple guest 日志仍在云端；本机保存报告、哈希和关键串口摘录。
- [R22 源码](../stage3/xnu-source-r22/)、[实际 Mach-O 布局](../stage3/layout-r22/kernel-contract.json)、[源内核产物](../artifacts/stage3-source-kernel-20261028/)、[临时启动镜像](../artifacts/stage3-source-r22-20261028/verification.json)。

## 下一段

下一验收点是 **SO-04J 上运行 Apple launchd**。先确定根卷承载方案：现有 system.apfs 为 2,492,203,008 字节，无法直接附加到 64 MiB 临时启动镜像；云端的物理加载地址也不能直接用于 Sony。需要按实测保留区评估分段内存载入、裁剪后的 RAM 根环境或只读存储路径，优先复用已有驱动和镜像工具。

手机上当前运行的是自有 PID1。Apple 用户态已在仿真运行；手机上的 Apple launchd、锁屏 / SpringBoard、触摸尚未完成。

阿里云完成本次源内核、UEFI 构建与最小 QEMU；完整 Apple guest 的驻留工作集超出阿里云可用内存，才通过其 Codex CLI 使用指定云环境。共享服务 PID / NRestarts 与基线一致。
