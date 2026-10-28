# SO-04J read-only boot display probe B

Derived from the verified `sony-uefi` probe A in a separate local and Aliyun build directory. It retains the corrected relocation helper, 4 KiB MMU, pstore serial library and Sony reset path, and adds a device mapping for the measured MDSS register range `[0x0c900000,0x0c990000)`.

On the measured Sony boot CPU only, the DXE probe reads 248 registers in the eight VIG/DMA SSPP blocks, five CTL blocks, four layer mixers and four interfaces listed by the running SO-04J DTB. Offsets and decoding come from the fixed current Linux source. It writes no display register or framebuffer byte.

The same FD passed QEMU's SEC/MMU/DXE checks, with its Sony MMIO branch skipped on Cortex-A72. Native temporary boot returned the complete display snapshot through pstore, followed by automatic reset and Android boot completion. See `work/captures/sony-display-20261027/display-contract.json` and the raw first-return pstore.

Measured boot buffer: `0x9d400000`, 1080×1920, BGR888, 3240 bytes per row, two 540×1920 pipes. CTL0 and CTL1 have command-mode bit 17 set. A future GOP needs the corresponding pixel format and command transfer path; the current Linux fb0's 32-bit stride is a separate configuration.
