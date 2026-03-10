# Input/cdaudio/CMakeLists.txt

include(CheckIncludeFile)
check_include_file("linux/cdrom.h"   HAVE_LINUX_CDROM)
check_include_file("sys/cdio.h"      HAVE_SYS_CDIO)

if(HAVE_LINUX_CDROM OR HAVE_SYS_CDIO)
    xmms_plugin(
        TARGET    cdaudio
        TYPE      input
        SOURCES
            cdaudio.c cdaudio.h
            configure.c
            cdinfo.c  cdinfo.h
            cddb.c    cddb.h
            http.c    http.h
    )
    if(HAVE_LINUX_CDROM)
        target_compile_definitions(cdaudio PRIVATE HAVE_LINUX_CDROM=1)
    endif()
else()
    message(STATUS "  cdaudio: CD-ROM headers not found, skipping")
    set(XMMS_ENABLE_CDAUDIO OFF PARENT_SCOPE)
endif()
