# Prebuilt assessment — not a public release

The recorded system image and boot payload still exist locally. Their freshly computed SHA-256 hashes are:

| Local artifact | SHA-256 |
| --- | --- |
| `work/tissot-camera-fixes-system.img` | `a687863603085bab369b7869b92c610f13cb287e2432be2b1305dcdee087fb45` |
| `work/tissot-aec-final-payload.img` | `1cb21c502d61b06251b5105035520bd731e72397737dcac6e7359d35d6398c69` |

The system image matches the full historical hash in the consolidated status table. The payload matches the abbreviated `1cb21c50` identifier in the reviewed deployment records; no independent historical full-hash comparison was established in this export. Historical camera acceptance also involved a rebuilt APK installed under `/data`, so the image pair alone has not been proven to reproduce that complete device state.

## Why binaries are not bundled

The consolidated project status explicitly records permissive SELinux, `ro.adb.secure=0`, an embedded host authorization key, and diagnostic hooks. These are bench bring-up artifacts, not safe public installation packages. Firmware/vendor redistribution rights and complete corresponding source availability also require review. No images have been modified, repackaged, uploaded or presented as release-ready.

Before a public binary release: remove host-specific authorization material, review debugging/security settings and redistribution licenses, preserve complete corresponding source and build provenance, establish exact kernel/module/system/APK compatibility, and retest a freshly provisioned device. Changing image contents invalidates the historical hashes and requires new acceptance results. This remains outstanding; the completed artifact in this turn is the source overlay.
