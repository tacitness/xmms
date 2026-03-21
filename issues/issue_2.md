gtk3(libxmms): replace GdkPixmap/GdkBitmap/GdkGC with cairo_t surfaces
## GTK2 → GTK3 Migration: Drawing subsystem in libxmms/

GdkPixmap, GdkBitmap, and GdkGC were removed in GTK3. All drawing must be ported to cairo.

**Affected files:**
- `libxmms/dirbrowser.c` — uses GdkPixmap, GdkBitmap, GtkCTree, GtkCTreeNode
- `libxmms/xentry.c` — includes `gdk/gdki18n.h` (removed in GTK3)
- `libxmms/util.c` — uses GtkSignalFunc (replaced by GCallback)

**Migration mapping:**
| GTK2 | GTK3 replacement |
|------|---------|
| `GdkPixmap` | `cairo_surface_t` / `GdkPixbuf` |
| `GdkBitmap` | `cairo_surface_t` (1-bit masks via cairo) |
| `GdkGC` | `cairo_t` |
| `GtkSignalFunc` | `GCallback` |
| `gdk/gdki18n.h` | `<glib/gi18n.h` |
| `GtkCTree` | `GtkTreeView` + `GtkTreeStore` |
| `GtkCTreeNode` | `GtkTreeIter` |

**Compile errors to fix (from first GTK3 build attempt):**
```
xentry.c:36:10: fatal error: gdk/gdki18n.h: No such file or directory
dirbrowser.c:114:8: error: unknown type name 'GdkPixmap'
dirbrowser.c:160:21: error: unknown type name 'GtkCTree'
util.c:37:96: error: unknown type name 'GtkSignalFunc'
```
