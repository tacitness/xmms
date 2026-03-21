# Visualization/sanalyzer/CMakeLists.txt
# GTK3: ported from GdkPixmap/GdkGC to cairo_image_surface_t.

xmms_plugin(
    TARGET    sanalyzer
    TYPE      visualization
    SOURCES
        spectrum.c
        ${CMAKE_SOURCE_DIR}/Visualization/common/vis_chrome.c
    LINK_LIBS
        m
)
