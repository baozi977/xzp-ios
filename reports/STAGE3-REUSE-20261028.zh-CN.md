# 阶段三：复用路线与当前实现

更新：2026-10-28，Asia/Hong_Kong。目标为在已通过两轮实机验收的 SO-04J XNU 基线上，接入 Apple 用户态。当前目标版本固定为 iOS 16.5（20F66），先验收 dyld 和 Apple launchd。

## 复用选择

| 来源 | 已取得并采用的部分 | 本项目接入方式 |
|---|---|---|
| HackiOS / XNU 8796.121.2 | 与 R21 相同版本的内核源码、平台 / APFS / 兼容驱动、构建脚本 | 保留原平台实现，添加 Sony 的无 PAN、DT timer 和 GIC 顺序增量；复用其依赖准备流程 |
| HackiOS iOS 16.5 用户态 | 官方 IPSW ZIP64 / Range 提取、选取系统文件、libzbitmap、APFS 构建、bootstrap 和 QEMU 脚本 | 已按固定版本生成 root / system APFS 镜像；修正少量主机兼容和运行参数 |
| 一加五 UniXNU 设备仓 | ADT `p8,timer-intid` 绑定、RAM Data / 用户态依赖 / KeyStore / 显示集成记录和核对工具 | 直接复用 DT timer 机制，转换为本内核早期 SecureDT 接口；其余组件按 ABI 与实际故障选择 |
| 朋友 README | 项目来源、构建步骤和故障索引 | 对照公开源码与原始材料，已取得其公开引用的完整固定仓库；未取得其私人修改部分 |

一加五最新公开仓库已明显超出早期 P4 快照：维护者报告 iPadOS 17.7.11（21H461）界面已在硬件显示，触摸仍在开发。该路线使用 XNU 10063.141.1。当前 SO-04J 已验证的 XNU 8796 与 HackiOS 16.5 路线匹配，因此先沿这条兼容版本推进；一加五的板级地址、存储布局和 17.7 用户态组件不会直接放进 8796 镜像。来源：[一加五固定 README](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/2508a592d0672134cd97f081c7aea8cfab54a5e3/README.md)、[上游集成记录](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/2508a592d0672134cd97f081c7aea8cfab54a5e3/docs/P21_UPSTREAM_INTEGRATION.md)。

## 已完成的实质工作

- 阿里云已取回三套完整固定源码，归档哈希和展开大小保存在 `captures/stage3-20261027/source-status-08.json`。私有源码保留，另建构建副本。
- 本机保存了 14 份关键公开文件，并逐份核对 Git blob SHA-1 与 SHA-256：[来源清单](../research/stage3-reuse-20261027/sources.json)。
- 阿里云已通过上游 `xnu/apply.sh` 取得 Apple 基线 `c54f35ca767986246321eb901baf8f5ff7923f6a`，应用 73 个已有文件补丁和 104 个新增文件，然后成功检查 / 应用 Sony 增量。
- Sony 增量把 `__ARM_PAN_AVAILABLE__` 配为 0，移除强制 `+pan` 编译特性；从 ADT `/arm-io/timer` 的 `p8,timer-intid` 读取 PPI，并要求值在 16..31；将 ARE / Group1 配置移到 redistributor PPI 之前。源码：[sony-xnu8796](../stage3/sony-xnu8796/)。源内核已在阿里云生成，并通过 R22 的 A72 / SO-04J 自有 PID1 验收。
- 阿里云复用上游 ZIP64 选择器，仅用 102,589 字节的有界 HTTP Range 读取，核对 Apple BuildManifest：iOS 16.5、20F66、iPhone10,3 / 10,6、正确 OS / SystemOS / AppOS 组件。Manifest SHA-256 为 `7dd041dc7ac05f2e1f041013024f9062b8166d916c708ff8b46aca5e089e091f`。
- 首项云端任务已完成 Apple 镜像 CRC / SHA 校验、系统文件提取、43 个 arm64 dyld cache 文件选取和现成 APFS 镜像构建。原始报告与脚本已取回并核对 Git blob：[云端准备成果](../artifacts/stage3-cloud-20261028/reports/STAGE3-APPLE-USERLAND-20261027.md)。

## 运行验证接续

第一项云端任务生成的 `system.apfs` 为 2,492,203,008 字节；两次尝试均停在主机 UEFI / ROM 配置，尚无真实 dyld / Apple launchd 运行证据。

已对取回的脚本作小范围接续：

1. 用 `-nic none` 关闭 QEMU 默认网卡，消除离线运行仍请求 `efi-virtio.rom` 的问题。
2. 恢复上游 7 GiB 稀疏 guest 地址空间。system.apfs 位于物理 `0xfc000000`，4 GiB guest 无法覆盖整个镜像；7 GiB 地址映射按需驻留，运行内存受云端资源约束。
3. 复用现有匹配 workspace 和已下载镜像；如果任务容器重新初始化，则复用已取回的脚本重建工作目录。

运行接续已通过。动态测试程序 `hello_dyn` 经 Apple dyld / 共享缓存运行并返回预期状态 7；bootstrap 随后执行 `/sbin/launchd.ios`，Apple launchd 输出 Darwin Bootstrapper 7.0.0、完成早期启动，并经 xpcproxy 拉起服务。共一次完成的真实 guest 实验，7 GiB 稀疏地址空间、单 CPU、离线。原始 Apple launchd 文件大小 525,088 字节，SHA-256 为 `4c61512d19b9a55a6ddad27e23b88fc5008fbf77d0c0b5e6bdd7c7d90cef3e06`。

运行报告、输入 / 输出哈希和串口摘录：[STAGE3-APPLE-RUNTIME-20261028.md](../artifacts/stage3-runtime-cloud-20261028/reports/STAGE3-APPLE-RUNTIME-20261028.md)。取回的四份文件均核对 Git blob；完整原始 guest 日志留在云端忽略目录，报告保存了关键串口摘录。

可复现接续源码：[cloud-runtime](../stage3/cloud-runtime/)。任务：[Apple 用户态运行接续](PRIVATE_EXPERIMENT_REFERENCE)。`hello_dyn` 是项目的动态探针；真实 Apple 程序的执行证据来自后续 launchd 与 xpcproxy。该结果完成用户态 ABI / 启动依赖的仿真验收，SO-04J 的无 PAN 源内核及后续原生用户态接入继续推进。

## 资源分工

阿里云用于源码获取、版本核对、Sony 补丁、私有工具链及能在既有限额内完成的构建。其总内存约 1.83 GiB、可用约 1 GiB、无 swap；完整 iOS QEMU 工作集约 2.5–3 GiB，因此该项仿真经阿里云 Codex CLI 提交到指定 `baozi977/xzpios-labold` 环境。

阿里云没有安装全局依赖或替换系统库。私有 Clang 11.1.0 / cctools 已运行并生成 Darwin ARM64 对象，iPhoneOS 16.5 头文件已按固定提交取得；flex / bison / termcap / libbsd 等工具从已有 Debian 索引核对 SHA 后解包到私有前缀，UUID 复用已验收的 UEFI 私有依赖。MIG、csh、unifdef、XNU 头文件导出、firehose / pthread 内核库均已完成；源内核与 R22 运行结果见 [阶段三验收](STAGE3-ACCEPTANCE-20261028.zh-CN.md)。Clang11 的旧选项名已明确映射，保留自动变量清零和 sanitizer 列表的原行为。

全部共享 StateDirectory 的构建单元串行运行，避免所有者在并行准备中发生变化。手机已通过 R22 源内核验收并自动返回 Android，R21 保留为旧内核对照。

## 接下来的验收顺序

1. 私有工具链能生成 Darwin ARM64 对象；源内核完成无 PAN / DT timer 构建与指令检查。
2. 云端取得真实 dyld 与 Apple launchd 的启动证据及首个服务依赖结果。
3. 将源内核重新接入 Sony 加载器：增加 ADT timer 属性，根据新 Mach-O 导出装载位置和入口，复用现有临时启动 / 原始日志 / 返回流程。
4. 在手机上验证 Apple 最小用户态，再接入后续根卷和服务阶段。显示、SpringBoard 和输入沿已经审计的现成模块逐步适配。
