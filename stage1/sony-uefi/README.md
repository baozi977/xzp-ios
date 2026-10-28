# Sony SO-04J headless UEFI probe

The first integration target is SEC relocation → stack → 4 KiB MMU → decompression → DXE dispatch → retained log → the Sony warm-reset path.

Inputs are edk2-msm8998 `ace27110b9b9b6c591387d50cf748ec646402b4a`, its edk2 gitlink `e7aac7fc137e247edad22f7ee53b9a1fba227397`, and Brotli gitlink `f4153a09f87cbb9c826d8fc12c74642bb2d879ea`. The source fetch manifest records archive hashes. Derived ARM platform code and common library mappings retain their upstream license comments.

Build on Aliyun using `work/scripts/stage_uefi_sources.py` and the generated reviewed Workbench upload/launch scripts. The transient build unit is single-job, one CPU, 640 MiB, no swap or network; UUID headers and static library are extracted privately from the pinned Debian package. BaseTools' initial build passed 302 upstream tests.

Address plan (half-open intervals):

| Purpose | Physical range |
|---|---|
| Android-protocol source Image | `0x80080000–0x80280000` |
| Relocated 2 MiB FD | `0xa0000000–0xa0200000` |
| DXE usable arena | `0xa0200000–0xa4000000` |
| PrePi HOB/stack arena | `0xa0400000–0xa4000000` |
| Primary stack | `0xa3fc0000–0xa4000000` |
| FDT pointer capture | first 8 bytes at `0xffb00000` |
| Sony debug + pstore reservation | `0xffb00000–0xffd00000` |
| Ramoops console | `0xffcc0000–0xffd00000` |
| PS_HOLD fallback page | `0x010ac000–0x010ad000` |

The DXE allocator sees only the 64 MiB test arena, with the FD allocated as boot-services data. Other SO-04J RAM is not exposed for allocation in this probe. This avoids the overlapping OnePlus template map and preserves Sony fixed reservations. No other handset's DXE binaries, GOP, storage, USB or ACPI drivers are included.

The DXE probe runs early via APRIORI, records its entry and control registers, delays one second, then resets the measured Sony boot CPU through Qualcomm SCM `0xc2000901/2` and PS_HOLD fallback. Cortex-A72 in QEMU stops at an explicit marker before the Sony-only reset call. Android's reboot-bootloader path sets the warm-reset PMIC policy, as verified by entry probe C. This target tests DXE dispatch; additional architectural protocols and a loader are subsequent work.

The relocation helper uses `adr` with its function symbol, rather than the current instruction address. Adding interrupt masking before the original `adr .` shifted the computed copy source by four bytes; QEMU exposed the resulting invalid absolute GUID pointer. The corrected helper preserves TE packaging and verifies the relocated Image header byte-for-byte.

Run QEMU before packaging with the four byte-identical original Sony DTBs and original boot ramdisk/header. Phone execution uses `fastboot boot` only. Preserve the first returning Android's pstore before another reboot.
