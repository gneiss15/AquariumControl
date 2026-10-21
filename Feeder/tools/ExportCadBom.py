# ExportCadBom.py, Version: 1.03
"""Export CAD quantities separately and render the maintained Feeder BOM.ods (CSV fallback)."""

import csv
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile


PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "Hw" / "CAD" / "Feeder.FCStd"
OUTPUT = PROJECT / "Hw"


def read_bom(source):
    with zipfile.ZipFile(source) as archive:
        root = ET.fromstring(archive.read("Document.xml"))
    types = {o.get("name"): o.get("type") for o in root.find("Objects")
             if o.tag == "Object"}
    objects = {
        o.get("name"): {p.get("name"): p for p in o.find("Properties")}
        for o in root.find("ObjectData")
    }

    def value(name, prop):
        element = objects[name].get(prop)
        if element is None:
            return ""
        enum = element.find("CustomEnumList")
        if enum is not None:
            index = int(element.find("Integer").get("value"))
            return list(enum)[index].get("value")
        child = next(iter(element), None)
        return child.get("value", "") if child is not None else ""

    rows = []
    groups = []
    grouped = {}
    links = objects["Asm_Assembly"]["Group"].findall("./LinkList/Link")
    for link in links:
        instance = link.get("value")
        if types[instance] != "App::Link":
            continue
        target = objects[instance]["LinkedObject"].find("XLink")
        if target.get("file"):
            raise ValueError("External assembly links are not supported: " + instance)
        name = target.get("name")
        label = value(name, "Label")
        standard = diameter = length = ""
        if types[name] == "Part::FeaturePython" and "Diameter" in objects[name]:
            kind = "Fastener"
            standard, diameter, length = (value(name, key)
                                          for key in ("Type", "Diameter", "Length"))
            if length == "Custom":
                length = str(float(value(name, "LengthCustom")))
            label = "Screw " + diameter + " x " + length
            key = (kind, standard, diameter, length)
        else:
            kind = "Assembly" if types[name] == "App::Part" else "Part"
            key = (kind, name)
        if key not in grouped:
            row = dict(Position=len(rows) + 1, Quantity=0, CADLabel=label,
                       Kind=kind, Standard=standard, Diameter=diameter,
                       Length_mm=length, SourceObjects=[], AssemblyLinks=[])
            rows.append(row)
            grouped[key] = row
        row = grouped[key]
        row["Quantity"] += 1
        row["SourceObjects"].append(name)
        row["AssemblyLinks"].append(instance)
        if kind == "Assembly":
            children = objects[name]["Group"].findall("./LinkList/Link")
            groups.append((label, [value(c.get("value"), "Label") for c in children]))
    if not rows:
        raise ValueError("No physical assembly links were found.")
    return rows, groups


# Manual CSV fields are independent of the generated CAD comparison export.
EXTRA_FIELDS = ["Description", "Material", "Manufacturer", "PartNumber",
                "Supplier", "OrderURL", "Unit", "Notes"]


def write_csv(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: "; ".join(value) if isinstance(value, list) else value
                             for key, value in row.items()})


def markdown(rows, german):
    def escape(value):
        return str(value or "").replace("|", "\\|").replace("\r\n", "\n").replace("\n", "<br>")

    if german:
        text = ["# Feeder – Stückliste", "",
                "[English](BOM.md) · [CSV](BOM.csv) · [Hardwareübersicht](README.DE.md)", ""]
        headers = ["Pos.", "Menge", "Bezeichnung"]
        labels = {"Material": "Material", "Manufacturer": "Hersteller",
                  "PartNumber": "Artikelnummer", "Supplier": "Bezugsquelle", "Notes": "Bemerkung"}
    else:
        text = ["# Feeder – bill of materials", "",
                "[Deutsch](BOM.DE.md) · [CSV](BOM.csv) · [Hardware overview](README.md)", ""]
        headers = ["Pos.", "Qty", "Item"]
        labels = {"Material": "Material", "Manufacturer": "Manufacturer",
                  "PartNumber": "Part number", "Supplier": "Supplier", "Notes": "Notes"}
    optional = [key for key in labels if any(row.get(key) or
                (key == "Supplier" and row.get("OrderURL")) for row in rows)]
    text.extend(["| " + " | ".join(headers + [labels[key] for key in optional]) + " |",
                 "| " + " | ".join(["---", "---:", "---"] + ["---"] * len(optional)) + " |"])
    for row in rows:
        item = row.get("Description") or row.get("CADLabel", "")
        if not row.get("Description"):
            if german and row.get("Kind") == "Fastener":
                item = f"Schraube {row.get('Diameter', '')} × {row.get('Length_mm', '')}"
            if row.get("Standard"):
                item += " (" + row["Standard"] + ")"
        quantity = row.get("Quantity", "")
        unit = row.get("Unit", "")
        if unit and unit != "pcs":
            quantity += " " + unit
        cells = [escape(row.get("Position")), escape(quantity), escape(item)]
        for key in optional:
            cell = escape(row.get(key))
            if key == "Supplier" and row.get("OrderURL", "").startswith(("https://", "http://")):
                url = row["OrderURL"].replace(" ", "%20").replace("(", "%28").replace(")", "%29")
                cell = "[" + (cell or "Link") + "](" + url + ")"
            cells.append(cell)
        text.append("| " + " | ".join(cells) + " |")
    return "\n".join(text) + "\n"



def render_ods(path):
    """Read the bilingual sheets without changing the workbook or its original tab."""
    ns = {"table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
          "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0"}
    with zipfile.ZipFile(path) as archive:
        root = ET.fromstring(archive.read("content.xml"))
    for language, filename in (("DE", "BOM.DE.md"), ("EN", "BOM.md")):
        table = root.find(".//table:table[@table:name='" + language + "']", ns)
        if table is None:
            raise ValueError("Missing BOM language sheet: " + language)
        rows = []
        for row in table.findall("table:table-row", ns):
            cells = []
            for cell in row:
                value = " ".join("".join(p.itertext()) for p in cell.findall("text:p", ns))
                count = min(7 - len(cells), int(cell.get("{" + ns["table"] + "}number-columns-repeated", "1")))
                cells.extend([value] * max(0, count))
                if len(cells) == 7:
                    break
            if any(cells):
                rows.append(cells + [""] * (7 - len(cells)))
        german = language == "DE"
        text = ["# Feeder – " + ("Stückliste" if german else "bill of materials"), "",
                ("[English](BOM.md)" if german else "[Deutsch](BOM.DE.md)") +
                " · [ODS](BOM.ods) · " + ("[Hardwareübersicht](README.DE.md)" if german else "[Hardware overview](README.md)"), ""]
        notes = False
        for index, cells in enumerate(rows):
            if cells[0] in ("Notes", "Anmerkungen"):
                notes = True
                text.extend(["", "## " + cells[0], ""])
            elif notes:
                text.append(cells[0] + ". " + cells[1])
            else:
                cells = [v.replace("|", "\\|").replace("\n", "<br>") for v in cells]
                for column, value in enumerate(cells):
                    if index and value.startswith(("https://", "http://")):
                        cells[column] = "[Link](" + value.replace(" ", "%20") + ")"
                text.append("| " + " | ".join(cells) + " |")
                if index == 0:
                    text.append("| " + " | ".join(["---", "---:"] + ["---"] * 5) + " |")
        (OUTPUT / filename).write_text("\n".join(text) + "\n", encoding="utf-8")


def main(argv=None):
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--render-only", action="store_true",
                        help="Update Markdown from BOM.ods (CSV fallback) without reading CAD.")
    args = parser.parse_args(argv)
    master = OUTPUT / "BOM.csv"
    if not args.render_only:
        rows, _ = read_bom(SOURCE)
        comparison = PROJECT / "generated" / "CAD-BOM.csv"
        write_csv(comparison, rows, list(rows[0]))
        print(f"CAD comparison export: {comparison}")
        # Seed a missing master once; never rewrite an existing maintained CSV.
        if not master.exists():
            fields = list(rows[0]) + EXTRA_FIELDS
            for row in rows:
                row.update({key: "pcs" if key == "Unit" else "" for key in EXTRA_FIELDS})
            write_csv(master, rows, fields)
    if (OUTPUT / "BOM.ods").exists():
        render_ods(OUTPUT / "BOM.ods")
        print("Rendered DE and EN from BOM.ods; workbook preserved.")
        return
    with master.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not {"Position", "Quantity", "CADLabel"}.issubset(reader.fieldnames or []):
            raise ValueError("BOM.csv requires Position, Quantity and CADLabel columns.")
        maintained = list(reader)
    (OUTPUT / "BOM.md").write_text(markdown(maintained, False), encoding="utf-8")
    (OUTPUT / "BOM.DE.md").write_text(markdown(maintained, True), encoding="utf-8")
    print(f"Rendered {len(maintained)} maintained BOM rows; existing BOM.csv preserved.")


if __name__ == "__main__":
    main()
