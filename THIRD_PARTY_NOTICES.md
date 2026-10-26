# Third-party material

This file identifies vendored source and keeps original attribution visible.
Renaming a wrapper or directory does not change upstream copyright or terms.

| Local path | Origin | Terms recorded in this repository | Review status |
|---|---|---|---|
| `lib/BrewProfileRuntime/upstream/` | [jniebuhr/gaggimate](https://github.com/jniebuhr/gaggimate), pinned revision in `lib/BrewProfileRuntime/UPSTREAM.json` | The README at the pinned revision declares CC BY-NC-SA 4.0 for the upstream work. No separate LICENSE-named file exists at that revision. | Preserve attribution, pinned revision and local modification notes. |
| `lib/PressureControlVendor/` | [jniebuhr/gaggimate](https://github.com/jniebuhr/gaggimate), details in `lib/PressureControlVendor/UPSTREAM.md` | CC BY-NC-SA 4.0 as recorded by upstream. | Preserve attribution and modification record. |
| `lib/PSM/` | Vendored PSM module | Unlicense; full text in `lib/PSM/LICENSE`. | Keep license text with distributions. |

The pinned upstream README license statement was checked against commit
`61cacea506cdc9f8cf5032089a92ba4dd5d4a24a` in the local Git reference;
the five copied upstream profile/process files match the SHA-256 values in
`lib/BrewProfileRuntime/UPSTREAM.json`.

Libraries installed by PlatformIO are declared in `platformio.ini` and
`hmi/platformio.ini`; they retain their own licenses. This document does not
grant rights in third-party material. Firmware and source distributions need
the relevant notices and a checked list of third-party dependencies.
