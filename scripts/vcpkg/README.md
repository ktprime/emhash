# vcpkg port for emhash

This directory contains the vcpkg port definition for emhash.
To submit to vcpkg-registry, copy this directory to `ports/emhash/` in the vcpkg repo.

## Usage (local overlay)

```bash
vcpkg install emhash --overlay-ports=/path/to/emhash/scripts/vcpkg
```

## Files

| File | Purpose |
|------|---------|
| `vcpkg.json` | Port manifest (name, description, dependencies); version is filled from the source [VERSION](../../VERSION) file by `portfile.cmake` |
| `vcpkg.json.in` | Manifest template with the `${EMHASH_VERSION}` placeholder (reference for registry submission) |
| `portfile.cmake` | Build/install instructions for vcpkg |

## Release checklist

Before submitting upstream:

1. Create a release tag `vX.Y.Z` matching the [VERSION](../../VERSION) file (currently `1.1.0`).
2. Download the release tarball and compute its SHA512:

   ```bash
   curl -sL https://github.com/ktprime/emhash/archive/refs/tags/vX.Y.Z.tar.gz | sha512sum
   ```

3. Replace the placeholder SHA512 in `portfile.cmake`.
4. Test with the overlay install command above.
