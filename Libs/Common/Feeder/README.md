<!-- README.md, Version: 1.06 -->
# Feeder Common

[Deutsch](README.DE.md)

This directory is not an Arduino library. Project files in `Feeder` and `FeederSimple` include the shared headers and C++ sources through thin wrappers.

Schedules, configuration, status API, temperature sensing, time synchronization, WebSocket support and the drive interface are shared here. Feeder's WLAN/web-server implementation includes the common source; FeederSimple provides a project-specific extension for its diagnostic page.

Readable web sources for both devices are in `WebSrc/`. Both PreBuild scripts generate minified web files from them; FeederSimple omits the servo-specific `ServoSteps.json` file.

Hardware access uses the project-specific `FeederDrive.h` and the shared `TFeederDrive` interface.
