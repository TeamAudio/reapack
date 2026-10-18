# Set up reapack-archive

Download a checksum-verified `reapack-archive` release binary from
[TeamAudio/reapack releases](https://github.com/TeamAudio/reapack/releases)
and add it to `PATH`.

```yaml
- uses: TeamAudio/reapack/.github/actions/setup-reapack-archive@reapack-archive-v0.2.0
```

The action ref selects the default binary version, so action code and
binary move together. Override only to install a different release:

```yaml
- uses: TeamAudio/reapack/.github/actions/setup-reapack-archive@reapack-archive-v0.2.0
  id: reapack-archive
  with:
    version: 0.1.3
- run: '"${{ steps.reapack-archive.outputs.executable-path }}" --help'
  shell: bash
```

## Inputs

- `version` — release version without a leading `v` (default: the
  version matching the action ref).
- `install-dependencies` — on Linux, install runtime libraries
  (`libcurl4`, `libssl3`, `libxml2`, `zlib1g`) via apt. Default `true`.

## Outputs

- `executable-path` — absolute path to the `reapack-archive` executable.
- `version` — installed version.

## Supported runners

Linux x86-64, macOS arm64, and Windows x64 — the platforms with
published release assets. Other runners fail with an explicit error.
Linux and macOS binaries dynamically link libcurl, libxml2, and
OpenSSL; the Windows build is statically linked.
