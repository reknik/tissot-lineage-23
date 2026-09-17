# Selected historical evidence: camera-final-verification

Source in the original workspace: `tissot-mainline-status.md`. Verbatim selected lines, not new measurements or raw-log verification. Omitted sections are not bundled; line references refer to the original snapshot.

## Source lines 1811–1831

```text
### Round 24 — camera: final photo-path verification; video is a separate gap (2026-09-16)

Full evidence: `work/evidence/camera-awb-refine/` and
`work/evidence/camera-usability/`.

- **Verified on the current known-good build** (`a6878636…` + `1cb21c50…`):
  app opens straight to the preview (no first-run dialog); back camera 0 =
  OV12A10 main streams and captures; the camera toggle reaches **Camera ID 2**
  (front, `camera-sensor@2d`) and captures there too; stills are 1536×2048
  with correct Exif `ImageWidth/Length` and `ExifImageWidth/Height`.
- **Video recording is not working.** The `android.media.action.VIDEO_CAPTURE`
  entry (`com.android.camera.VideoCamera`) runs the photo `CaptureModule` and
  the shutter saves a JPEG, not an MP4; no video file or MediaStore entry is
  produced. Video was never a tracked camera milestone in the plan, so it is
  recorded as a separate follow-up rather than part of this objective.
- **Objective closure:** the reported camera defects are fixed —
  nondeterministic camera IDs (Round 20), dark/green output (Round 21), the
  unreachable front camera (Round 22), the capture JNI crash and the stuck
  first-run dialog (Round 23). Phase 7 is at PASS 64 / FAIL 2 / NOT TESTED 35.
- **Not tested:** physical/subjective image-quality acceptance (the user's
  verdict), and video recording.
```

## Qualification

Historical claims and artifact names are preserved as evidence, not deployment recommendations. Short artifact hashes are not sufficient to authenticate downloads. Raw logs/images and the referenced older build outputs are not included.
