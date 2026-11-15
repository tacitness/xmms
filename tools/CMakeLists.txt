add_executable(xmms-remote-sync remote_sync.c)
target_compile_options(xmms-remote-sync PRIVATE ${XMMS_C_WARNINGS})
target_link_libraries(xmms-remote-sync PRIVATE libxmms PkgConfig::GLIB)
install(TARGETS xmms-remote-sync RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
