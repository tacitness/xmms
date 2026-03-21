gtk3(skin): replace GdkPixmap-based skin loader with GdkPixbuf + cairo
The XMMS skin system (xmms/skin.c) loads Winamp .wsz skin archives and renders them using GdkPixmap blitting. GdkPixmap is removed in GTK3.

Migration path:
- Load skin images via GdkPixbuf (already GTK3-compatible)
- Render to cairo_surface_t via cairo_set_source_surface()
- Implement CSS-based theming via GtkCssProvider for coloured regions
- Skin parser to extract BMP/PNG resources from ZIP archives

This is the highest priority GTK3 migration item as the skin engine is fundamental to XMMS's identity.
