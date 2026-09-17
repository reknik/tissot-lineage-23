# Xiaomi Mi A1 — Mainline LineageOS Bring-up

A compact engineering showcase of the work to bring **LineageOS 23.2 (Android 16)** to the **Xiaomi Mi A1 (`tissot`)**, using the community **msm8953-mainline Linux kernel** rather than Xiaomi’s downstream kernel.

> Experimental bring-up, not a production ROM release. This repository contains selected source patches and curated evidence—not a complete source tree, reproducible build kit, or installable image.

## Engineering highlights

- **Camera pipeline:** integrated a libcamera Android provider and a CPU software ISP for RAW10 Bayer-to-NV12/NV21 conversion, with demosaicing, scaling, white balance, and gamma correction.
- **Camera compatibility:** repaired framework-facing sensor metadata and buffer handling, made camera IDs deterministic, and restored front-camera access and still capture in the camera app.
- **Display:** implemented QSEED2 scaler support in the mainline Qualcomm DPU path instead of incorrectly reusing QSEED3 programming.
- **Audio and platform integration:** worked through audio routing and browser playback faults, enabled zram, and implemented a sysfs-backed thermal HAL.
- **Evidence-driven debugging:** used bounded experiments, measured outcomes, regression checks, and explicit rollback criteria to distinguish confirmed fixes from rejected hypotheses.

Four real implementation patches are included: libcamera (including the new software ISP), Camera2, the QSEED2 display path, and the thermal HAL. Start with [PATCHES.md](PATCHES.md) for provenance, [ISSUES.md](ISSUES.md) for evidence-backed findings, and [PORTING.md](PORTING.md) for integration gaps and build lessons. Raw logs and images remain excluded.

## Ready-to-copy files

The [source overlay](SOURCE-OVERLAY.md) contains the 26 complete modified files behind the patches, arranged in Android checkout paths. Use the overlay **or** the patches—not both. A checksummed source archive is prepared separately for distribution.

Prebuilt images were located and hashed, but are **not bundled** because the recorded builds contain insecure bring-up settings and host authorization material. See [PREBUILT-ASSESSMENT.md](PREBUILT-ASSESSMENT.md) for hashes and release prerequisites.

## Current status

The latest reviewed records are dated **2026-09-16**. Rear-main and front-camera still capture were verified, but video recording is not working. Boot/display stability and other device acceptance work remain unfinished. See [STATUS.md](STATUS.md) for the scope and limitations of these results.

## Repository contents

| File | Purpose |
| --- | --- |
| [README.md](README.md) | Project overview and engineering highlights |
| [STATUS.md](STATUS.md) | Short, dated summary of recorded results and remaining work |
| [PATCHES.md](PATCHES.md) | Patch index, exact baselines, dependencies and reuse instructions |
| [PORTING.md](PORTING.md) | Build context, missing integration and safety notes |
| [ISSUES.md](ISSUES.md) | Open and resolved issues with links to selected evidence |
| `patches/` | Four implementation diffs; no binaries |
| `evidence/` | Selected original report excerpts with provenance |

## What stays outside this repository

The full Android checkout, build outputs, ROM archives, extracted firmware, device logs, photos, identifiers, host authorization keys, and experiment artifacts remain in the original local workspace. Keeping this repository separate avoids uploading hundreds of gigabytes or publishing machine- and device-specific material.

No existing files were moved or deleted to create this showcase. It is a curated source-and-evidence snapshot, not a backup of the full development work.

## Project context and credit

This work builds on [LineageOS](https://github.com/LineageOS), [AOSP](https://source.android.com/), the [msm8953-mainline community kernel](https://github.com/msm8953-mainline/linux), and [libcamera](https://libcamera.org/). Upstream projects and their contributors retain credit for their work. This is an unofficial project, not an official Xiaomi or LineageOS release.
