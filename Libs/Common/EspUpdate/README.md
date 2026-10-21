<!-- README.md, Version: 1.14 -->
# EspUpdate

[Deutsch](README.DE.md)

Shared web update support for Feeder, FeederSimple, TmrSw and ESP32 projects such as TmrSwShelly.

`WebSrc` contains shared HTML, CSS and JavaScript. Readable project pages contain the `EspUpdateCommonHead` and `EspUpdateCommonBody` markers. `PrepareWebSrc.py` generates complete project pages during PreBuild.

The update page has two sections:

- **Update package** for a single `*.update.signed` file.
- **Update Firmware and/or Filesystem**, with optional fields for `*.bin.signed` or `*.bin.gz.signed`, and `*.littlefs.signed`. Select firmware only, LittleFS only, or both.

Combined upload supports two alternatives:

- `*.update.signed` is the compact single-file package. The signed package contains firmware and only files actually present in `data/`. Firmware is first written into the OTA area, but selected for boot only after the entire package signature is verified. One restart follows.
- Alternatively, upload `*.bin.signed` or `*.bin.gz.signed` together with `*.littlefs.signed` in the second section. A gzip firmware image uses less OTA space. `*.littlefs.signed` is a full partition-sized LittleFS image. The same section supports firmware-only or filesystem-only updates.

ESP8266 builds normally generate both `*.bin.signed` and `*.bin.gz.signed`. The web updater verifies firmware signatures as a SHA-256/RSA stream. Signature and length field are not written into the OTA area. `Update.begin()` receives payload size + 1; `Update.end(true)` is called only after successful explicit verification. The ESP8266 core's automatic RSA/ECC signature verifier is not used. The project RSA public key is generated as `EspUpdateSigning.h` during build.

## Compact filesystem package

Building `*.update.signed` internally creates a compact LittleFS file package in LFSUPD3 format. This internal package is not a separate web-upload artifact. It contains:

- Magic `LFSUPD3\0`.
- Configured LittleFS partition size.
- File count.
- Sum of file content sizes.
- Complete file table with path length, content length and path for each file.
- Every file's contents twice, in the same order.

The full table is known before the first content byte is received. In pass 1, the ESP compares every file byte-for-byte with installed LittleFS. Missing files, different sizes or different contents are marked as changed. It then plans staging only for new/changed files:

- In pass 2, identical files are consumed without staging.
- As many changed files as possible are written as `<Name>.espupd` into free LittleFS space.
- Only changed content that does not fit as temporary flash copies is kept in RAM.
- The build orders larger files first; firmware also plans by size using the full table.
- Temporary flash requirements are calculated by blocks, retaining at least one LittleFS block as reserve.
- RAM staging also retains a heap reserve.

Existing user files are not changed during reception. The signature covers firmware, table and all file contents together. An aborted upload or invalid signature discards unactivated firmware and all `*.espupd` files.

Commit occurs only after successful signature verification:

1. Temporary `<Name>.espupd` files replace their targets through remove + rename. Current `*.espupd` files are then gone.
2. Old files absent from the new table are removed to free space for RAM-buffered files.
3. RAM-buffered files are written.
4. Only after successful filesystem commit is firmware selected for the next boot through `Update.end(true)`.

Orphaned `*.espupd` files from interrupted uploads are removed during setup and immediately before each new upload.

The compact file package is not a gzip-compressed LittleFS image. The ESP8266 core can unpack gzip filesystem images through eboot, but requires `ATOMIC_FS_UPDATE` and a bootloader update command. Combined upload already needs the single eboot command for firmware. The file-based package avoids that conflict while transferring only firmware and required LittleFS files.

Power loss during staging leaves previous user files intact. The actual commit after signature verification is not fully transactional; new firmware is selected for boot only at the very end.

## Progress display

Web forms upload through XMLHttpRequest. The page shows a percentage and transferred/total bytes.

JavaScript adds multipart field `_EspUpdateProgress=1`. The server then streams a text response. With compact `*.update.signed`, LittleFS commit runs in the normal POST handler after reading the full multipart request. The same HTTP request can return commit status lines:

- `PROCESS`
- `Files changed: <changed>, deleted: <deleted>, total: <total>`
- `FILES <done> <total>`
- `FIRMWARE`
- `RESTART`
- `ERROR <text>`

After pass 1, comparison counts are reported as a line during subsequent processing. `FILES` updates after every changed LittleFS file is activated or written from RAM; identical files already count as done. Obsolete files are not counted individually in `FILES`. After `RESTART`, the browser waits for `/Index.html` and reloads automatically when reachable.

Without `_EspUpdateProgress`, the server sends an HTML result response.

## Recovery

`SetupRecovery()` accepts firmware and full LittleFS together in one multipart request. Select firmware, LittleFS or both; uploading both results in one restart after successful uploads. `Config.json` is a separate recovery upload. Compact `*.update.signed`/LFSUPD3 is not accepted in recovery, because comparison and staging require readable existing LittleFS.

## ESP32

The ESP32 recovery uploader accepts DeviceType-typed firmware, LittleFS and configuration artifacts. It checks the 64-byte DeviceType header before flashing and does not write it into the target partition. The ESP32 web updater does not change bootloader or partition table. `SetupRecovery()` and `SetupStartupError()` are also available on ESP32. Obvious full-flash artifacts (`*.bootloader.bin`, `*.partitions.bin`, `*.boot_app0.bin`, `*.factory.bin`, `*.merged.bin`) are rejected by firmware upload. Signed LFSUPD3/combined-update paths are ESP8266-specific.
