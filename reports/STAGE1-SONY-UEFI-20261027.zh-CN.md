# SO-04J 原生 UEFI SEC / MMU / DXE 实机验证

2026-10-27，Sony Xperia XZ Premium SO-04J，序列号 `DEVICE_SERIAL`。

## 结果

通过 `fastboot boot` 临时启动自建 Sony 固件，实机完成入口重定位、栈初始化、4 KiB MMU、固件卷解压、DXE Core 和自建 DXE 驱动执行，再经 Qualcomm SCM 复位返回 Android。首个返回 Android 中取回完整 pstore 日志，独立校验通过。

关键日志：

```text
SONY_MEMORY_BEGIN base=A0400000 size=3C00000
SONY_MMU_RETURN status=Success table=A3FBF000 size=10
Loading DxeCore at 0x00A3EC3000 EntryPoint=0x00A3EE1918
XZPIOS_SONY_UEFI_DXE_REACHED_20261027_A
Sony DXE: EFI revision 20046, table A3EB0018, boot services A3EEE190
SONY_DXE_REGS MIDR=51AF8014 TCR=280803518 SCTLR=C0180D
XZPIOS_SONY_UEFI_RESET_20261027_A
```

`TCR_EL1.TG0=00`，证明当前使用 4096 字节翻译粒度。`SCTLR_EL1.M/C/I=1`，MMU、数据缓存和指令缓存已开启。启动 CPU 为实测 Kryo CPU0。

Android 返回后 `sys.boot_completed=1`，内核为 `4.4.302-xzpios-fit`，未加载模块。boot 与 FOTAKernel 校验保持：

- boot：`4d3dd1eee8df36bdb6b85198bb478e1f3173af89d791f12ace257ec63cb731e1`
- FOTAKernel：`019d60fbef413647f2a49c1391d12d64771a63813d8cbcbbbaa30429f112b985`

## 源码与构建

源码目录：`work/stage1/sony-uefi/`。远端目录：`/opt/xzpios-lab/sony-uefi-20261027`。

依赖固定为 edk2-msm8998 `ace27110b9b9b6c591387d50cf748ec646402b4a`、对应 edk2 `e7aac7fc137e247edad22f7ee53b9a1fba227397`、Brotli `f4153a09f87cbb9c826d8fc12c74642bb2d879ea`。归档 SHA-256 见 captures 下 `fetch-result.json`。

全程在既有阿里云私有实验目录构建，使用既有 GCC 9.4 / binutils 2.35.2，单个编译任务，内存上限 640 MiB，CPU 上限 100%，无 swap、无构建网络。UUID 开发文件从固定 Debian 软件包提取到私有目录。BaseTools 初次构建通过 302 项上游测试。四个既有服务的 PID、运行状态和重启计数与构建前一致。

保留日志库直接使用已验证的 Sony ramoops console，并在 MMU 开启前后清理缓存。DXE 分配器仅能使用专用的 64 MiB 测试内存范围，其中 2 MiB FD 单独保留；Sony pstore、debug、splash、固件保留区均不向分配器开放。未包含其他机型的二进制驱动。

## 修复及验证

模板保留了非目标平台依赖与重叠内存描述，已换为独立 Sony 入口、日志及内存初始化。入口屏蔽中断后，旧 `adr .` 的当前位置相对函数入口偏移了 4 字节，导致复制源后移、静态 GUID 指针错误。改为 `adr` 函数入口符号，QEMU 同时验证重定位后镜像头与原始输入逐字节一致，以及 GUID 指针、MMU 和 DXE 执行。

QEMU 使用同一个 FD、Cortex-A72、单核、稀疏 RAM 文件，内存上限 512 MiB。在显式停止标记前验证完整启动链；Sony SCM 复位在实机验证。原始控制台日志与结果 JSON 均已保存。

## 产物与证据

- `work/artifacts/sony-uefi-20261027-a/boot.img`，SHA-256：`088c7525981be9b269b9690234cd8afc454a8e33612dea6eef820f73f0109214`
- 同目录 `Image`，2 MiB，SHA-256：`b60b4fa6e4345f29966bac8dfd66f6ce678883fd4cd02e142b6ee9f4ed07c3dc`
- `work/captures/uefi-sony-20261027/verified-result.json`
- `work/captures/uefi-sony-20261027/phone/entry-log.txt`
- `work/captures/uefi-sony-20261027/phone/after-pstore/console-ramoops-0`，SHA-256：`2442d655c2202e22519e6c8f13746bd355b7b0a2008fe188dee0c4fa22d998e0`
- `work/captures/uefi-sony-20261027/qemu-console.txt`

打包保留了原始四棵 DTB、ramdisk 和 Sony boot v0 头部参数。实机仅执行临时启动。

## 下一步

实测 ABL 保留的 framebuffer 地址、行跨度、像素格式，再接入 Sony GOP 和加载器所需的 UEFI 基础协议。随后整合已修复 PAN/UAO 检查的 xnuloader、固定 XNU 装载内存图并进入 XNU。当前证据到原生 DXE 驱动为止；尚未执行 XNU 或 iOS 用户空间。
