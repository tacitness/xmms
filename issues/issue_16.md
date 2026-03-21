feat(eq): port EQ preset list dialog from GtkCList to GtkTreeView
## Overview

The Equalizer presets manager dialog (`equalizerwin_load/save/delete_preset_w` in `xmms/equalizer.c`) uses `GtkCList` which was removed in GTK3.

## Current State

Three TODO stubs in `equalizer.c`:
```c
// ~line 1067
/* TODO(#gtk3): gtk_clist_get_text removed */
clist = NULL;

// ~line 1079
if ((clist && FALSE) /* TODO(#gtk3): clist->selection */) {
    /* TODO(#gtk3): gtk_clist_get_text removed */
}
```

The preset list renders visually (the dialog opens) but selecting a preset does nothing because the selection callback cannot read the selected row text.

## Migration

```c
// GTK2
GtkCList *clist = gtk_clist_new(1);
// selection: clist->selection -> ((GtkCListRow*)sel->data)->data / gtk_clist_get_text()

// GTK3 replacement
GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
// selection: gtk_tree_selection_get_selected() -> gtk_tree_model_get()
```

## Affected Functions
- `equalizerwin_load_preset_w()`
- `equalizerwin_save_preset_w()`  
- `equalizerwin_delete_preset_w()`
- `equalizerwin_eqpreset_select_row_cb()`
