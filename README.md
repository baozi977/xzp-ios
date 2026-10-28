# XZPIOS

Sony Xperia XZ Premium SO-04J / MSM8998 原生 XNU 与 iOS 用户态移植研究。

截至2026-10-28，阶段一至四已完成：Sony UEFI入口与日志；原生XNU / 自有PID1；UEFI图形与实体按键UniXNU菜单；Apple APFS根环境、dyld共享缓存、原始Apple launchd及基本服务。

| 阶段 | 实机结果 | 源码 |
|---|---|---|
| 1 | SEC / 4 KiB MMU / DXE，保留日志，自动回Android | stage1/ |
| 2 | 原生Darwin22.5.0 / XNU8796.121.2；R21两次测试各14次PID1心跳 | stage2/ |
| 3 | 无PAN源码内核，Sony显示修复、UniXNU音量/电源键菜单；Apple ABI仿真 | stage3/ |
| 4 | 原生APFS根、Apple dyld、launchd Early boot、notifyd和cfprefsd功能调用 | stage4/ |

S4J：init 36项检查0失败；动态探针退出7；IOSurface接口80项检查0失败；2427次UFS读取、56,209,408字节、0错误。90秒定时回UEFI并恢复Android。Apple锁屏/SpringBoard、物理图形呈现、触摸、持久Data和长期运行属于后续阶段。

详细结果见 [阶段四验收](reports/STAGE4-ACCEPTANCE-20261028.zh-CN.md) 和 [核验摘要](evidence/sony-stage4-20261028-s4j/verified-result.json)。报告中原始captures/artifacts路径指向私有实验记录，不随公开包分发。本仓库提供脱敏的验收摘要；不将摘要当作完整原始日志。

## 构建与复现

见 [BUILD.md](BUILD.md)。依赖提交固定在upstreams.lock.json。保留了接受版本的板级源码、XNU补丁、PID1、区段映射器和APFS流式编码改动。源码中的/opt/xzpios-lab是公开化后的实验目录占位，build_worker.py是历史工具链适配记录，需要按本机依赖路径调整。

## 许可

XZPIOS原创代码采用MIT；上游派生文件保留各自头部许可证。见 [第三方来源](THIRD_PARTY_NOTICES.md)。不包含Apple固件、用户态镜像、SDK、分区备份、原始设备日志、账号凭据或服务器连接资料。
