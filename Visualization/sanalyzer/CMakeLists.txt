# Visualization/sanalyzer/CMakeLists.txt
# GTK3: ported from GdkPixmap/GdkGC to cairo_image_surface_t.

xmms_plugin(
    TARGET    sanalyzer
    TYPE      visualization
    SOURCES
        spectrum.c
    LINK_LIBS
        m
)
