# Visualization/blur_scope/CMakeLists.txt
# GTK3: ported from GdkPixmap/GdkRgbCmap to cairo_image_surface_t.
# The x86 blur_8.s assembly is not used; relies on the C fallback.

xmms_plugin(
    TARGET  blur_scope
    TYPE    visualization
    SOURCES
        blur_scope.c blur_scope.h
        config.c
        ${CMAKE_SOURCE_DIR}/Visualization/common/vis_chrome.c
)
