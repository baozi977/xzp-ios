# 阶段四：XNU 原生 UFS 文件读取验收

2026-10-28，SO-04J 实机 S4F 通过。XNU 内核和 PID1 经实际 UFS 控制器分别读取同一份65,536字节项目文件，FNV-1a均为 `87e12b7b5c5c1189`。累计20次硬件读取、131,072字节、0错误，随后输出14次连续心跳。

文件位于 Android 未加密的项目目录 `/data/xzpios-stage4/`。只读 FIEMAP 获取实测区段，XNU 将这些区段发布为 `/dev/ufs_fixture` 和 `/dev/rufs_fixture`；文件不需要复制到内存根盘。多段区段、非对齐读取、范围拒绝及错误传递已通过映射器测试。

两个实机失败定位后修复：

- S4D：固件保留的上一条命令是 WRITE(10)，上游只读交接检查拒绝接管。S4E/F 新建独立只读待机队列，先执行已知32 KiB测试文件的 READ(10)并核对CRC；XNU在此待机队列和自己的队列之间切换。
- S4E：交接时关闭中断的路径首次映射命令页，触发分配断言。S4F 将映射提前至 probe，切换阶段仅使用已核对的缓存映射。

S4F 结束时，XNU恢复待机队列，UEFI再恢复原固件队列；显示内存CRC前后一致，刷新和TE/AR状态恢复。手机自动回到Android，boot/FOTAKernel哈希与基线一致。

验收记录：[独立核验结果](../captures/sony-stage4-20261028-s4f/verified-result.json)、[实机保留日志](../captures/sony-stage4-20261028-s4f/phone/entry-log.txt)、[源码](../stage4/sony-uefi-s4f/)、[XNU适配源码](../stage4/sony-xnu-storage/)。

源内核SHA-256：`070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c`。临时boot镜像SHA-256：`d7ee5b917d133c9ec4f4d15fd81264e24401e316e6006a6eb75f75aad4f3338a`。

Apple镜像已在阿里云构建完成：启动工具卷4,194,304字节，系统卷2,608,537,600字节。两卷均通过固定7-Zip23.01完整读取/解压检查；镜像内原始Apple launchd哈希为 `4c61512d19b9a55a6ddad27e23b88fc5008fbf77d0c0b5e6bdd7c7d90cef3e06`。构建使用单进程64 KiB分块压缩和磁盘暂存，640 MiB内存限额内完成，未调用云端。

下一验收为：只读挂载系统卷，校验Apple文件，复用 `kern.redwood_switch_root` 切换根目录，接入固定用户态init，再观察dyld、Apple launchd及xpcproxy。S4F完成的是存储读取验收。
