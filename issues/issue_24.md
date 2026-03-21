chore(gtk3): resolve all TODO(#gtk3) stubs for 1.0 parity release
## Overview

All GTK2→GTK3 migration stubs need to be resolved before we can cut a clean **v1.0 parity release** — the point where XMMS resurrect is 100% functionally equivalent to the original GTK2 XMMS, building cleanly against GTK3 with zero GTK2 API remnants.

This issue tracks the full set of `TODO(#gtk3)` markers across `xmms/`, organized by theme so they can be tackled sub-task by sub-task.  Each group is a candidate for its own focused PR.

---

## Sub-tasks

### 1 — GtkItemFactory → GMenuModel / GtkPopoverMenu  🔴  *blocking parity*
> **Files:** `xmms/main.c`, `xmms/playlistwin.c`, `xmms/equalizer.c`, `xmms/main.h`, `xmms/util.h` (also cross-referenced as `TODO(#3)`)

The biggest remaining chunk.  All four main-window right-click menus, all playlist window popup/sort/filetypes/sub-menus, and the EQ presets menu are defined as `GtkItemFactoryEntry[]` tables inside `#if 0` guards.  The factory creation calls are completely stubbed out.

- `mainwin_options_menu_entries[]`, `mainwin_songname_menu_entries[]`, `mainwin_vis_menu_entries[]`, `mainwin_general_menu_entries[]` → `GMenu` / `GtkPopoverMenu`
- `playlistwin_sort_menu_entries[]`, `playlistwin_sub_menu_entries[]`, `playlistwin_popup_menu_entries[]`, `playlistwin_playlist_filetypes[]` → `GMenu`
- `equalizerwin_presets_menu_entries[]` → native `GtkMenu` (already partially rebuilt; finish the port)
- Remove all `gtk_item_factory_get_widget` stubs (`w = NULL`, `menu = NULL`, `widget = NULL`)
- Remove `gtk_item_factory_set_translate_func` / `gtk_item_factory_create_items` / `gtk_item_factory_popup_data_from_widget` stubs

---

### 2 — GtkCList → GtkTreeView completion (skin selector + queue dialog)  🔴  *broken functionality*
> **File:** `xmms/main.c`

Three dialogs have their list widget created as `gtk_tree_view_new()` but all data operations are no-ops:

**Skin selector** (`skin_list_\*` functions):
- `gtk_clist_append` / `gtk_clist_clear` / `gtk_clist_get_row_data` / `gtk_clist_set_row_data_full`
- `gtk_clist_set_sort_column` / `gtk_clist_set_sort_type` / `gtk_clist_sort`
- `gtk_clist_freeze` / `gtk_clist_thaw` / `gtk_clist_moveto` / `gtk_clist_select_row`
- `clist->selection` → `gtk_tree_selection_get_selected()`
- `clist->focus_row` → `GtkTreePath`

**Queue dialog** (`mainwin_queue_list_\*` functions): same GtkCList → GtkTreeView + GtkListStore conversion required.

---

### 3 — Window shaping (non-rectangular windows)  🔴  *visual parity*
> **Files:** `xmms/main.c`, `xmms/equalizer.c`

XMMS's defining visual feature — the shaped skin windows — is currently disabled.

- Replace `gtk_widget_shape_combine_mask(widget, bitmap, 0, 0)` with `gtk_widget_input_shape_combine_region()` + `cairo_region_create_from_surface()` from a 1-bit `CAIRO_FORMAT_A1` surface
- `mainwin_bg_dblsize`: create cairo surface for 2× doublesize mode
- `nullmask` / `mask` creation stubs: implement via `cairo_image_surface_create(CAIRO_FORMAT_A1, w, h)`
- Remove `gdk_gc_new` / `gdk_gc_set_foreground` / `gdk_draw_rectangl
