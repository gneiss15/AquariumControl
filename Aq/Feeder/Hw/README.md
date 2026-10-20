<!-- Feeder/Hw/README.md, Version: 1.06 -->
# Feeder – hardware

[Deutsch](README.DE.md)

Feeder is a complete DIY design. This directory contains the mechanical CAD model, images and documentation for the electronics modules and their assembly. The electronics use purchased modules exclusively.

## Operating principle

A servo turns a metering disc with a cylindrical cavity through 180° for each feed. In one position, food enters the cavity from the container above it. In the other position, the measured portion falls into the aquarium.

## FeederV2 variant with O-ring

The [FeederV2.FCStd](CAD/FeederV2.FCStd) variant adds an O-ring groove in `Top` and a 22 × 2.5 mm O-ring (inside diameter × cross-section). Reason for the change: food can become trapped between the metering disc and housing. With approximately 0.2 mm protrusion, the ring is intended to prevent this. The CAD design and exploded view have been updated and visually checked by the designer. Physical testing is still pending, including sealing performance, friction, servo load and wear. The O-ring material remains to be selected.

`Feeder.FCStd` retains the previous design. The HTML/PNG exploded view linked below shows that design; the updated FeederV2 exploded view is available in its CAD file.

## Directory layout

- `CAD/`: mechanical CAD data with assembly and exploded view. Editable model: [Feeder.FCStd](CAD/Feeder.FCStd).
- `Images/`: photos, operating-screen images, the exploded-view preview and the standalone HTML 3D viewer.
- `Schematics/`: circuit, connection and wiring documentation, including the WiFi Kit 8 module pinout (manufacturer reference; not bundled).

## Exploded view

[Interactive 3D view and controls](ExplodedView.md)

[![Feeder exploded view](Images/FeederExploded.png)](Images/FeederExploded.html)

The image opens the offline HTML viewer. Rotate or zoom the model, use the explosion slider to separate the components and hide individual parts.

## Wiring

[![Feeder wiring](Schematics/FeederWiring.png)](Schematics/FeederWiring.svg)

[Editable SVG](Schematics/FeederWiring.svg) · [Connection list](Schematics/Wiring.md)

The cables are plugged in directly. At the sockets and servo, some cables are soldered at one end. The 4.7 kΩ pull-up is soldered directly at the sensor socket between data and 3.3 V. The diagram uses 13 single-wire Dupont leads with the sensor connection, or 10 without it. The temperature probe cable is separate.

## Bill of materials

[Bill of materials](BOM.md) · [Edit ODS](BOM.ods)

The bill of materials is maintained in `BOM.ods`. The `DE` and `EN` sheets contain the German and English versions. Keep quantities, sources and technical information aligned in both language sheets when editing. Items without CAD models are included. Do not order parts supplied with purchased assemblies twice.

The columns contain item number, quantity, CAD label, description, manufacturer/supplier, source and note references. Additional notes appear below the table.

After saving the ODS, update both Markdown language versions from the Prjs root:

```sh
python3 Aq/Feeder/tools/ExportCadBom.py --render-only
```

Running `python3 Aq/Feeder/tools/ExportCadBom.py` exports current CAD quantities to `generated/CAD-BOM.csv` under the project root. Transfer relevant changes into the ODS as needed. The script does not modify the ODS.

## Assembly and first-time setup

**Coming soon – incomplete.** Step-by-step assembly, sensor connector contact numbers, cable lengths and first-time setup including servo calibration are still to be documented. CAD, bill of materials and wiring documents are already available. Feedback from builders is welcome.
