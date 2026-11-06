# GnAqPrjsLib

[Deutsch](README.DE.md)

Shared Aquarium project library: heating/charging profiles, PI control,
device/source assignments, Dashboard synchronization and the UDP protocol.
Universal utilities remain in `../GnBasicLib`; dependencies go only from
GnAqPrjsLib to GnBasicLib.

- [UDP protocol](UdpStatus.md)
- [Device identity](DeviceIdentity.md)
- [Library boundary review (German)](Abgrenzung.DE.md)
- [UDP macro trial](tests/UdpMacroTrial/README.md)

Main builds for Feeder, FeederSimple, TmrSwA8T and TmrSwShelly and native builds
for MultiTimer and Dashboard include both libraries. Header names and behavioral
interfaces are preserved. Transfer helpers copy both libraries without changing
device runtime data. Any old Aquarium files left in the Pi's GnBasicLib directory
must be moved aside recoverably during the actual rollout to prevent stale header
shadowing. Nothing has been transferred yet.

`MainSw` is the general main-switch role for measurement-dependent profiles;
`HeaterMain` remains an equivalent compatible input name. `HeaterFloor` and
`Timer` retain their meanings.
