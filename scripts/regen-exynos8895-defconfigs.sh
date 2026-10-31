#!/usr/bin/env bash
# Regenerate all Exynos8895 board configurations against the same Kconfig tree.
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
regen_root=$(mktemp -d "${TMPDIR:-/tmp}/exynos8895-defconfigs.XXXXXXXX")
trap 'rm -rf -- "$regen_root"' EXIT
cd "$repo_root"
configs=(arch/arm64/configs/exynos8895-*_defconfig)
r7_required=(
    RT_GROUP_SCHED CPU_FREQ_STAT CPU_FREQ_DEFAULT_GOV_SCHEDUTIL
    CPU_FREQ_GOV_SCHEDUTIL CPU_FREQ_GOV_INTERACTIVE
    KSM ZRAM DEVTMPFS MEMORY_STATE_TIME
    BPF_SYSCALL KPROBES XFRM_STATISTICS INET_UDP_DIAG INET6_AH
    NF_CONNTRACK_PROCFS NETFILTER_XT_MATCH_BPF NETFILTER_XT_MATCH_L2TP
    NETFILTER_XT_MATCH_OWNER IP_NF_MATCH_AH NET_SCH_NETEM NET_CLS_BPF
    VSOCKETS PPP_FILTER PPP_MULTILINK PPPOE PPPOLAC PPPOPNS PPP_ASYNC
    PPP_SYNC_TTY EXT2_FS EXT2_FS_XATTR EXT2_FS_SECURITY EXT3_FS
    EXT3_FS_SECURITY NTFS_FS CRYPTO_RSA CRYPTO_CCM CRC_CCITT
    SCHEDSTATS TIMER_STATS DEBUG_ATOMIC_SLEEP DEBUG_LIST DPM_WATCHDOG
)
for config in "${configs[@]}"; do
    name=${config##*/}
    output="$regen_root/$name"
    mkdir -p "$output"
    make O="$output" ARCH=arm64 \
        ANDROID_VERSION="${ANDROID_VERSION:-100000}" \
        ANDROID_MAJOR_VERSION="${ANDROID_MAJOR_VERSION:-q}" "$@" "$name"
    while IFS= read -r required; do
        if ! grep -qxF "$required" "$output/.config"; then
            echo "$name lost required board/boot option: $required" >&2
            exit 1
        fi
    done < <(awk '/^CONFIG_(SOC_EXYNOS8895|SEC_(DREAMLTE|DREAM2LTE|GREATLTE)_PROJECT|MACH_[A-Z0-9_]+|RELOCATABLE_KERNEL|EXYNOS_VIDEO|EXYNOS_DECON_[A-Z0-9_]+|SND_SOC_SAMSUNG[A-Z0-9_]*|MOTOR_[A-Z0-9_]+|USB_CONFIGFS_[A-Z0-9_]+)=y$/ {print}' "$config")
    for required in CONFIG_FIMC_BNS_V4_20_0=y CONFIG_THERMAL=y CONFIG_EXYNOS_THERMAL=y CONFIG_CPU_THERMAL=y CONFIG_GPU_THERMAL=y CONFIG_ANDROID_TIMED_OUTPUT=y CONFIG_CPU_IDLE=y CONFIG_ARM64_EXYNOS_CPUIDLE=y CONFIG_RELOCATABLE_KERNEL=y CONFIG_RELOCATABLE=y CONFIG_SCHED_EHMP=y CONFIG_SCHED_TUNE=y CONFIG_CGROUP_SCHEDTUNE=y CONFIG_SCHED_USE_FLUID_RT=y; do
        grep -qxF "$required" "$output/.config" || { echo "$name missing $required" >&2; exit 1; }
    done
    for option in "${r7_required[@]}"; do
        grep -qxF "CONFIG_$option=y" "$output/.config" || {
            echo "$name lost r7 alignment option CONFIG_$option" >&2
            exit 1
        }
    done
    grep -qxF CONFIG_BLK_DEV_LOOP_MIN_COUNT=48 "$output/.config" || {
        echo "$name lost the r7 loop-device count" >&2
        exit 1
    }
    if grep -qxF CONFIG_ARM64_VHE=y "$output/.config"; then
        echo "$name enables VHE, unlike the r7 configuration" >&2
        exit 1
    fi
    if grep -qxF CONFIG_REGULATOR_S2MPS16=y "$output/.config"; then
        echo "$name selects removed S2MPS16 regulator" >&2
        exit 1
    fi
    if grep -Eq '^CONFIG_(COMPAT_VDSO|VDSO32)=y$' "$output/.config"; then
        echo "$name selects the removed compat vDSO backport" >&2
        exit 1
    fi
    if grep -qxF CONFIG_SOC_EXYNOS8890=y "$output/.config"; then
        echo "$name selects removed Exynos8890 platform" >&2
        exit 1
    fi
    if grep -qxF CONFIG_ARM_GIC_V3=y "$output/.config"; then
        echo "$name selects a GICv3 controller on the GICv2 platform" >&2
        exit 1
    fi
    if grep -qxF CONFIG_KALLSYMS_BASE_RELATIVE=y "$output/.config"; then
        echo "$name lost the Samsung absolute kallsyms dump ABI" >&2
        exit 1
    fi
    if grep -qxF CONFIG_SCHED_HMP=y "$output/.config"; then
        echo "$name still selects legacy HMP" >&2
        exit 1
    fi
    make O="$output" ARCH=arm64 \
        ANDROID_VERSION="${ANDROID_VERSION:-100000}" \
        ANDROID_MAJOR_VERSION="${ANDROID_MAJOR_VERSION:-q}" "$@" savedefconfig
    test -s "$output/defconfig"
done
# Publish only after every board passed configuration and invariant checks.
for config in "${configs[@]}"; do
    cp -- "$regen_root/${config##*/}/defconfig" "$config"
done
printf 'Regenerated and checked %s Exynos8895 defconfigs.\n' "${#configs[@]}"
