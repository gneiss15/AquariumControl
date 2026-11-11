## Field name abbreviations

Abbreviations also apply to word components inside field names. Descriptions use full words. Identity remains unchanged. Unit suffixes use C-compatible spelling.

| Full word | Abbreviation |
| :--- | :--- |
| Device | Dev |
| Hardware | Hw |
| Capabilities | Caps |
| Channel | Ch |
| Channels | Chs |
| Configuration | Cfg |
| Config | Cfg |
| Function | Func |
| Profile | Prof |
| Profiles | Profs |
| Revision | Rev |
| Version | Ver |
| Source | Src |
| Sources | Srcs |
| Temperature | Temp |
| Measurement | Meas |
| Measurements | Meas |
| Input | In |
| Output | Out |
| Error | Err |
| Errors | Errs |
| Sequence | Seq |
| Counter | Cnt |
| Learning | Learn |
| Calibration | Cal |
| Override | Ovr |
| Diagnostics | Diag |
| Seconds | Sec |
| Milliseconds | Ms |
| Percent | Pct |
| Watts | W |

## UDP part

| Part | Meaning |
| :--- | :--- |
| Live | Included in the regular compact Status data. |
| Info | Descriptive data sent separately on request for the matching InfoRev. |
| Omit | Absent from both the regular compact Status and its Info replies. Access depends on the field: power meter diagnostics via HTTP, source mappings via SourceInfo. |

## Protocol versions

Ver identifies the version of the corresponding message format. It allows receivers and senders to be updated in stages:

1. Update receivers to accept and process both the current and the next version.
2. Update senders to publish the next version.
3. Once the migration is complete, stop accepting the previous version.

Receivers reject unsupported versions. An embedded structure with its own Ver is versioned independently of the outer message.

## Field presence

Required means the field must be present. Optional means it may be omitted. The decoder accepts JSON null for every field; the field evaluation determines whether null is allowed and what it means.

## Validation rules

Rules can be combined. A constant in the rule column is the value required in the message.

| Rule | Meaning |
| :--- | :--- |
| Any | The declared JSON type applies without an additional rule. |
| HttpUrl | Complete URL using the http or https scheme. |
| WebSocketUrl | Complete URL using the ws or wss scheme. |
| Choice(a,b,...) | The value must equal one of the listed alternatives. |
| NonEmpty | A string must contain at least one character. |
| Count(min,max) | The collection must contain between min and max entries, inclusive. |
| Range(min,max) | The numeric value must be within the inclusive bounds. |
| Utf8Bytes(max) | The text may contain at most max bytes when UTF-8 encoded. |
| Hex16 | A 16-digit hexadecimal revision identifier. |
| RequiredIf(Compact,Hex16) | With Compact set to true, the field must be present and satisfy Hex16. |
| ItemsHaveIf(Compact,Chs,Id) | With Compact set to true, every Chs entry must provide Id. |
| Revision | UTC revision timestamp in the protocol format. |
| ServiceAddress | An address that identifies a service. |
| ChannelAddress | Physical address matching TChannelId. |
| OutputAddress | An address that resolves to exactly one switchable output. |
| MeasurementName | Name of the requested measurement, for example Temperature or BatteryPercent. |

| All(rules...) | Every listed rule must hold. |
| Each(rule) | Every array element or map value must satisfy the rule. |
| TemperatureProfileIfActive | When Active is true, require the complete operational heating configuration and the Temperature measurement kind. |
| Positive | Numeric value greater than zero. |
| TimeOfDay | Time in HH:MM, hour 00–23 and minute 00–59. |
| Duration | Duration in HH:MM with minutes 00–59. |
| PositiveDuration | Nonzero duration in HH:MM. |
| DurationBefore(field) | Positive duration smaller than the named duration field. |


HTTP response types are selected by the requested URL. WebSocket and SSE event types are selected by their event context or an explicit Type field.

The UDP part column applies only when a structure participates in compact UDP status.

## Scope and naming

All contract type names begin with T.

| Prefix | Purpose |
| :--- | :--- |
| TUdp | UDP messages and UDP-specific payloads |
| THttp | HTTP request or response envelopes |
| TWs | WebSocket event envelopes and connection metadata |
| TSse | Server-Sent Events envelopes |
| TJson | Shared JSON structures used in more than one context |

Shared scalar types have descriptive names such as TChannelId and TUnixTime. Existing library types retain their names, for example TGNString.

Network endpoints use complete URLs, including scheme, host, port when required and path.

Device-local hardware selection and filesystem paths are outside this network contract. UDP broadcast destination and port are shared build constants. The default is 255.255.255.255 on port 44444.

