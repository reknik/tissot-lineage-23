# Picking up the port

## Start here

1. Read [STATUS.md](STATUS.md) and [ISSUES.md](ISSUES.md); distinguish historical observations from outstanding acceptance.
2. Use [PATCHES.md](PATCHES.md) for exact upstream SHAs and project paths. Inspect each patch in a disposable checkout before integrating it. These are selected working-tree diffs, not a complete release series.
3. Obtain the missing device and platform integration before expecting a build. No known-good complete port can be reconstructed from these four patches alone.

## Recorded build context

- Platform: LineageOS 23.2 / Android 16, arm64 Cortex-A53.
- Product: `lineage_tissot_mainline`.
- Device target: `device/xiaomi/mi89xx-mainline/tissot_mainline`.
- Recorded lunch target: `lineage_tissot_mainline-bp4a-userdebug`. The release component `bp4a` is required in the recorded tree.
- Kernel project: `kernel/mainline/msm8953-mainline`; source remote and exact baseline are in PATCHES.md.
- Boot design: mainline-style target using lk2nd and modules under `/system/lib/modules`, rather than a conventional downstream vendor-partition layout.

These are historical build coordinates, not a tested sync/build recipe. The live workspace local manifest names additional LineageOS mainline projects and proprietary vendor dependencies; no pinned full multi-project manifest is included.

## Integration that is NOT exported

- Device makefiles, overlays, module lists, init/fstab changes, audio routing and firmware provisioning. Some local device files contain host authorization material; do not copy the directory wholesale.
- Dedicated sensor drivers, DTS camera routing and CAMSS changes required for the three tissot sensors.
- The common-device camera-provider Android.bp, VINTF fragment and product packaging that expose `camera.libcamera.so` through the HIDL provider wrapper.
- Thermal APEX packaging, bootloader/payload construction, USB/ADB recovery work, audio/platform compatibility changes and proprietary GNSS/RIL dependencies.
- QSEED2 KUnit test source and kernel Makefile wiring; the patch includes only the DPU implementation directory.

Request or reconstruct those pieces with provenance and redistribution permission before trying to reproduce the full port. Do not distribute extracted firmware or vendor libraries solely because they were present in the workspace.

## Build and debug lessons

- The recorded Camera2 app resource changes required `m Camera2 systemimage`; `m systemimage` alone did not rebuild the app. A `/data` APK can shadow the system APK, so verify which build is active.
- App-installed JNI dependencies must be accessible from the APK namespace; the Camera2 patch embeds its native libraries.
- A camera service being registered is not enough: inspect static metadata, allocator plane offsets/strides, actual active sensor and still output. Test API1 and API2 independently.
- QSEED2 must not use QSEED3 register programming. The old shortcut produced CTL flush/vblank timeouts and a blue-line artifact in the project history.
- Keep system image, kernel/modules and boot payload identity together. Truncated hashes in old prose are references, not integrity verification.

## Safety and acceptance

No generic flash commands are provided. Establish the device-specific boot chain, partition/slot mapping, full artifact checksums, recovery path and known-good image pair first. Never assume a composite boot image is interchangeable with an lk2nd payload.

Work one intervention at a time: record a baseline, hypothesis, planned change, acceptance gates and rollback; preserve evidence before summarizing results. Recheck boot stability, non-root ADB, storage, audio, camera enumeration and capture after integration. Passing `git apply --check` is not compilation or device acceptance.
