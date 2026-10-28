#!/usr/bin/env bash
# One-CPU, offline, bounded HackiOS QEMU run. Apple images remain under ignored out/.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
workspace=${WORKSPACE:-"$root/out/stage3/workspace"}
seconds=${1:-300}
case "$seconds" in ''|*[!0-9]*) echo "seconds must be an integer" >&2; exit 2;; esac
(( seconds >= 1 && seconds <= 600 )) || { echo "seconds must be 1..600" >&2; exit 2; }
cd "$workspace"
if [[ -z ${QEMU_EFI:-} && -f /usr/share/AAVMF/AAVMF_CODE.no-secboot.fd ]]; then
  export QEMU_EFI=/usr/share/AAVMF/AAVMF_CODE.no-secboot.fd
  export QEMU_VARS=/usr/share/AAVMF/AAVMF_VARS.fd
fi
# Explicit -nic none suppresses QEMU default NIC and its option ROM; NET being empty alone does not.
# Seven GiB is sparse guest address space: the 2.49GB system image starts at PA0xFC000000.
# It must fit below RAM_BASE0x40000000 + RAM_SIZE, without allocating all guest RAM.
exec timeout --foreground --signal=TERM --kill-after=20 "$((seconds + 60))" \
  env -u QEMU_NET QEMU_RAM_GB=7 QEMU_TB_MB=64 qemu/run-ios.sh "$seconds" \
  -smp 1 -cpu max -display none -nic none
