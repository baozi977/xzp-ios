# Accepted Sony UEFI / UniXNU menu U07

Accepted on SO-04J on 2026-10-28. This is the graphics baseline for subsequent native boot experiments. The original handover and accepted R22 kernel remain unchanged.

The fixed Sony GOP retains the measured ABL 1080x1920 BGR888 surface (physical0x9d400000, stride3240), exports standard BLT operations, and takes ownership of splash autorefresh through the Linux MDSS TE/idle/AR sequence. Every submitted frame waits for freshly latched PP0/PP1 completion bits. Cleanup restores the original image at PoC and original refresh register values.

Existing UniXNU Menu/Ui/Font/Logo from commit0fc0fc1f9ddd0fbd4787738afef38667df5e0a95 provide the portrait interface. The scheduler polls cooperatively in this minimal firmware. The menu shows XNU/PID1 test, Return to Android and Display test. Sony PM8998 status reads provide volume up/down and power confirmation; all addresses and GPIO identities come from the measured Sony DT. The test menu has a 90-second unattended bound.

Native acceptance: 6 key events, zero read failures, XNU selected from menu, 14 consecutive PID1 heartbeats, timed return, unchanged display memory across XNU, fresh display completion after return, 10 total completed display submissions, and automatic Android recovery with unchanged boot/FOTAKernel hashes. Owner confirmed normal menu movement, confirmation and display.

Build and small QEMU ran only on the existing Aliyun private toolchains and resource limits. The phone uses fastboot boot. Artifacts: work/artifacts/sony-uefi-ui-20261028-u07/. Evidence: work/captures/sony-uefi-ui-20261028-u07/. Report: work/reports/UEFI-UI-ACCEPTANCE-20261028.zh-CN.md.

The menu's XNU entry runs the accepted standalone PID1. Native Apple userland and the later iOS display/input pipeline remain subsequent project stages. Use a new version tag and capture directory for modifications to this frozen accepted firmware.
