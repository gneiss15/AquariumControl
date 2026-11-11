<!-- UdpStatus.md, Version: 1.02 -->
# UDP status and descriptions

[Deutsch](UdpStatus.DE.md)

Devices publish status on UDP port **44444**, normally every **10 seconds**. State changes, overrides, errors, validity changes, startup and reconnection also trigger a status. Completed measurement cycles supply the published values. Numeric changes alone do not trigger extra broadcasts.

UDP messages use protocol version **3**. `Type` is a value from `TMsgType`. Message fields and compact-status handling are defined by `Definitions/Contract/TUdp*.inc.h`.

```json
{"Ver":3,"Type":2,"ID":"02:11:22:33:44:55@4","Compact":true,"InfoRev":"0123456789abcdef","Status":{"BootCnt":7,"Chs":[{"Id":"02:11:22:33:44:55@4/0","State":true}]}}
```

`ID` identifies a service instance. `InfoRev` fingerprints the descriptive fields associated with this compact status. It is not an expiry time: unchanged descriptions remain valid until a different revision arrives.

A receiver asks the sending IP on UDP port **44500 + ServiceId** for the specific missing fields (for example, MultiTimer: 44501):

```json
{"Ver":3,"Type":3,"ID":"02:11:22:33:44:55@4","InfoRev":"0123456789abcdef","Fields":["Root","Channel:02:11:22:33:44:55@4/0"]}
```

Fields marked `Live` are written in the status datagram. `Info` fields are returned as requested parts: `Root`, `Identity`, `Channel:<channel address>` and `Measurement:<kind>`. `Omit` fields are not sent in compact status; some are available through HTTP, while channel state flags marked `Omit` are represented in `Bits`.

An `Info` reply contains one complete descriptive JSON object in `Data`, associated with the same `ID`, `InfoRev` and requested `Field`. One datagram carries one part. Each service has its own control socket (44501–44504); independent observers such as HeatHistory use a temporary reply port. Missing replies are retried. Info traffic does not refresh online age or create measurement samples.

The five `JD_BIT` entries in `SharedTypes.inc.h` define channel bits in order: `InState`, `AutomaticState`, `OvrActive`, `OvrState` and `CalOvrActive`. Each value bit is immediately followed by its `<Field>Msk` validity bit. A clear mask preserves the receiver's prior value; a set mask applies the value bit, including `false`. The five pairs use 10 of the 16 bits in `Bits`. The merge rule applies uniformly; no field-specific post-merge override is applied.

`State` and `OvrUntil` are ordinary `Live` fields. Channel names, hardware properties, next-change details and state labels are descriptive `Info` fields. `PowerMeter.Diag` and `HeatDiag.ActualMainOn` are `Omit`; HTTP status remains the source for HTTP-only power diagnostics. `HeatDiag.Cfg` is nested within the PI diagnostic object, not a separate datagram. The compact writer applies nested field parts too, so only `TJsonHeatConfig` members marked `Live` are included there.

## Source lookup without a running Dashboard

Confirmed source assignments remain stored on consumers. They do not expire with age. Only Dashboard changes the authoritative assignment; peers can answer from their last centrally confirmed mapping.

```json
{"Ver":3,"Type":5,"Func":"FeederAq1"}
```

The request may be broadcast when the source is unknown. A reply is sent to the requester's IP and reply port:

```json
{"Ver":3,"Type":6,"ID":"02:11:22:33:44:55@4","Func":"FeederAq1","Src":"02:11:22:33:44:66@3","Rev":"2026-11-01T12:00:00.000Z"}
```

Newer confirmed revisions replace cached values. Conflicting claims at the same revision remain unresolved; an existing local confirmed assignment is retained as fallback. Peer replies do not change central configuration. If no confirmed mapping is available, the source remains unknown and the existing missing-measurement behavior applies.

After a reboot the local confirmed mapping is usable immediately. A node without one asks the available participants. If no confirmed answer exists, the source remains unknown and the existing missing-measurement behavior applies. A backup Dashboard is a separate planned extension.

## Control and temporary diagnostics

`Status.HeatControl` is the permanent floor-control contract. `Status.HeatDiag` contains the PI observations used by HeatHistory, and `Status.ChargeLearn` carries the latest charge-learning event. Compile-time diagnostic switches control publication of these diagnostic fields; they do not change heating control or learning behavior.
