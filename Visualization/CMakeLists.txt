# Visualization/CMakeLists.txt
# GTK3 migration: visualization plugins ported from GdkPixmap/GdkGC to cairo.

if(XMMS_ENABLE_BLURSCOPE)
    add_subdirectory(blur_scope)
endif()

if(XMMS_ENABLE_SANALYZER)
    add_subdirectory(sanalyzer)
endif()
