# Issues and evidence for the next porter

Snapshot of records dated 2026-09-15–16; no new device experiments. The bundled evidence consists of selected original report excerpts, not independently verified raw logs.

## Open: intermittent DRM/sync-file boot panic

**Observation:** two boot attempts hung; one retained panic named `drm_crtc.c:161`, `drm_crtc_fence_get_driver_name`, `sync_file_get_name`, and `sync_file_ioctl`, around 28 seconds into boot. Other boots of the same kernel succeeded.

[Original report excerpt](evidence/camera-boot-panic.md). A fence lifetime problem was the report’s hypothesis, not a demonstrated root cause. Identical kernels and intermittent success do not establish that camera module ordering caused it. None of the exported patches is claimed to fix this panic.

**Next useful work:** preserve complete pstore and exact kernel/module/image hashes on failure and compare against successful boots; investigate fence/CRTC lifetime with a bounded diagnostic change. Require repeated boot acceptance before calling it fixed.

## Open: video capture

**Observation:** the video entry launched the photo CaptureModule; the shutter saved a JPEG, with no MP4 or video MediaStore entry. [Final photo-path report](evidence/camera-final-verification.md). Successful still capture is not evidence of video support.

**Next useful work:** separate app intent/module routing from encoder and HAL stream-combination support. Define acceptance as a playable video with actual recorded frames and audio, appropriate timestamps, and no provider crashes. No working recording path is bundled.

## Resolved, with calibration debt: Camera API1 returns -19

Focal-length metadata alone was insufficient. The follow-up audit identified missing `ANDROID_SENSOR_INFO_PHYSICAL_SIZE`; adding sensor unit-cell metadata alongside the focal-length fallback produced six successful API1 connections and zero `-19` errors in the recorded acceptance run. [Audit and result excerpts](evidence/camera-sensor-metadata-db.md). The relevant changes are in [libcamera.patch](patches/libcamera.patch).

**Caveat:** the historical focal-length fallback is a placeholder, not module calibration. Cross-check sensor pitch, physical dimensions, FOV and focal lengths against trustworthy module data before treating Exif or geometric metadata as calibrated. The full original report contains a malformed still-capture sentence and inconsistent physical-size arithmetic; those passages are not copied as authoritative values here. The exported source still needs a calibration review.

## Resolved: unstable camera IDs and unreachable front camera

Earlier module-order experiments gave misleading explanations; later consolidated records attribute nondeterminism to sorting raw entity pointers. Sensor-name ordering gave rear-main ID 0 across three recorded boots. The front app selector then needed ID 2 rather than ID 1. The exported libcamera and Camera2 patches contain the changes. [Latest acceptance excerpt](evidence/camera-final-verification.md) records rear and front still capture. Validate the mapping across boots and query facing characteristics rather than assuming the tissot-specific IDs transfer to another phone.

## Still incomplete

GPU memory/UI performance, end-to-end cellular, outdoor GNSS, battery drawdown and subjective image-quality acceptance remain incomplete in the reviewed records. No detailed evidence export or fix claim is made here for these areas. The recorded regression tally is 64 pass / 2 fail / 35 not tested; the complete test inventory is not bundled.
