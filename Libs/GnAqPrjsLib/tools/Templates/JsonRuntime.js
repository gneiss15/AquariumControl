// JsonRuntime.js, Version: 1.01
(function(Global) {
  "use strict";
  const Bytes = Value => new TextEncoder().encode(Value).length;
  const Own = (Value, Key) => Object.prototype.hasOwnProperty.call(Value, Key);
  function SplitRules(Text) {
    let Start = 0, Depth = 0;
    const Parts = [];
    for (let Index = 0; Index < Text.length; ++Index) {
      if (Text[Index] === "(") ++Depth;
      else if (Text[Index] === ")") --Depth;
      else if (Text[Index] === "," && !Depth) { Parts.push(Text.slice(Start, Index)); Start = Index + 1; }
    }
    Parts.push(Text.slice(Start));
    return Parts;
  }
  function Typed(Type, Value) {
    if ((Type === "TGnJsonNull" || Type === "TJsonNull")) return Value === null;
    if ((Type === "TGnJsonValue" || Type === "TJsonValue")) return Value === null || typeof Value === "string" || typeof Value === "boolean" || typeof Value === "number" && Number.isFinite(Value) || Array.isArray(Value) && Value.every(Item => Typed(Type, Item)) || Value !== null && typeof Value === "object" && Object.values(Value).every(Item => Typed(Type, Item));
    if (Type === "TGNString") return typeof Value === "string";
    if (Type === "bool") return typeof Value === "boolean";
    if (Type === "double") return typeof Value === "number" && Number.isFinite(Value);
    if (/^uint/.test(Type)) {
      const Bits = Number(Type.match(/\d+/)[0]);
      return Number.isSafeInteger(Value) && Value >= 0 && (Bits >= 53 || Value < 2 ** Bits);
    }
    if (Type === "TGnChannelId") return typeof Value === "string" || Number.isSafeInteger(Value) && Value >= 0;
    if (Type.startsWith("VariantOf(")) return SplitRules(Type.slice(10, -1)).some(Choice => Typed(Choice, Value));
    if (Type.startsWith("ArrayOf(")) return Array.isArray(Value) && Value.every(Item => Typed(Type.slice(8, -1), Item));
    if (Type.startsWith("MapOf(")) return Value !== null && typeof Value === "object" && !Array.isArray(Value) && Object.values(Value).every(Item => Typed(Type.slice(6, -1), Item));
    return Valid(Type, Value);
  }
  function Rule(Text, Value, Row) {
    if (Text.startsWith("RequiredIf(")) Text = Text.slice(Text.indexOf(",") + 1, -1);
    if (Text.startsWith("RequiredUnless(") || Text.startsWith("RequiredWhen(")) Text = Text.slice(Text.indexOf(",", Text.indexOf(",") + 1) + 1, -1);
    if (Text === "PositiveDuration") return Rule("Duration", Value, Row) && Value !== "00:00";
    if (Text.startsWith("DurationBefore(")) return Rule("PositiveDuration", Value, Row) && typeof Row[Text.slice(15,-1)] === "string" && Value < Row[Text.slice(15,-1)];
    if (Text === "FitsFloat") return Math.abs(Value) <= 3.4028234663852886e38;
    if (["ChannelAddress", "ServiceAddress", "OutputAddress"].includes(Text)) {
      const Match = /^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}(?:@([1-4]))?(?:\/([0-9]+))?$/i.exec(Value);
      return !!Match && Value.slice(0,17) !== "00:00:00:00:00:00" && (Match[2] === undefined || Number(Match[2]) <= 4294967295) && (Text !== "ServiceAddress" || Match[1] !== undefined && Match[2] === undefined);
    }
    if (Text === "HttpUrl" || Text === "WebSocketUrl") return (Text === "HttpUrl" ? /^https?:\/\/[^/\s]+(?:\/[^\s]*)?$/ : /^wss?:\/\/[^/\s]+(?:\/[^\s]*)?$/).test(Value);
    if (Text === "TemperatureProfileIfActive") {
      if (Value.Active !== true) return true;
      const Keys = ["Setpoint", "FloorW", "MainMaxW", "CapacityWhPerK", "LossWPerK", "Kp", "TiSec", "WindowSec", "DelaySec"];
      return Row.Meas === "Temperature" && Keys.every(Key => typeof Value[Key] === "number" && Number.isFinite(Value[Key])) && ["CapacityWhPerK", "Kp", "TiSec", "WindowSec"].every(Key => Value[Key] > 0) && ["FloorW", "MainMaxW", "LossWPerK"].every(Key => Value[Key] >= 0);
    }
    if (Text === "Positive") return Value > 0;
    if (Text === "Any" || Text === "Finite") return true;
    if (Text === "NonEmpty") return Value.length !== 0;
    if (Text === "Hex16") return /^[0-9a-fA-F]{16}$/.test(Value);
    if (Text === "WireId" || Text === "SourceId") return /^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@[1-4]$/.test(Value) && !Value.startsWith("00:00:00:00:00:00") && (Text !== "SourceId" || Value.endsWith("@3"));
    if (Text === "Revision") return /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/.test(Value) && !Value.startsWith("0000") && Number.isFinite(Date.parse(Value)) && new Date(Value).toISOString() === Value;
    if (Text === "TrueValue") return Value === true;
    if (Text === "EmptyOrUuid") return Value === "" || /^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$/.test(Value);
    if (Text === "TimeOfDay" || Text === "Duration") return /^\d{2}:\d{2}$/.test(Value) && Number(Value.slice(3)) < 60 && (Text === "Duration" || Number(Value.slice(0, 2)) < 24);
    if (Text === "MeasurementName") return /^[A-Za-z0-9_]{1,64}$/.test(Value) && Value !== "Kind";
    if (Text === "SensorId") return /^28-[0-9a-fA-F]{12}$/.test(Value);
    if (Text === "Ipv4") return Value.split(".").length === 4 && Value.split(".").every(Part => /^\d{1,3}$/.test(Part) && Number(Part) <= 255);
    const Kind = Text.slice(0, Text.indexOf("("));
    const Args = SplitRules(Text.slice(Text.indexOf("(") + 1, -1));
    switch (Kind) {
      case "Each": return Object.values(Value).every(Item => Rule(Args[0], Item, Row));
      case "All": return Args.every(Part => Rule(Part, Value, Row));
      case "Range": return Value >= Number(Args[0]) && Value <= Number(Args[1]);
      case "Count": return Object.keys(Value).length >= Number(Args[0]) && Object.keys(Value).length <= Number(Args[1]);
      case "Positive": return Value > 0 && Value <= Number(Args[0]);
      case "Choice": return Args.includes(Value);
      case "Utf8Bytes": return Bytes(Value) <= Number(Args[0]);
      case "TextLength": return Bytes(Value) >= Number(Args[0]) && Bytes(Value) <= Number(Args[1]);
      case "EmptyOrLength": return Value === "" || Bytes(Value) >= Number(Args[0]) && Bytes(Value) <= Number(Args[1]);
      case "ServiceIds": return Rule("WireId", Value, Row) && Args.includes(Value.slice(-1));
      case "Below": return !Own(Row, Args[0]) || typeof Row[Args[0]] === "number" && Value < Row[Args[0]];
      case "WireIdIf": return Row[Args[0]] !== true || Rule("WireId", Value, Row);
      case "TextIf": return Row[Args[0]] !== true || Value === Args[1];
      case "RangeIf": return Row[Args[0]] !== true || Value >= Number(Args[1]) && Value <= Number(Args[2]);
      case "ItemsHaveIf": return Row[Args[0]] !== true || (Value[Args[1]] || []).every(Item => Own(Item, Args[2]));
      case "ParentTextUnless": return Value[Args[0]] === Args[1] || !Own(Row, Args[2]) || Row[Args[2]] === Args[3];
      default: throw new Error("Unknown generated JSON rule: " + Text);
    }
  }
  function Valid(Name, Value, Partial = false) {
    const Schema = TYPES[Name];
    if (!Schema) return false;
    if (Schema.Alias) return Typed(Schema.Alias, Value) && Rule(Schema.Rule, Value, Value);
    if (Schema.Enum) return Number.isInteger(Value) && Schema.Enum.some(Item => Item.Value === Value && !Item.Name.endsWith("_MAX"));
    if (Schema.ValueType) return Typed(Schema.ValueType, Value) && Rule(Schema.Rule, Value, Value);
    if (Schema.Shape === "Tuple") {
      const Required = Schema.Fields.filter(Field => Field.Presence.startsWith("Required")).length;
      return Array.isArray(Value) && Value.length >= Required && Value.length <= Schema.Fields.length && Value.every((Item, Index) => {
        const Field = Schema.Fields[Index];
        return Item === null && (ACCEPT_NULL || Field.Presence.endsWith("Nullable")) || Typed(Field.Type, Item) && Rule(Field.Rule, Item, Object.fromEntries(Schema.Fields.map((Entry, Index) => [Entry.Name, Value[Index]])));
      });
    }
    if (Value === null || typeof Value !== "object" || Array.isArray(Value)) return false;
    try {
      return TYPES[Name].Fields.every(Field => {
        const Presence = Field.Presence || "Required", Key = Field.Name, Text = Field.Rule || "Any";
        if (!Own(Value, Key)) {
          const Args = SplitRules(Text.slice(Text.indexOf("(") + 1, -1));
          if (Text.startsWith("RequiredIf(") && Value[Args[0]] === true) return false;
          if (Text.startsWith("RequiredUnless(") && Value[Args[0]] !== Args[1]) return false;
          if (Text.startsWith("RequiredWhen(") && Value[Args[0]] === Args[1]) return false;
          return Partial || !Presence.startsWith("Required");
        }
        const Item = Value[Key];
        if (Item === null && (ACCEPT_NULL || Presence.endsWith("Nullable")) && Field.Kind !== "Const") return true;
        return Typed(Field.Type, Item) && (Field.Kind === "Const" ? Item === Field.Value : Rule(Text, Item, Value));
      });
    } catch { return false; }
  }
  function Read(Name, Text) {
    const Value = JSON.parse(Text);
    if (!Valid(Name, Value)) throw new Error("Invalid JSON contract: " + Name);
    return Value;
  }
  function Write(Name, Value, Space) {
    if (!Valid(Name, Value)) throw new Error("Invalid JSON contract: " + Name);
    return JSON.stringify(Value, null, Space);
  }
  Global.GnJson = Object.freeze({Valid, Read, Write, Describe: Name => TYPES[Name]});
})(globalThis);
