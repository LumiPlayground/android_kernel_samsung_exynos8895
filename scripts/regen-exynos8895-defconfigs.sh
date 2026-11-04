#!/usr/bin/env bash
# Generate every board configuration before replacing any saved defconfig.
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
regen_root=$(mktemp -d "${TMPDIR:-/tmp}/exynos8895-defconfigs.XXXXXXXX")
trap 'rm -rf -- "$regen_root"' EXIT
cd "$repo_root"
configs=(arch/arm64/configs/exynos8895-*_defconfig)
for config in "${configs[@]}"; do
    name=${config##*/}
    output="$regen_root/$name"
    mkdir -p "$output"
    make O="$output" ARCH=arm64 "$@" "$name"
    if ! grep -qx 'CONFIG_RELOCATABLE_KERNEL=y' "$output/.config"; then
        echo "Samsung relocation support missing from $name" >&2
        exit 1
    fi
    if [[ "$name" == *dream* ]] &&
       ! grep -qx 'CONFIG_MAX77865_FG_ADAPTATION_WA=y' "$output/.config"; then
        echo "Fuel gauge workaround missing from $name" >&2
        exit 1
    fi
    make O="$output" ARCH=arm64 "$@" savedefconfig
done
for config in "${configs[@]}"; do
    cp -- "$regen_root/${config##*/}/defconfig" "$config"
done
printf 'Regenerated %s Exynos8895 defconfigs.\n' "${#configs[@]}"
