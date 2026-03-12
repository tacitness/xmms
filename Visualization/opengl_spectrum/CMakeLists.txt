# Visualization/opengl_spectrum/CMakeLists.txt
# GTK3 port: raw X11/GLX + pthread replaced with GtkGLArea + g_timeout_add.
# Requires: OpenGL (GL), libepoxy (for GtkGLArea function pointer management).

xmms_plugin(
    TARGET  OpenGLSpectrum
    TYPE    visualization
    SOURCES
        opengl_spectrum.c opengl_spectrum.h
        configure.c
        ${CMAKE_SOURCE_DIR}/Visualization/common/vis_chrome.c
    LINK_LIBS
        OpenGL::GL
        PkgConfig::Epoxy
)
