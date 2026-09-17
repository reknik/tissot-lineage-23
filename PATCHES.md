# Selected implementation patches

These are uncommitted working-tree snapshots relative to each project’s exact HEAD below, not upstream commits or a complete port. They include local diagnostics and workarounds; review before reuse. Existing upstream licenses apply; no blanket relicensing is intended. No proprietary binaries, authorization keys, raw device logs, or ROM images are included.

## libcamera

- Patch: [patches/libcamera.patch](patches/libcamera.patch)
- Android checkout project: `external/libcamera`
- Baseline and upstream remotes:

```text
cd4b2eff206afa6746c4864e2bffcc2e1f0e0b9c
aosp	https://android.googlesource.com/platform/external/libcamera (fetch)
aosp	https://android.googlesource.com/platform/external/libcamera (push)
```

## camera-app

- Patch: [patches/camera-app.patch](patches/camera-app.patch)
- Android checkout project: `packages/apps/Camera2`
- Baseline and upstream remotes:

```text
e0f3c0a88fdb67a41a52d4705b8604c338e57194
aosp	https://android.googlesource.com/platform/packages/apps/Camera2 (fetch)
aosp	https://android.googlesource.com/platform/packages/apps/Camera2 (push)
```

## qseed2

- Patch: [patches/qseed2.patch](patches/qseed2.patch)
- Android checkout project: `kernel/mainline/msm8953-mainline`
- Baseline and upstream remotes:

```text
7213855948c70fd165356526f1de1c3b6bf4d554
github	https://github.com/msm8953-mainline/linux (fetch)
github	https://github.com/msm8953-mainline/linux (push)
```

## thermal-hal

- Patch: [patches/thermal-hal.patch](patches/thermal-hal.patch)
- Android checkout project: `hardware/interfaces`
- Baseline and upstream remotes:

```text
13d687a84c834af855bfb314b853b8372a7aa6d9
github	https://github.com/LineageOS/android_hardware_interfaces (fetch)
github	https://github.com/LineageOS/android_hardware_interfaces (push)
```

## Scope and dependencies

- **libcamera:** tracked local changes plus the new `converter_softisp.cpp`; includes RAW10 conversion, buffer handling, metadata, exposure/pipeline work and diagnostics. This is a combined snapshot, not a minimal single-fix patch.
- **camera-app:** JNI packaging, first-run settings and front camera selection. The hardcoded front ID 2 is tissot-specific and depends on deterministic sensor ordering in the libcamera patch; do not generalize it to other devices.
- **qseed2:** only the tracked `drivers/gpu/drm/msm/disp/dpu1/` changes. Kernel tests, broader GPU diagnostics, device configuration and other kernel changes are intentionally not included. This directory snapshot may contain related display changes beyond the scaler.
- **thermal-hal:** the tracked sysfs-backed default AIDL thermal implementation. Device packaging of the thermal APEX is not included.

## Safe reuse

Use a separate checkout of the listed upstream project at the exact baseline SHA. First inspect the patch, then run `git apply --check /absolute/path/to/the.patch` from that project root. Only apply it after reviewing the changes and missing integration requirements. Do not apply these blindly to an active, dirty bring-up tree.

A successful patch check is not a build or device test. This export omits device-tree integration, sensor drivers, firmware provisioning, provider packaging, bootloader changes, and other platform work. It cannot reproduce the full port on its own. See [PORTING.md](PORTING.md) and [ISSUES.md](ISSUES.md).
