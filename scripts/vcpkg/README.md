# vcpkg port for emhash

This directory contains the vcpkg port definition for emhash.
To submit to vcpkg-registry, copy this directory to `ports/emhash/` in the vcpkg repo.

## Usage (local overlay)

```bash
vcpkg install emhash --overlay-ports=/path/to/emhash/scripts/vcpkg
```

## Files

- `vcpkg.json` — Port manifest (name, version, dependencies)
- `portfile.cmake` — Build/install instructions for vcpkg
