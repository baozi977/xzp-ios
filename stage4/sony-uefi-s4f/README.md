S4F accepted on SO-04J, 2026-10-28. Native XNU and static PID1 each read the same 64 KiB project fixture through the Sony UFS controller and match FNV 87e12b7b5c5c1189. Total: 20 reads, 128 KiB, zero errors, 14 heartbeats. The kernel restored the private READ standby queue; UEFI restored the original firmware queue and framebuffer, then returned to Android. Permanent boot/FOTAKernel hashes unchanged.

S4D refused a retained firmware WRITE descriptor. S4E introduced a verified READ-only standby queue but exposed an allocation inside the interrupt-disabled switch. S4F maps that command page during probe. The accepted kernel is 070717724b639c15051d8e02c0dffd1c679c6a4b29e32679fa727e8e89e7d69c.

Evidence: ../../captures/sony-stage4-20261028-s4f/verified-result.json and phone/entry-log.txt. Apple launchd native execution remains a later acceptance item.
