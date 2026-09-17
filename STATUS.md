# Bring-up status

**Record date: 2026-09-16.** This is a curated summary of the local consolidated project status, not a fresh device test. The complete original status document and raw experiment evidence remain outside this repository. Selected original report excerpts are bundled under `evidence/`; see [ISSUES.md](ISSUES.md). Implementation snapshots and exact baselines are indexed in [PATCHES.md](PATCHES.md).

## Recorded results

| Area | Result and qualification |
| --- | --- |
| Android platform | LineageOS 23.2 / Android 16 bring-up targeting `lineage_tissot_mainline` on Xiaomi Mi A1 with the community mainline kernel and lk2nd boot chain. Not a production-ready port. |
| Camera enumeration | Sensor-name ordering replaced pointer-based ordering; rear-main OV12A10 became camera ID 0 on three consecutive recorded boots. |
| Camera compatibility | Focal-length metadata and sensor physical-size information repaired the legacy Camera API1 initialization failure. |
| Image processing | Software ISP white balance and gamma improved previously dark, green output. Physical/subjective image-quality acceptance remains outstanding. |
| Still capture | Rear-main and front-camera capture verified at 1536×2048 with matching Exif dimensions; front-camera selection, JNI packaging, and first-run dialog faults fixed. |
| Audio | Browser playback wedge and microphone routing fixes recorded. This does not establish complete telephony/audio acceptance. |
| Memory / thermal | Zram enabled; sysfs-backed thermal HAL implemented. Full power and thermal acceptance is not implied. |
| Regression snapshot | Latest recorded Phase 7 result: **64 passed, 2 failed, 35 not tested**. The full test definitions and raw results are not bundled here. |

## Remaining limitations

- **Video recording is not working:** the tested video entry opened the photo path and saved a JPEG, not a video. No successful recording acceptance is claimed.
- An intermittent boot-time kernel panic in the DRM/sync-file fence path remains open in the project records.
- GPU memory and UI performance investigation remains unfinished.
- Cellular, outdoor GNSS, battery drawdown, and other hardware acceptance work remain incomplete; running services are not proof of end-to-end functionality.
- These documents do not provide installation instructions, complete source changes, artifact downloads, or reproducible build validation.

## Interpretation

The showcase emphasizes concrete engineering progress without claiming daily-driver readiness. Earlier intermediate fixes were sometimes disproven by later tests; this summary favors the latest reviewed entries. No builds, flashing, reboots, or new device tests were performed while preparing this repository.
