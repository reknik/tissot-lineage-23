# Selected historical evidence: camera-boot-panic

Source in the original workspace: `work/evidence/camera-id-order/README.md`. Verbatim selected lines, not new measurements or raw-log verification. Omitted sections are not bundled; line references refer to the original snapshot.

## Source lines 123–134

```text
### New defect observed during this round (not caused by it, recorded)

Two boot attempts of the unchanged kernel hung during boot; ramoops from the
first (`stuck-boot-console-ramoops.txt`, `stuck-boot-dmesg-ramoops-0.txt`)
shows `kernel BUG at drivers/gpu/drm/drm_crtc.c:161!` →
`drm_crtc_fence_get_driver_name` ← `sync_file_get_name` ← `sync_file_ioctl`
at t≈28 s with bootanim running, then `Kernel panic - not syncing`. The boot
reason was recorded as `kernel_panic,bug`. The same kernel booted fine twice
(other boots of this session), so this is an **intermittent DRM/sync-file
fence lifetime bug** — it needs its own causal round; no attribution to the
module order is possible (the kernel binary is identical in all payloads of
this session).
```

## Qualification

Historical claims and artifact names are preserved as evidence, not deployment recommendations. Short artifact hashes are not sufficient to authenticate downloads. Raw logs/images and the referenced older build outputs are not included.
