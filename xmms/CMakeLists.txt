# xmms/CMakeLists.txt — main XMMS executable

set(XMMS_SOURCES
    bmp.c           bmp.h
    skin.c          skin.h
    util.c          util.h
    output.c        output.h
    fft.c           fft.h
    input.c         input.h
    effect.c        effect.h
    general.c       general.h
    visualization.c visualization.h
    fullscreen.c    fullscreen.h
    pluginenum.c    pluginenum.h
    playlist.c      playlist.h
    controlsocket.c controlsocket.h
    dock.c          dock.h
    widget.c        widget.h
    sbutton.c       sbutton.h
    pbutton.c       pbutton.h
    tbutton.c       tbutton.h
    textbox.c       textbox.h
    menurow.c       menurow.h
    hslider.c       hslider.h
    monostereo.c    monostereo.h
    vis.c           vis.h
    svis.c          svis.h
    number.c        number.h
    playstatus.c    playstatus.h
    playlist_list.c     playlist_list.h
    playlist_slider.c   playlist_slider.h
    playlist_popup.c    playlist_popup.h
    eq_graph.c      eq_graph.h
    eq_slider.c     eq_slider.h
    main.c          main.h
    skinwin.c       skinwin.h
    prefswin.c      prefswin.h
    playlistwin.c   playlistwin.h
    equalizer.c     equalizer.h
    hints.c         hints.h
    about.c         about.h
    sm.c            sm.h
    mpris2.c        mpris2.h
    plugin.h
    xmms.h
    getopt.c        getopt1.c  getopt.h
    urldecode.c     urldecode.h
    dnd.h
    mkdtemp.c
)

add_executable(xmms ${XMMS_SOURCES})

target_compile_options(xmms PRIVATE ${XMMS_C_WARNINGS})
target_compile_definitions(xmms PRIVATE ${XMMS_DEFINES})

target_include_directories(xmms PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libxmms
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(xmms PRIVATE
    libxmms
    PkgConfig::GTK
    Threads::Threads
    ${X11_LIBRARIES}
    ${SM_LIBS}
    ${VM_LIBS}
    m
    # dlopen for plugin loading
    ${CMAKE_DL_LIBS}
)

# GTK3: export all symbols so dlopen'd plugins can resolve symbols defined in
# the main binary (e.g. get_current_effect_plugin, xmms_get_gentitle_format)
target_link_options(xmms PRIVATE -Wl,--export-dynamic)

# Install
install(TARGETS xmms
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Public plugin API headers
install(FILES plugin.h fullscreen.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/xmms"
)

# Desktop entry is installed from data/xmms.desktop via the root CMakeLists.txt
# (issue #38 — that file has Categories=, Keywords=, and StartupWMClass= required by GNOME)

# Skin subdirectory
add_subdirectory(defskin EXCLUDE_FROM_ALL)
