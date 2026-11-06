<!-- UdpStatus.md, Version: 1.01 -->
# UDP status and descriptions

[Deutsch](UdpStatus.DE.md)

Devices publish status on UDP port **44444**, normally every **10 seconds**. State changes, overrides, errors, validity changes, startup and reconnection also trigger a status. Completed measurement cycles supply the published values. Numeric changes alone do not trigger extra broadcasts.

HTTP status APIs retain their full format. Older, uncompressed version-2 status senders remain readable.

```json
{"Version":2,"Type":"Status","ID":"02:11:22:33:44:55@4","Compact":true,"InfoRevision":"0123456789abcdef","Status":{"BootCounter":7,"Channels":[{"Id":0,"State":true}],"Temperature":25.4}}
```

`ID` is the physical MAC@ServiceId. Function names, channel properties and other descriptions are resolved separately. `InfoRevision` is a content fingerprint, not a time limit. An unchanged description stays valid indefinitely. A changed fingerprint invalidates the cached description for that sender.

A receiver asks the sending IP on UDP port **44500 + ServiceId** for the specific missing fields (for example, MultiTimer: 44501):

```json
{"Version":2,"Type":"InfoRequest","ID":"02:11:22:33:44:55@4","InfoRevision":"0123456789abcdef","Fields":["Identity","Channel:0"]}
```

Description fields are `Root`, `Identity`, `Channel:<JSON channel Id>` and `Measurement:<measurement Id>`. Responses use `Type: Info`, the same `ID`/`InfoRevision`, `Field` and `Data` (one complete serialized JSON object). Each description part uses one datagram. Each service uses its own control socket (44501–44504); independent observers such as HeatHistory use an ephemeral reply socket. Both requests and replies therefore reach the correct application when Dashboard and MultiTimer share one IP. Missing replies are retried with backoff. Description responses never refresh a device's online age or create measurement samples.

Compact channel fields retain `Id` and `State`. Optional `Bits` encodes input state (1), active override (2), override state (4), calibration override (8), different automatic state (16), automatic state true (32), null input (64) and null automatic state (128). `Until` carries the override deadline. Consumers reconstruct the normal rich channel object before passing it to their existing application logic.

## Source lookup without a running Dashboard

Confirmed source assignments remain stored on consumers. They do not expire with age. Only Dashboard changes the authoritative assignment; peers can answer from their last centrally confirmed mapping.

```json
{"Version":2,"Type":"SourceRequest","Function":"FeederAq1"}
```

The request can be broadcast when the source is unknown. A peer or Dashboard replies to the requester's IP/reply port with `Type: SourceInfo`, its own `ID`, `Function`, `Source` (MAC@3), `Revision` (the central assignment revision) and `Confirmed: true`. Newer confirmed revisions replace cached claims; conflicting claims at the same revision are not guessed. An existing confirmed local assignment remains the fallback. Peer replies do not write central configuration or turn peers into configuration authorities.

After a reboot the local confirmed mapping is usable immediately. A node without one asks the available participants. If no confirmed answer exists, the source remains unknown and the existing missing-measurement behavior applies. A backup Dashboard is a separate planned extension.

## Control and temporary diagnostics

`Status.HeatControl` is the permanent floor-control contract: `Schema`, `Mode`, `Profile`, `Config.FloorDevice`, `Config.FloorChannel`, and `FloorRequest`. It retains synchronized `CurrentTime`, the 60-second lease, binding checks and fallback behavior independently of observation. Receivers prefer it; `HeatPi` remains a compatibility fallback only when `HeatControl` is absent. Explicit null control revokes the request.

`GN_DEBUG_HEAT_PI` gates publication of the temporary `Status.HeatPi` observation block, still needed by HeatHistory. `GN_DEBUG_CHARGE_LEARNING` gates the repeated last completed `Status.ChargeLearning` event and Dashboard journal. Both switches live in `GnTemporaryDiagnostics.h`, default on. Setting either to zero must not change control or learning.
