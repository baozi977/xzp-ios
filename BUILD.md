# 构建说明

1. 按upstreams.lock.json取得固定EDK2/UniXNU/HackiOS依赖；Sony DSC中包路径需要与实际PACKAGES_PATH一致。UniXNU菜单派生文件保留BSD-2-Clause-Patent头。
2. 对HackiOS XNU8796源码应用stage3/sony-xnu8796/中的补丁。沿用匹配iPhoneOS16.5 SDK、Theos/cctools和私有Clang11适配；stage3/xnu-source-r22/build_worker.py记录命令和Mach-O/PAN/未定义符号审计。SDK需自行合法取得。
3. stage4/sony-xnu-storage/kernel_worker.py记录原生UFS接入与源树恢复流程；SonyUfsContract.h固定共享ABI，test_extent.c可用宿主C11编译运行，验证多段/非对齐/范围与错误处理。
4. 以stage2/pid1-heartbeat或stage4/pid1-apfs-j为PID1输入，scripts/build_pid1_macho.py记录私有GCC/binutils及静态Mach-O封装。按实际内核/PID1生成KernelContract.h。
5. S4J实机1088 MiB范围[a4000000,e8000000)，4 KiB硬件翻译、16 KiB软件页。恰好1 GiB会选择4 KiB软件页并拒绝共享缓存16 KiB签名单元。交接context应有完整、对齐的16 KiB物理空间；见SonyUfsPrepare.h和fix_stage4j_page_policy.py。
6. APFS构建改动见stage4/apfs_stream.py和build_apfs_images.py，64 KiB单进程分块压缩，资源叉磁盘暂存。Apple镜像和SDK不分发。先验证与原编码输出一致，再完整读取/解压检查镜像。
7. SonyFiles.h和SonyFixture.h是原实机项目文件的历史区段/校验输入，不能直接用于另一台手机或移动后的文件。用extent_probe.c在目标设备重新测量，只读核对文件SHA-256、加密标志、分区范围与每段原始块，再生成对应表。
8. 历史构建：640 MiB、单CPU、无swap；A72固件往返仿真512 MiB。完整Apple ABI仿真曾使用较大云环境，S4J手机运行另有实机证据。此公开包未在全新机器做端到端重建，不提供一键刷写脚本。原生测试使用临时fastboot boot，保留备份与返回日志。

当前公开包是可审阅的移植源码和实验复现输入，包含历史路径适配。下一步工程工作是将工具链与板值输入参数化，建立干净环境构建入口。
