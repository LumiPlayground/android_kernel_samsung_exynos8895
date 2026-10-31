# Exynos8895 Linux 4.9.191 port

Target: Korean Galaxy S8+ (dream2ltekor), sep-10.0 userspace.
Rebased vendor baseline: `5ead67fe54456d27fee98698b9bec5bd20b40c96`.
Original vendor tip: `6f1fa95bdd2d0fae1c38c05ed43563f0ab288bb3`.
Upstream target: v4.9.191 (`bf489db05ebf7106dd77f79e6edabc45fc318416`).
Working branch: `port/exynos8895-4.9.191`.

## Continued r7 shared-core alignment

Reference: Lunarixus android-13-oneui, commit
`72aa73808788511125610fe83528bdd88af8d94a` (exynos9810-r7).
This pass aligns shared 4.9 behavior while preserving the Exynos8895
platform and sep-10.0 interfaces:

- Futex shared keys use inode sequence numbers, including inode initialization
  and the matching key layout, avoiding inode references in wait queues.
- Writeback tracks queued inodes under i_lock, and preserves the memcg pointer
  when clearing PG_writeback because the page can become free immediately.
- Epoll checks graph paths before registering wait queues and holds file
  references throughout reverse-path checking.
- Sticky-directory creation checks snapshot parent ownership/mode before
  pathname traversal; userfaultfd registration requires VM_MAYWRITE.
- POSIX message queues and utimes pass mount information to existing VFS
  helpers. The missing lookup_one_len2 declaration is supplied explicitly.
- CPU idle accounting is saved before timer teardown and restored during timer
  preparation, including idle_calls. The previous ARM64 SMP calls are removed
  to avoid duplicate accounting. Tick-disabled builds have internal stubs.
- Snapshot PM tracing recognizes the actual PM notifier chain, rather than
  treating matching event numbers on unrelated chains as suspend events.
- Pipe-buffer initialization clears flags; GSO handles untrusted fragmented
  buffer layouts; IGMP source checks lock the list; IPv6 address notification
  tolerates a missing host route; ICMP credits follow r7 randomization.
- Block queue mapping distributes online CPUs across hardware queues and maps
  offline CPUs safely; discard honors REQ_SYNC; max sectors updates io_pages.

All six defconfigs regenerate unchanged. RELOCATABLE_KERNEL, the Samsung
header-adjacent entry, EHMP hardware guards, FMP/eCryptfs interfaces, GPU,
bcmdhd4361 and FIMC-IS2 are retained. Upstream mmap/vmalloc/IRQ and script
interpreter fixes that r7 lacks are retained. The 9810 RKP/FIVE, diskcipher,
energy tables and Android 13 userspace feature dependencies are not copied
wholesale into the sep-10.0 target.

The header-adjacent entry assertion now compares `stext - _text` to 0x40.
Linaro's linker falsely rejects the previous absolute-address equality under
PIE/shared linking despite identical map addresses. A minimal link regression
accepts the correct offset and rejects 0x44; entry code remains unchanged.

Validation: GCC 6.3.1 normal dream2ltekor Image/Image.gz and DTBH build passed.
Selected core objects also compile with MEMCG, PM_SLEEP and TICK_ONESHOT off,
and USERFAULTFD on. Host tests of the actual block mapper pass dense/sparse
CPU masks, SMT, offline CPUs, excess queues and allocation failure under
AddressSanitizer/UBSan. Eight modeled startup cases and raw payload checks
pass; EL/MMU/cache operations remain modeled. All six saved configs and
protected driver trees are unchanged.

Payloads: `/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-r7-shared-core/`.
Comparison records and build logs: `/workspace/kernel-env/r7-shared-core/`.
Across kernel/mm/fs/ipc/lib/block/crypto, approximately 89% of shared paths
already match r7 byte-for-byte after this pass; this is a source comparison,
not a claim of runtime equivalence. Remaining differences include upstream
fixes, platform compatibility and separate feature backports.
Physical boot and the original DPU/SysMMU fault remain unverified.

## Samsung entry-layout compatibility candidate

The working 4.4 Image places `stext` immediately after its 64-byte header:
its first instruction is `0x14000010`, branching to Image + 0x40. The prior
4.9 image instead branches to `.init.text` approximately 21 MiB later, and
contains an undefined instruction at +0x40. For Exynos8895 with Samsung
RELOCATABLE_KERNEL enabled, primary startup now stays in `.head.text`,
restoring the working header-adjacent entry layout. Native 4.9 relocation,
page tables, MMU setup and the Samsung x1/x2 boot argument ABI are retained.

This is a compatibility candidate: there is no evidence yet that the failing
bootloader actually enters at +0x40. It must be tested on the device before
calling the boot failure fixed. The existing last_kmsg driver cannot capture
a failure before its initialization; adding later logging does not resolve
such a failure.

The candidate uses the normal dream2ltekor defconfig, hlng@longa identity,
and no early RAM/UART tracing. All six saved defconfigs remain unchanged.
Modeled startup reaches start_kernel from both Image and Image + 0x40 at
four loads, including the historical Samsung load 0x80148000. The old
normal Image faults when entered directly at +0x40. EL transitions, cache
maintenance and MMU register operations are modeled, so these checks do
not establish hardware boot correctness. GPU, Wi-Fi and FIMC-IS2 are unchanged.

Candidate payloads: `/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-samsung-entry/`.
Use its raw Image and matching dtb_dream2lte.img together when repacking.
The original DPU/SysMMU panic remains unresolved pending a bootable port.

## Earlier saved-log diagnostics

The latest hlng@longa normal image still hangs, but the reported "Starting
kernel" is from a saved bootloader/last_kmsg log, not a live UART trace.
The device tree selects console=ram. Therefore the last bootloader line
does not identify a kernel execution checkpoint. Header offset/flags match
the working vendor Image (0x80000, 0xa). Modeled startup also passes at the
historical Samsung load 0x80148000; EL/MMU/cache operations remain modeled.
The physical-device hang has not been located or fixed.

Earlier logging-only diagnostic payloads:
`/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-boot-ram/`.
They enable EXYNOS_SNAPSHOT_EARLY_LOG and append
`loop.max_part=7 initcall_debug ignore_loglevel loglevel=8` to the DT command
line. UART tracing and serial earlycon are off. The existing snapshot buffer
selected by ess_setup is used; no fixed physical RAM writes are added.
After the hang, warm-reboot into recovery/a working kernel and capture
/proc/last_kmsg and /sys/fs/pstore before another reboot. A cold power cycle
can erase retained RAM. Look for "ESS early boot:" and initcall_debug output.
A hang before memory/scheduler/RCU initialization may still precede this log.

A separate optional UART build is archived at
`/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-boot-uart/`.
EXYNOS8895_BOOT_UART_TRACE emits 012345 before the MMU switch, using only
the existing UART_DBG at 0x10430000. It preserves boot arguments, uses
bounded polling and does not initialize clocks/pins. Stages: entry, saved
arguments, EL setup, page tables, CPU setup, then MMU enable. Stage 5 can
repeat for secondary CPUs. This option defaults off in all six saved configs.
The earlycon name is `exynos`, and the diagnostic UART command line uses
`earlycon=exynos,0x10430000 console=ttySAC0,115200n8 keep_bootcon`.

The Samsung earlycon writer previously waited forever for the FIFO-enable
configuration bit, including when the bootloader left FIFO mode disabled.
It now polls TX-empty with a bound and emits normal CRLF. Host tests cover
FIFO off/on, delayed ready, permanently busy and empty writes. UART-stage
emulation checks ready/busy behavior, boot arguments, relocation and page
tables at three loads including 0x80148000. UART registers, EL/MMU and cache
operations are simulated; these checks do not establish hardware boot.

Both diagnostic builds use hlng@longa and compile with GCC 6.3.1. All six
normal defconfigs regenerate unchanged. GPU, bcmdhd4361 and FIMC-IS2 are
unchanged. Reports and build logs: `/workspace/kernel-env/early-boot-uart/`.

## Samsung ONESHOT netfilter removal

The Samsung ONESHOT match is removed, including its Kconfig entry, build
registration, implementation, UAPI header and all six board defconfig entries.
The GCC 6.3.1 Image/Image.gz and DTBH build passes. No ONESHOT match/UID
symbols remain in vmlinux; all six configs regenerate without the option.
Modeled bootstrap and raw-payload checks pass. The removal checks are recorded under
`/workspace/kernel-env/remove-netfilter-oneshot/`. New payloads are packaged
in `/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-no-oneshot/`.
Older artifact directories below predate this code removal.

## Rebased sep-10.0 without the backport range

The entire inclusive range from `ad127f3a68d61bc772f421c4253ebd041e56823f`
to `82550284fd80261269ddbd26fa2ddd0a213301d7` was dropped from the
sep-10.0 ancestry. All 50 retained later commits were replayed without patch changes
(verified with git range-diff). The original vendor and pushed port branches
remain available as backups. The new vendor-base branch is
`rebase/sep-10.0-no-vdso`; the new port merges the actual v4.9.191 again.

The 60 baseline paths affected by the dropped range were evaluated against
a fresh merge. Other Exynos8895 port resolutions were reused. Native 4.9.191
kprobes remain supplied by upstream. ARM64 uses the upstream assembly vDSO
and the combined AArch32 vectors/sigreturn/kuser page. The generic C vDSO,
compat vDSO directory, split sigreturn source, extra Kconfig switches and
ARM32 toolchain wiring were removed. Native vDSO data updates use the
retained timekeeper's raw_sec/tkr_raw.xtime_nsec representation. All six
configs are regenerated without COMPAT_VDSO.

Review records: `/workspace/kernel-env/rebase-sep10-no-vdso/`, including
rebase-report.json, range-diff.txt and per-path resolutions.json. The older
retarget/build records below describe the preceding merge unless overridden
by this section. Hardware boot and the DPU/SysMMU panic remain unverified.

## Retarget to 4.9.191

The in-progress merge parent is now the actual v4.9.191 commit, matching
r7's upstream version. This is a source retarget: later upstream stable
changes were removed using a three-way comparison, retaining original
vendor units and r7 Android implementations rather than replacing their
interfaces with upstream-only code. Paired ext4/fscrypt source uses r7.
The Samsung relocation record, EHMP, early-log instrumentation, all six
board configurations and the hardware-removal exclusions remain required.
Existing 4.9.337 artifact directories and older results below are historical.
The pre-retarget source and Git index/merge metadata are backed up under
`/workspace/kernel-env/retarget-4.9.191/`; applied.json records file decisions.

## Current status

The merge has no unresolved index entries. The enabled dream2ltekor hardware
configuration compiles and links Image, Image.gz and dtb_dream2lte.img with
Linaro GCC 6.3.1. All six board defconfigs regenerate together and retain
relocation, thermal, camera, display, audio and haptics requirements.
The resolved port is recorded as a merge on the rebased vendor history.

The user reports a Samsung-logo hang with no new last_kmsg. Hardware boot
has not been demonstrated. The port is experimental and not complete as a
working device kernel. The earlier DPU/SysMMU panic is also unresolved;
a successful cross-build or version upgrade does not establish a fix.

## Boot failure investigation

The original uploaded last_kmsg includes an older successful bootloader
handoff with a kernel payload of 0x1f89000 bytes, a ramdisk at 0x89000000,
and a device tree at 0x8a000000. Its kernel size is consistent with an
uncompressed ARM64 Image. The user confirmed that the failing boot.img used the raw Image. A gzip
compression mismatch is therefore ruled out as the explanation. Use the raw
`arch/arm64/boot/Image` in the kernel slot and the generated Samsung DTBH
`dtb_dream2lte.img` in the existing dt slot. These separate build outputs
are not a complete Android boot image. Preserve the existing boot image's
ramdisk, board metadata and Samsung layout when repacking.

`scripts/check-exynos8895-boot.py boot.img` checks a repacked Samsung legacy
Android boot image for a raw ARM64 kernel and bounded DTBH/DTB entries.
It rejects gzip kernels, missing DTBH and truncated payloads. It can also
check separate files with `Image --dtb dtb_dream2lte.img`. It does not
verify bootloader signatures, device selection or actual runtime loading.

The final ELF contains only R_AARCH64_RELATIVE and NONE dynamic relocations.
An instruction-level startup check reaches start_kernel at physical loads
0x80080000, 0x80280000 and 0x80380000, checking Samsung x1/x2 capture, the
third TEXT_OFFSET word, initial stack/current task, FDT and kimage_voffset.
EL transitions, MMU translation and cache maintenance were modeled in that
check; it does not validate CPU registers, physical caches or peripherals.
The Exynos snapshot log hook is registered at early_initcall, after core
architecture, memory, scheduler and timer initialization. An earlier hang
can therefore leave no new last_kmsg.

## Compatibility retained

- Samsung CONFIG_RELOCATABLE_KERNEL selects the native 4.9 relocation
  engine. Bootloader DRAM base and actual text offset are retained in the
  original three-word record, outside BSS clearing. Initial and secondary
  task setup matches THREAD_INFO_IN_TASK. Reserved TTBR0 and KPTI page
  table layout are paired with entry code.
- Samsung crash-dump metadata retains an absolute kallsyms address table.
  Relative kallsyms is disabled by default for Exynos8895 with SEC_DEBUG;
  all six regenerated configs check that invariant.
- ARM_GIC_V3 is selected only for !ARCH_EXYNOS, matching the 9810
  reference. This also omits the CONFIG_ARM_GIC_V3-guarded system-register
  setup in the early ARM64 EL2 entry path. That hardware path was modeled
  in the instruction check, so its earlier success cannot rule out a
  hardware register-access problem. It is absent from all six Exynos8895
  defconfigs. GICv2 remains
  enabled; the donor PCI ITS selection is retained.
- Scheduler Kconfig entries and HMP defaults match the 9810 reference.
  The r7 EHMP, SchedTune, cgroup SchedTune and Fluid RT selections are
  enabled across all six configs; legacy HMP remains disabled. Fluid RT
  exposes the implemented sched_rt_boost_threshold sysctl from the r7
  reference; the stale, unimplemented cluster-spill entry was replaced.
- EHMP replaces HMP. Legacy Samsung boost callers retain functional
  wrappers. Original Exynos8895 topology and capacity assumptions remain;
  Exynos9810 energy tables were not copied. Energy-aware tuning needs
  hardware measurements.
- Original Exynos8895 clocks/CAL, device trees, thermal control, storage,
  DPU/panels, IOVMM/SysMMU, ION heap IDs, legacy fences and media graph
  interfaces remain, with 4.9 interface adaptations where necessary.
- Linaro GCC 6.3.1 builds ARM64. The native AArch32 combined vectors page
  supplies sigreturn and kuser helpers; compat time calls use syscalls. No
  ARM32 vDSO compiler is required. Outputs remain outside the checkout.

The Lunarixus Exynos9810 tree is a code reference, not the kernel base.
Shared 4.9 APIs now target 4.9.191 and the r7 Android interfaces; its device trees,
frequency/energy tables and hardware drivers were not substituted for 8895.

## Removal scope and exceptions

Exynos8890 platform selection and dedicated platform support were removed.
S2MPS16 regulator, MFD, IRQ and RTC support was removed. GPU code remains
unchanged from the original vendor HEAD, including any inactive S2MPS16
references in that protected tree. bcmdhd4361 also matches the original
vendor HEAD. FIMC-IS2 retains its Exynos8890 support; its remaining changes
are 4.9 API adaptations, not Exynos8890 removal.

Legacy bindings containing exynos8890 in shared ION/JPEG code remain where
the original Exynos8895 device trees actually use those bindings.

## Reproduce

```sh
. /workspace/kernel-env/activate
scripts/regen-exynos8895-defconfigs.sh HOSTCFLAGS=-fcommon
make O=/workspace/kernel-env/out/port-4.9-dream2ltekor \
  HOSTCFLAGS=-fcommon ANDROID_VERSION=100000 ANDROID_MAJOR_VERSION=q \
  exynos8895-dream2ltekor_defconfig
make O=/workspace/kernel-env/out/port-4.9-dream2ltekor \
  HOSTCFLAGS=-fcommon ANDROID_VERSION=100000 ANDROID_MAJOR_VERSION=q \
  -j5 Image.gz dtb_dream2lte.img
python3 scripts/check-exynos8895-boot.py \
  /workspace/kernel-env/out/port-4.9-dream2ltekor/arch/arm64/boot/Image \
  --dtb /workspace/kernel-env/out/port-4.9-dream2ltekor/arch/arm64/boot/dtb_dream2lte.img
```

The build generates both raw Image and Image.gz. Use raw Image for the
Samsung packaging check above. HOSTCFLAGS=-fcommon is required by the
legacy host DTC with this environment's modern host GCC.

Earlier port build log: `/workspace/kernel-env/port-build-round18.log`.
Startup model: `/workspace/kernel-env/head-entry-verification.json`.
Original panic analysis: `/workspace/kernel-env/last-kmsg-analysis.md` and
`/workspace/kernel-env/sysmmu-review.md`.
The prior 4.4 diagnostic patch in kernel-env/diagnostics is unapplied.

A diagnostic build enables CONFIG_EXYNOS_SNAPSHOT_EARLY_LOG and adds
initcall_debug to the existing command line. It initializes the existing
snapshot kernel log after memory, scheduler, workqueue preparation and RCU,
before IRQ and timer initialization, and records progress around init_IRQ
and time_init, including clock-provider and clocksource probing. It reuses that mapping at normal snapshot initialization
and captures the previous last_kmsg only once. Event tracing remains off
until the normal initcall. This option is off in all six normal defconfigs.
It cannot capture hangs before its own initialization or guarantee retained
logs across a cold power cycle. Mapping/allocation failures are checked.

Next required evidence is a retained log from the diagnostic Image, or
a new bootloader/early UART trace. Display scanout, GPU/camera, storage,
suspend/resume and the original SysMMU fault all require on-device validation.

Diagnostic build log: `/workspace/kernel-env/r7-aligned-build-final.log`.
Kconfig comparison: `/workspace/kernel-env/reference-review/r7-kconfig-alignment.json`.
Diagnostic artifacts: `/workspace/kernel-env/artifacts/4.9.337-dream2ltekor-early-log/`.
The diagnostic image is instrumentation, not a confirmed boot fix.

## Additional r7 defconfig alignment

All six defconfigs align 44 more shared r7 settings: RT group scheduling,
schedutil as the default governor and CPU frequency statistics; KSM,
zram, devtmpfs and memory-state accounting; BPF/probes, network diagnostics,
filters, PPP and vsockets; ext2/ext3/NTFS support, RSA/CCM and CRC-CCITT;
and scheduler/timer/list/atomic-sleep diagnostics and DPM watchdog.
The loop-device minimum is 48 and ARM64 VHE is disabled, matching r7.
Interactive remains selectable for existing sep-10.0 governor requests.
Schedutil uses the implemented native CPUFreq fast-switch API; ACME retains
its supported slow transition path when fast switching is unavailable.

The expanded comparison is saved in
`/workspace/kernel-env/reference-review/r7-defconfig-expanded-summary.json`.
The existing 8895 hardware settings, cgroup controllers and namespaces,
legacy ION/sync/verity interfaces, zswap/frontswap, memory tuning, Samsung
relocation and absolute kallsyms are retained. EAS defaults remain off
pending an 8895 energy model. Donor-only drivers, hypervisor/security
firmware dependencies, and unsupported config symbols are not imported.
These configuration changes require on-device validation; a successful
build does not establish that the reported logo hang or SysMMU panic is fixed.

The enabled schedutil path is paired with the donor slack trace events and
adapted to the standard 8895 CPU_PM_ENTER/EXIT/ENTER_FAILED protocol. Its
QoS class mapping comes from existing ACME cpufreq-domain device-tree nodes;
optional slack timing stays disabled without explicit schedutil DT settings.
Slack timing comparisons use milliseconds, and scaling work selects an online
CPU instead of deriving a CPU number from the online CPU count. Android
PPPoLAC/PPPoPNS protocol IDs and their existing sockaddr headers are restored
alongside the enabled transports.

Expanded normal build log: `/workspace/kernel-env/expanded-r7-build-final.log`.
Before retargeting, the enabled vsock implementation matched canonical v4.9.337, removing a
stale duplicated function while retaining upstream stable fixes.
Expanded artifacts: `/workspace/kernel-env/artifacts/4.9.337-dream2ltekor-r7-expanded/`.

Expanded early-log build log: `/workspace/kernel-env/expanded-r7-diagnostic-build.log`.
The early-log variant is stored in the expanded artifact directory under `early-log/`.

## Shared code alignment with the 9810 reference

ACME now selects a referenced policy through an online CPU in its cluster,
matching the reference hotplug behavior. QoS updates, DVFS constraints and
scaling, and initial policy updates use that lookup consistently. All-offline
clusters are handled without dereferencing a missing policy. The reference's
frequency-transition entry/exit snapshot records and unsigned frequency-table
return type are retained. Existing 8895 CAL operations and thermal limits
remain in place.

Scheduler device-tree CPU masks are checked before per-CPU use, and schedutil
releases its initialized kobject if sysfs creation fails. Existing 8895 CPU PM
adaptations, native fast-switch API, and missing-energy-model guards remain.
These changes do not establish a boot or SysMMU fix. The hotplug/governor
behavior still requires on-device testing.

Build log: `/workspace/kernel-env/r7-code-alignment-build.log`.
Review: `/workspace/kernel-env/reference-review/r7-code-alignment.json`.

## 4.9.191 retarget validation

The upstream source delta from 4.9.337 to 4.9.191 was applied to the existing
port, with individual file decisions in
`/workspace/kernel-env/retarget-4.9.191/applied.json`. Original vendor files
and exact r7 implementations are retained. Overlapping Exynos hardware
units keep their existing 4.9 API adaptations. This remains an Android
vendor kernel with downstream backports, not a pristine upstream tree.

The futex declarations and RT mutex debug interfaces now pair with the
4.9.191 core. ARM64 trampoline fixmaps match the older entry implementation,
which retains Samsung's EL1 exception-stack padding. The unused duplicate
Cavium entry and mixed cache-trap callback signature were corrected. The
r7 memcg locking implementation pairs with its headers; r7 overflow helpers
remain for its Android filesystem code. Ext4/fscrypt use the paired r7
source with the existing 8895 diagnostic-read adaptation.

Normal build log: `/workspace/kernel-env/retarget-4.9.191/build-final.log`.
All six defconfigs were regenerated and checked for repeatability. The
optional early snapshot logger remains available and off in normal configs.
Hardware boot and the original DPU/SysMMU panic still require device testing.

The r7 split workqueue initialization is retained as a paired core/header
unit: early queue setup precedes RCU and snapshot logging; workers start
before pre-SMP initcalls. Samsung exception vectors retain their branch
hardening handlers. Snapshot printk hooks now pass the 4.9.191 continuation
state. Wakelock formatting and POSIX-clock registration use the paired
4.9.191 implementations. ION callbacks use the native six-argument genpool
allocator API; the duplicate, identical CEA table was removed.

Current normal-build artifacts are in
`/workspace/kernel-env/artifacts/4.9.191-dream2ltekor-no-vdso/`: raw Image, Samsung
DTBH, matching vmlinux/System.map/config, source-retarget report and checksums.
The Linaro GCC 6.3.1 build passed. Bootstrap emulation and payload validation
are recorded alongside the artifacts; CPU EL/MMU/cache operations are modeled,
so these checks do not establish a successful physical-device boot.

The rebased merge builds Image, Image.gz and DTBH with Linaro GCC 6.3.1 and
an empty CROSS_COMPILE_ARM32 prefix. All six defconfigs regenerate identically.
The native ARM64 vDSO exports all four expected functions; the final kernel
contains combined AArch32 sigreturn/kuser vector symbols and no vdso32 symbols.
Modeled bootstrap checks pass at three physical loads. Build log:
`/workspace/kernel-env/rebase-sep10-no-vdso/build-final.log`.

The subsequent config-only compat-vDSO enablement commit
`409977329217ec01d494727ee47604138c8211ee` was also dropped from the
rebased vendor history. Its six defconfig settings were already absent from
the tested port. The rebuilt merge preserves the exact previously validated
kernel/configuration tree; only this documentation and the merge ancestry
change. All 50 remaining later patches are unchanged (git range-diff).

The inclusive vendor range `a0bd65ab844d7d5a8541a826382080bb2b9b8c69`
through `dce63930d58e327f4a9011fee308931fa331f9f4` (28 commits) was also
dropped before the 4.9.191 merge. All eight subsequent vendor patches replay
unchanged. Upstream 4.9.191 supplies the shared I/O and LZ4 implementations;
the existing Exynos8895 merge resolutions are preserved. The final kernel
and defconfig source remains identical to the validated port; only merge
ancestry and this document change. Review records are under
`/workspace/kernel-env/drop-ion-iov-lz4-range/`.

The vendor proc backports `1991c75491a7beca5aec237347341683e48484e6`
(faster proc status formatting) and `cf28de760488` (seq decimal prefix API)
were both dropped before the upstream merge. All 11 remaining later patches
replay unchanged. The final merge retains upstream 4.9.191 proc code and
existing Samsung adaptations. Kernel/config source remains identical to the
validated port; only merge ancestry and this documentation change. Review
records: `/workspace/kernel-env/drop-proc-status-commit/final-report.json`
and `range-diff-both-dropped.txt` in that directory.

The F2FS backport merge `50fd58ce0b6c` and follow-ups `4f7c6c696413`,
`0cfcf3b643d7` and `3d40faee4fe1` were dropped from the vendor ancestry.
The rebase starts at the F2FS merge's first parent, excluding its imported
side history, and replays all 24 later patches unchanged. The final merge
preserves the existing 4.9/r7 filesystem implementations and Samsung port
resolutions. Kernel/config source remains identical to the validated port;
only merge ancestry and this document change. Review records:
`/workspace/kernel-env/drop-f2fs-merge/`.
