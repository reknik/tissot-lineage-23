# Ready-to-copy modified sources

`source-overlay/` contains 26 complete files in their original paths relative to the Android checkout. `source-overlay/MANIFEST.json` maps every file to its project, exact baseline commit and corresponding patch. These are an alternative representation of the four patches, not additional fixes.

## Use

1. Read PATCHES.md and PORTING.md for baseline commits and missing integration.
2. In a separate Android checkout, use the recorded baseline for each affected project. Back up any local modifications.
3. Review and copy only the source files listed in MANIFEST.json into their matching paths. Do not copy the manifest or this showcase wholesale into Android. Do not apply the patches again after copying the files.
4. Compile the affected components and run device-specific acceptance tests. Copying these files saves implementation work, not compilation. The overlay is not a complete device port.

## Validation

All 26 files were checked byte-for-byte using Git blob hashes against the result of applying the exported patches to the exact recorded baselines in isolated temporary indexes. Exported XML was parsed successfully. No Android or kernel compilation, linking, device installation or runtime test was performed.

The archive `tissot-source-overlay.tar.gz` is prepared separately under the parent workspace’s `showcase-downloads/` directory, alongside a SHA256SUMS file. This keeps generated archives out of the Git repository. The unpacked source remains in this repository for review. Source licenses and upstream attribution continue to apply; inclusion does not relicense the files.
