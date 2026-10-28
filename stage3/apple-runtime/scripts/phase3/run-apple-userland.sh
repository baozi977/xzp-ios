#!/usr/bin/env bash
# Validate and run the fixed HackiOS phase-3 workspace. Apple inputs stay in ignored out/.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
workspace=${WORKSPACE:-$repo/out/stage3/workspace}
seconds=${1:-240}
[[ $seconds =~ ^[0-9]+$ ]] && (( seconds > 0 && seconds <= 600 )) || {
  echo "duration must be 1..600 seconds" >&2; exit 2;
}
cd "$workspace"
[[ $(sha256sum prebuilt/mach.development.bcm2837 | cut -d' ' -f1) == 9239d4adb7a2c433d7ed5729b47b63acc5281af73d8a81640ef3c678c8afd26c ]]
[[ -f userland/root.apfs && -f userland/system.apfs ]]
python3 - userland/system.apfs <<'PY'
import os, sys
size = os.path.getsize(sys.argv[1])
rd_end = 0xFC000000 + size
ram_end = 0x40000000 + (7 << 30)
print(f"system_size={size} rd_end=0x{rd_end:x} ram_end=0x{ram_end:x}")
if rd_end > ram_end:
    raise SystemExit("system image exceeds 7 GiB guest physical layout")
PY
export QEMU_EFI=${QEMU_EFI:-/usr/share/AAVMF/AAVMF_CODE.no-secboot.fd}
export QEMU_VARS=${QEMU_VARS:-/usr/share/AAVMF/AAVMF_VARS.fd}
export QEMU_RAM_GB=7 QEMU_TB_MB=64 QEMU_SMP=1
exec ./qemu/run-ios.sh "$seconds"
