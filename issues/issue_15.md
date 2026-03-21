feat(prefs): port preferences dialog from GTK2 stub to full GTK3 implementation
## Overview

`xmms/prefswin.c` is currently a **86-line stub** that renders a single label: `"Preferences dialog not yet ported to GTK3."` The original implementation (`prefswin.c.gtk2-bak`, 1477 lines) used deprecated GTK2-only widgets throughout.

## Required Migration

| GTK2 Widget | GTK3 Replacement |
|---|---|
| `GtkCList` (plugin lists) | `GtkTreeView` + `GtkListStore` |
| `GtkOptionMenu` (output plugin selector) | `GtkComboBoxText` |
| `GtkTooltips` | `gtk_widget_set_tooltip_text()` |
| `GTK_OBJECT()` | `G_OBJECT()` |
| `gtk_signal_connect()` | `g_signal_connect()` |
| `GtkTable` | `GtkGrid` |
| `gtk_widget_size_request()` | `gtk_widget_get_preferred_size()` |

## Preferences Tabs to Implement

1. **Audio I/O** — output plugin selector (GtkComboBoxText), audio buffer sizes, bit depth
2. **Input Plugins** — GtkTreeView list with Enable/Disable/Configure/Info buttons  
3. **Output Plugins** — GtkTreeView list with Configure button
4. **Effect Plugins** — GtkTreeView list with Enable/Configure buttons
5. **General Plugins** — GtkTreeView list with Enable/Configure buttons
6. **Visualization Plugins** — GtkTreeView list (also linked from `prefswin_show_vis_plugins_page()`)
7. **Player** — fonts, scrolling title, shading, skin selector
8. **Playlist** — metadata lookup, title formatting string

## Stub Functions Needing Implementation

- `create_prefs_window()` — builds the full GtkNotebook tabbed dialog
- `prefswin_vplugins_rescan()` — refreshes visualization plugin list
- `prefswin_show_vis_plugins_page()` — shows prefs and switches to vis tab

## Notes

- All plugin list data comes from `get_input_list()`, `get_output_list()`, `get_vis_plugin_list()`, etc. (already GTK3-compatible)
- The `configure_plugin` / `enable_plugin` / `disable_plugin` callbacks are already ported
- Font selection: GTK2 used `GtkFontButton`; GTK3 keeps the same API
- This unblocks: visualization plugin selection, audio output reconfiguration, plugin hot-reload
