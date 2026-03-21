bug(playlist): "Add Folder" uses a minimal directory chooser instead of the standard GNOME GTK3 file chooser with sidebar/favourites
## Summary

The **Add Folder** button (second button on the playlist bottom bar) opens a plain `GtkFileChooserDialog` in directory-select mode that lacks: bookmarks sidebar, recent places, favourites, font rendering and the common GNOME file manager look-and-feel.

The **Add File** button (first button) correctly opens the full `GtkFileChooserDialog` in `GTK_FILE_CHOOSER_ACTION_OPEN` mode, which shows the standard sidebar with Favourites, Recent, Home, Music, mounted drives, and bookmarks.

## Repro Steps

1. Open the playlist window  
2. Click the **+FILE** button → standard GNOME file chooser with full sidebar ✓  
3. Click the **+DIR** button → minimal directory chooser — no sidebar, no bookmarks, no favourites ✗

## Root Cause

`playlistwin_add_dir_cb()` in `xmms/playlistwin.c` passes `GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER` without the same sidebar configuration applied to the file chooser.  The directory chooser likely derives from the old `dirbrowser` widget (`libxmms/dirbrowser.c`) rather than reusing `GtkFileChooserDialog`.

## Expected Fix

Use `GtkFileChooserDialog` with `GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER` so both dialogs share the same native GTK3 file-chooser with full bookmark/favourites support.

## Affected Files

- `xmms/playlistwin.c` — `playlistwin_add_dir_cb()` (or equivalent)  
- Possibly `libxmms/dirbrowser.c` — replace with `GtkFileChooserDialog`
