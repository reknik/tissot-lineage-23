# Selected historical evidence: camera-sensor-metadata-db

Source in the original workspace: `work/evidence/camera-sensor-metadata-db/README.md`. Verbatim selected lines, not new measurements or raw-log verification. Omitted sections are not bundled; line references refer to the original snapshot.

## Source lines 8–20

```text
## Bounded offline audit (2026-09-15, complete)

All 134 `ANDROID_*` tags read anywhere in libcameraservice were enumerated
and reduced to the static-characteristics subset, then checked against the
fix-build characteristics dump (`after-dumpsys-media-camera.txt`). Result:
**`ANDROID_SENSOR_INFO_PHYSICAL_SIZE` is the only missing required key on the
API1 open path.** Every other static INFO key the path touches is present on
all three cameras (timestampSource, minFrameDurations, maxFrameDuration,
sensitivityRange, colorFilterArrangement, orientation, whiteLevel,
activeArraySize, pixelArraySize, stream configurations/durations). The
remaining unmatched tags are request/result keys or optional feature keys
(flash, HEIC, depth, distortion, LED, logical-multi-camera) whose absence is
correct for this hardware.
```

## Source lines 28–40

```text
## One intended change

Two source edits, one logical change (complete the sensor metadata database
for the tissot sensors):

1. `external/libcamera/src/libcamera/camera_sensor_properties.cpp`: add
   `ov13880` (1200 nm), `ov12a10` (1250 nm), `s5k5e8` (1220 nm) unit-cell
   sizes with empty test-pattern maps (the kernel drivers expose no
   V4L2_CID_TEST_PATTERN). Values are vendor-documented datasheet constants —
   same epistemic class as the ROM-contract exposure data; cross-check
   against stock EEPROM data when available.
2. Re-apply round 4's `camera_capabilities.cpp` focal-lengths fallback
   (identical diff, `camera-capabilities-focal-lengths.diff`).
```

## Source lines 66–72

```text
## Result

**Confirmed — the hypothesis holds. API1 opens succeed with zero `-19`.**
The audit correctly predicted `ANDROID_SENSOR_INFO_PHYSICAL_SIZE` was the
last missing required key; with the sensor database entries plus the
focal-lengths fallback, `Parameters::initialize()` completes end-to-end and
the Camera app's API1 path works for the first time in the project.
```

## Source lines 85–89

```text
- API1 acceptance: 3 clean open/close cycles on camera 0 AND 3 on camera 2
  (6/6 API1 connects, **zero `-19`**, zero metadata errors, clean
  disconnects), driven through the Camera app's own
  `CameraPictureSizesCacher` (`pm clear` + permission grants per cycle to
  invalidate its cache; `measurement-logcat.txt`). Two further successful
```

## Source lines 121–127

```text
Measurement: audit + metadata dump + 6 API1 open/close cycles + API2 control
+ still + phase 7. Single change: sensor DB entries (3 sensors) + focal-lengths
fallback (one logical metadata-completeness change). Measured result:
physicalSize and focalLengths present on all cameras; 6/6 API1 opens succeed
with zero `-19`; still negotiates 1536×2048; no regressions. Conclusion:
hypothesis confirmed — API1 `-19` fixed. Rollback state: revert the two diffs
and reflash `f278e0f6…` (not exercised — no failure occurred).
```

## Qualification

Historical claims and artifact names are preserved as evidence, not deployment recommendations. Short artifact hashes are not sufficient to authenticate downloads. Raw logs/images and the referenced older build outputs are not included.
