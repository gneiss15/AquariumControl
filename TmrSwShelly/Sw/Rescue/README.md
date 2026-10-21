<!-- README.md, Version: 1.00 -->
# Shelly production Rescue

[Deutsch](README.DE.md)

Standalone production sources for Rescue. Run the build from the TmrSwShelly project root:

```bash
./EspRescueCompile.sh
```

`main/` contains hardware/SDK geometry checks, Main activation checks and the diagnostic service. `Runtime/` provides geometry/uncertainty checks and the Arduino compatibility layer. Stock rollback is blocked. Shared update/signature sources are in `../../../Libs/Common/`, relative to this directory.

The target partition table is generated from `../../partitions.csv`. Generated private WLAN headers and key metadata are under `../../generated/build/rescue/`; private originals are outside the repository. The build does not upload or write to a device.
