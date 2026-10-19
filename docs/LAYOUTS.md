# Layouts

A *layout* says where the elements of the interface sit. The built-in design is the *Default* layout; your own
layouts are small JSON files made with the **[Theme Studio](https://github.com/ravaninip/ThemeEditorVST)**.
A layout lists only what you changed: every element it does not mention stays where the built-in design puts it,
so a layout keeps working when a new version of the plug-in adds controls.

## Using a layout

1. In the Theme Studio, open the **Layout** tab, load Solomon's element list (it is built in), move, resize or
   hide elements, and download the file.
2. In the plug-in press **Layout** (next to the theme menu) and choose **Open layouts folder...**, or copy the
   file to:

   | System  | Folder                                          |
   |---------|-------------------------------------------------|
   | Windows | `%APPDATA%\Solomon\Layouts`                     |
   | macOS   | `~/Library/Application Support/Solomon/Layouts` |
   | Linux   | `~/.config/Solomon/Layouts`                     |

3. Choose **Reload layouts**, then pick your layout in the same menu.

The choice is a preference of the plug-in, shared by every instance and remembered between sessions. The theme
and layout menus themselves can be moved but never hidden, and they are always drawn on top, so a layout can
never lock you out of switching back to *Default*.

## File format (version 1)

```json
{
  "format": "plugin-layout",
  "version": 1,
  "plugin": "Solomon",
  "name": "Wide filter",
  "baseWidth": 980,
  "baseHeight": 720,
  "elements": {
    "knob.cutoff":      { "x": 20,  "y": 30, "w": 100, "h": 110 },
    "matrix.cell.0.0":  { "x": 400, "y": 600, "w": 100, "h": 20, "visible": false }
  }
}
```

- Coordinates are pixels in the **base window** (980 x 720). The plug-in scales them when the window is resized.
- `plugin` and the base size must match the plug-in; a layout made for another plug-in or window size is refused.
- `visible` is optional (default `true`).
- Rectangles are forced inside the window and to at least 12 x 12 pixels. Unknown element ids are ignored.
- At most 256 elements, 32 KiB per file, 100 files; symbolic links are ignored.

## Element ids

The ids are the contract between the plug-in and the Theme Studio. The plug-in can list them all, with their
default place, parameter and parent panel:

```
solomon_plugin_smoke --descriptor solomon.descriptor.json
```

| Id                                   | Element                                              |
|--------------------------------------|------------------------------------------------------|
| `panel.osc`, `panel.filter`, `panel.lfo`, `panel.ampEnv`, `panel.fltEnv`, `panel.matrix` | the six panels (background only) |
| `title`, `theme`, `layout`, `preset`, `model`, `meter` | top bar (`theme` and `layout` are locked)           |
| `header.*`                           | panel headings                                       |
| `knob.<name>`                        | knobs, e.g. `knob.cutoff`, `knob.ampA`, `knob.volume` |
| `menu.filterMode`, `menu.lfoShape`   | drop-down menus                                      |
| `graph.filter`, `graph.ampEnv`, `graph.fltEnv` | graphs                                     |
| `matrix.src.<n>`, `matrix.bar.<n>`, `matrix.dst.<n>`, `matrix.cell.<source>.<destination>` | modulation matrix |

The rectangle of a knob holds its name, dial and value. The dial has the same size in every knob (as large as the
rectangle allows, up to a maximum that scales with the window) and sits in the middle of the rectangle, so a layout
cannot make one knob look different from the others by accident.

A panel is only a background: moving it does not move what is inside (the Theme Studio can move a panel together
with its contents).
