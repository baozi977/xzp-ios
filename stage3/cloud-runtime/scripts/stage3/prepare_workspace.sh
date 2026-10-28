#!/usr/bin/env bash
# Prepare exactly the two pinned HackiOS repositories in their intended merged layout.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
dest=${1:-"$root/out/stage3/workspace"}
if [[ -e "$dest" ]]; then
  test "$(git -C "$dest" rev-parse HEAD)" = d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c
  test "$(git --git-dir="$dest/.userland.git" --work-tree="$dest" rev-parse HEAD)" = a79ff0772a3454fd1ef127e181e5b3bce6c9ce18
  echo "Reusing the matching existing workspace; keep all downloaded/extracted inputs."
  exit 0
fi
git clone --quiet --no-checkout https://github.com/oosexgang/hackios-xnu-kernel.git "$dest"
git -C "$dest" fetch --quiet origin d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c
git -C "$dest" checkout --quiet --detach d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c
# common/setup.sh requires a branch name; make a local name at the pinned detached commit.
userland=$(mktemp -d)
trap 'rm -rf "$userland"' EXIT
git clone --quiet https://github.com/oosexgang/hackios-xnu-userland.git "$userland"
git -C "$userland" fetch --quiet origin a79ff0772a3454fd1ef127e181e5b3bce6c9ce18
git -C "$userland" branch -f stage3-pinned a79ff0772a3454fd1ef127e181e5b3bce6c9ce18
(cd "$dest" && USERLAND_URL="$userland" USERLAND_REF=stage3-pinned common/setup.sh)
test "$(git --git-dir="$dest/.userland.git" --work-tree="$dest" rev-parse HEAD)" = a79ff0772a3454fd1ef127e181e5b3bce6c9ce18
(cd "$dest" && patch -p1 < "$root/scripts/stage3/patches/0001-prebuilt-optional-drawable-test.patch")
sha256sum "$dest/prebuilt/mach.development.bcm2837"
