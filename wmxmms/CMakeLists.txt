# wmxmms/CMakeLists.txt — WindowMaker dockapp companion binary

add_executable(wmxmms
    getopt.c
    getopt1.c
    getopt.h
    wmxmms.c
)

target_compile_options(wmxmms PRIVATE ${XMMS_C_WARNINGS})
target_compile_definitions(wmxmms PRIVATE ${XMMS_DEFINES})

target_include_directories(wmxmms PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libxmms
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(wmxmms PRIVATE
    libxmms
    PkgConfig::GTK
    ${X11_LIBRARIES}
)

install(TARGETS wmxmms
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES wmxmms.xpm
    DESTINATION "${XMMS_DATA_DIR}"
    OPTIONAL
)
