# Feeder – interactive exploded view

[Open the interactive 3D view](Images/FeederExploded.html)

[![Feeder exploded view](Images/FeederExploded.png)](Images/FeederExploded.html)

The preview links to a standalone HTML file. Open it in a current browser with WebGL support. The model and viewer are embedded in the file; no internet connection or FreeCAD installation is required. Markdown viewers display the image; opening the link provides the interactive model.

## Controls

- Drag to rotate; use the mouse wheel to zoom.
- Shift-drag to pan.
- Move the Explosion slider between the assembled and exploded positions.
- Use the component checkboxes to hide individual parts; All parts restores them.
- Fit view resets the viewing direction, zoom and pan.
- Save PNG exports the current canvas view.
- With the canvas focused, arrow keys rotate, plus/minus zoom and Home fits the view.

The interface currently uses German labels: `Zusammengebaut` (assembled), `Explodiert` (exploded), `Ansicht einpassen` (fit view), `Alle anzeigen` (show all), and `PNG speichern` (save PNG).

## Model source

Snapshot from [Feeder.FCStd](CAD/Feeder.FCStd), 20 October 2026: 22 assembly components and 17 saved exploded-view moves. The exported mesh is for documentation; the FreeCAD document remains the editable source. Regenerate the HTML and preview after changing the CAD model or its exploded-view moves. Intermediate slider positions interpolate the final component offsets; they do not replay the individual assembly steps.

[Deutsche Beschreibung](ExplodedView.DE.md)
