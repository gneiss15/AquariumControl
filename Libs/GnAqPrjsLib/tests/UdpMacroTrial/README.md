# UDP macro trial

[Deutsch](README.DE.md)

Small independent preprocessor experiment; not integrated into firmware or running services.

`Example.inc.h` is the sole field definition. `Generate.py` writes wrappers with
appropriate macro definitions and an include, then runs `gcc -E -P -x c`.
One pass generates structs; another generates metadata. Only preprocessor output
is rendered as Python, JSON and HTML. The helper does not parse macro source.

Requires Python3, gcc and g++. From Prjs:

```bash
python3 Aq/Libs/GnAqPrjsLib/tests/UdpMacroTrial/Generate.py --out Aq/Libs/GnAqPrjsLib/generated/UdpSchemaDraft/MacroTrial
python3 Aq/Libs/GnAqPrjsLib/tests/UdpMacroTrial/RunTests.py
```

Outputs: `Structs.h`, `Schema.py`, `Schema.json`, `Schema.html`, inspection wrappers
and raw metadata. All include a generated-file notice naming the source.
JSON uses its first `Generated` property because JSON does not allow comments.
The C++ type aliases in the helper are test adapters only.

Tests exercise constants, required fields, nested types, arrays, optional/nullable
representations, Python/JSON parity, HTML output and C++20 compile/run with warnings
as errors. A second pass changes only the definition (Data -> Payload, 400 -> 399)
and verifies every target follows it. Test binaries stay in temporary directories.

`TExampleMeasurement` and `TExampleStatus` are test-only types. Rules are represented
as metadata but not yet executed. This experiment provides neither production
JSON codecs nor a complete UDP specification. No device/deployment tests performed.

## Selection and receive dispatch

Select individual messages with `DoMsg_Info` / `DoMsg_InfoRequest`, or use
`Do_Observer_TX`, `Do_Observer_RX`, `Do_Service_TX`, `Do_Service_RX`.
`Do_All` includes all test types. Selection macros are consumed; every JD macro
must be defined. Pass `--select Do_Observer_RX` to generate only that group.

`Dispatcher.h` is generated from the preprocessed metadata too. The caller parses
JSON once, then passes the existing object. Type text maps to an enum for the
generated switch calling RxMsg_Info or RxMsg_InfoRequest; no prefix byte is added.
Each group is compiled with a receiver implementing only the selected callback.
Handlers still receive JSON views: complete field codecs/validation remain future work.
