# Export validation

- libcamera: PASS — git apply --cached --check against HEAD in isolated temporary index
- camera-app: PASS — git apply --cached --check against HEAD in isolated temporary index
- qseed2: PASS — git apply --cached --check against HEAD in isolated temporary index
- thermal-hal: PASS — git apply --cached --check against HEAD in isolated temporary index

Checks used a temporary index populated from the exact source HEAD, without changing any source worktree or its real index. New software ISP source is included in the libcamera patch. No build or device test was run.

A bounded text scan found no known workspace device serial, local host home path, private-key header, or binary-patch marker in the curated export. This is not a comprehensive secret or licensing audit; review before publishing. Source snapshots retain local diagnostics and workarounds.
