# Shared message definitions

The repeated .inc.h definitions supply the fields, rules, message selections
and dispatch identifiers.

## C++ includes

The C++ compiler expands the definition during the normal build. A source file
can define JD and whichever other hooks it needs, select DoMsg_<Name> or a
TX/RX group, then include MessageDefinition.inc.h. JD is mandatory. Unspecified
hooks expand to nothing. The entry point removes the hooks after the include.

For the standard C++ structures and codecs, select the desired message before
including Messages.h. The compiler expands its transitive dependencies too.

~~~cpp
#define DoMsg_Request
#include "Messages.h"
~~~

For a custom expansion, include MessageDefinition.inc.h directly. Each field name is used as both the C++ member and the JSON key.

JD_MESSAGE(Type, Tag, Value) supplies the type and stable dispatch identifier.
MessageDispatch.h expands this hook to type resolution, validation and the
receiver switch. SharedTypes.inc.h defines TMsgType. A source may also use the hook with its own
message selection.

GnUdpProtocol.h reads the protocol version from the Request definition.
Changing the definition triggers normal compiler dependency tracking.

## Python, browser and documentation

Run from the Prjs directory:

~~~sh
python3 Aq/Libs/GnAqPrjsLib/tools/GenerateUdp.py
~~~

GenerateUdp.py invokes the C preprocessor with the metadata hooks from
Templates/CollectMacros.inc.h, reads that output and distributes the resulting
model to the language and documentation renderers.

- Templates/JsonRuntime.py contains the Python codec runtime.
- Templates/JsonRuntime.js contains the browser codec runtime.
- CollectDefinitions.py runs the preprocessor and collects its records.
- GenerateDocumentation.py renders the Markdown reference.
- Templates/ContractIntroduction.md contains the shared explanatory text.
- Templates/PythonFactory.py.txt contains the message-constructor template.
- GenerateJsonBrowser.py supplies the model to the browser runtime.

## Contract and caller migration

C++ structures and all object/tuple codecs are expanded directly during normal
compilation by GnJsonCompile.inc.h. There is no C++ compatibility generator.
Messages.h expands MessageDefinition.inc.h and its domain files under
Definitions/Contract/. GnUdpSchema.h uses that same source for field metadata.

A source may define Do_TUdpStatusMessage before including Messages.h. Only that
message and its transitive dependencies are expanded. Without a selection,
Messages.h expands the complete contract. A custom JD expansion may include
MessageDefinition.inc.h directly; only JD is mandatory, other hooks default to
empty. Hooks and selection macros are removed after the expansion.

Generate Python/browser codecs and documentation into a staging folder:

~~~sh
python3 Aq/Libs/GnAqPrjsLib/tools/GenerateUdp.py --out /tmp/aq-contract
~~~

The protocol revision, numeric message types, aliases, fields and validation
rules all come from the same definition catalog. SharedTypes.inc.h defines
aliases and enums. JD and JD_CONST use the same name in C++ and JSON.

TransportDefinition.inc.h is the single source of broadcast destination, UDP
port and reporting interval. TransportConfig.h expands it for C++; the generator
adds the same settings to Messages.py. MessageSelection.inc.h defines TX/RX
role groups and DoMsg aliases for per-message selections.

The Markdown, HTML and machine-readable references are generated under
generated/MessageSchema/.
