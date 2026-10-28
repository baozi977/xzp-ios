# 阶段 1：Sony 原生入口与日志闭环

日期：2026-10-27，Asia/Hong_Kong。**首个实机验收已通过：自制入口载荷执行、日志跨复位保留、Android 取回日志、正常系统恢复。**

## 实机取得的结果

使用 `fastboot boot` 临时加载自制 ARM64 Image。入口代码占 4096 字节，直接记录 Bootloader 交付状态，随后通过 Qualcomm 复位路径返回正常 Android。载荷不初始化存储和其他 CPU。

[原始入口日志](../captures/stage1-entry-probe-20261027-c/phone/entry-log.txt)：

```text
XZPIOS_STAGE1_20261027_ENTRY_C
CurrentEL_raw=0x0000000000000004
SCTLR_entry=0x0000000000c00800
boot_x0_fdt=0x0000000083200000
boot_x1=0x0000000000000000
boot_x2=0x0000000000000000
boot_x3=0x0000000000000000
MIDR_EL1=0x0000000051af8014
MPIDR_EL1=0x0000000080000000
ID_AA64MMFR0_EL1=0x0000000000001122
ID_AA64MMFR1_EL1=0x0000000000000000
ID_AA64MMFR2_EL1=0x0000000000000000
CNTFRQ_EL0=0x000000000124f800
XZPIOS_STAGE1_END_RESET_SMC
```

| 输入 | 实测结论 | 对下一步的作用 |
|---|---|---|
| CurrentEL | EL1 | Sony 入口按 EL1 交接处理 |
| SCTLR | MMU、数据缓存均关闭 | 初始代码可按物理地址访问已核定的日志区 |
| x0 | DTB 位于 `0x83200000` | 与此前 Linux 布局证据直接对应 |
| x1–x3 | 全零 | 符合 ARM64 Linux boot protocol 的初始约定 |
| MPIDR | affinity `0x0` | 初期单核从 CPU0 启动 |
| MMFR0 / MMFR1 | `0x1122` / `0` | 与 CPU0 的原内核模块测量一致 |
| MMFR2 | `0` | 启动核 UAO 字段为 0；CPU1–7 仍待独立测量 |
| CNTFRQ | 19,200,000 Hz | 为计时器与 XNU timebase 提供实际入口值 |

恢复后 `4.4.302-xzpios-fit` 运行、`sys.boot_completed=1`、root 可用，`/proc/modules` 为空。boot SHA-256 仍为 `4d3dd1eee8df36bdb6b85198bb478e1f3173af89d791f12ace257ec63cb731e1`，FOTAKernel 仍为 `019d60fbef413647f2a49c1391d12d64771a63813d8cbcbbbaa30429f112b985`。

独立核对记录：[verified-result.json](../captures/stage1-entry-probe-20261027-c/verified-result.json)。测试前没有该构建标记，第一次返回 Android 后取得完整标记及结束行。

## 构建与打包

全部编译和 QEMU 在原阿里云私有 lab 执行，未使用云端任务。复用 GCC 9.4 / binutils 2.35.2；单次 unit 约 6 秒，MemoryMax=512 MiB、CPUQuota=25%、无 swap。三个版本构建前后，四个原业务服务的 PID、状态、启动时间及重启次数均相同。

源码：[entry.S](../stage1/entry-probe-c/entry.S)、[link.ld](../stage1/entry-probe-c/link.ld)。可用产物位于 [stage1-entry-probe-20261027-c](../artifacts/stage1-entry-probe-20261027-c/)，boot SHA-256 为 `4e14542ef551f457dbc95a6fd0b8a29cec3d70553a4a142350fb4fc2ca688cfd`。

Image 使用 ARM64 boot header，text_offset=`0x80000`，占用区间 `[0x80080000,0x80081000)`。打包保留本机原始 ramdisk、四棵 DTB 和 boot header 的载入地址。临时启动未写入 boot 分区。

QEMU Cortex-A72 验证了入口代码与 430 字节 console 记录的签名、长度、寄存器输出和结束标记；Sony 复位及跨启动留存由本次实机验证。

## 日志与复位路径

日志位于本机已测得的 ramoops console：`[0xffcc0000,0xffd00000)`。使用 Linux 4.4 `persistent_ram_buffer` 的 12 字节头和签名 `0x43474244`，每写完一条记录提交长度，并清理缓存至 PoC。

| 版本 | 变化 | 手机结果 |
|---|---|---|
| A | 未缓存入口检查，通用 PSCI SYSTEM_RESET | 回到 Android，未回收标记 |
| B | 增加 cache clean，仍用 PSCI reset | 回到 Android，未回收标记 |
| C | 保持 B 的日志代码，改为原 fit 源码的 Qualcomm 复位路径 | 完整日志回收成功，Android 恢复 |

C 参照固定 Linux 源码 `drivers/power/reset/msm-poweroff.c`：SCM halt PMIC arbiter、SCM deassert PS_HOLD，以及直接 PS_HOLD 回退。ARMv8 SCM 编码对应 `0xc2000901` / `0xc2000902`；`0x10ac000` 的 PS_HOLD 地址已与本机 DT `/soc/restart@10ac000` 核对。Android 的 reboot-bootloader 路径提前设置了保留内存的 warm-reset 策略。

后续固件早期日志应沿用已验证的 Sony 复位路径；通用 PSCI reset 在这两次尝试中没有回收到标记。

## 当前进度与下一项

阶段 1 的**入口和日志子阶段已完成**。完整 Sony UEFI、GOP 和 XNU 加载器整合尚未完成。

接下来将这个已测定的入口 / 日志 / 复位约定接入 Sony UEFI：先在内存图排除 debug / pstore，再加入最小固件启动与日志模块。显示物理基址、ABL 阶段 stride、像素格式作为独立测量项；暂不初始化 UFS。之后进入 XNU 的 boot_args / ADT 交接。
