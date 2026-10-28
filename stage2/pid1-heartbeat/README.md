# XZPIOS 自有最小 PID1

`pid1.c` 为独立的 Darwin arm64 程序。打开 `/dev/console`，读取实际 PID / UID，随后通过 `select` 的一秒超时输出心跳，持续运行。没有 libc、dyld 或外部动态库。

在阿里云使用现有私有 GCC9 / binutils 生成 ELF 和平坦代码，再由 `work/scripts/build_pid1_macho.py` 包装为静态 arm64 Mach-O。入口 `0x1000040a0`，文件 17,024 字节，SHA-256 为 `b4d305cd37a676ff6bfd5c8c67fc0ca49d7fa678d787b02df40d2acb3d92c16e`。编译资源限制为 128 MiB、CPU 50%、60 秒。

系统调用编号来自 Apple XNU `xnu-8796.121.2` 的 `bsd/kern/syscalls.master`：write=4、open=5、getpid=20、getuid=24、select=93。源码、链接脚本和产物哈希均记录在 `work/artifacts/pid1-heartbeat-20261027/manifest.json`。

运行验收要求原始内核日志同时包含 `XZPIOS PID1 START pid=1 uid=0` 和至少三次心跳。编译成功本身不代表运行通过；运行结果保存在对应 XNU 实验的 captures 目录。
