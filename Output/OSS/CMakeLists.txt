# Output/OSS/CMakeLists.txt

include(CheckIncludeFile)
check_include_file("sys/soundcard.h" HAVE_SYS_SOUNDCARD_H)
check_include_file("soundcard.h"     HAVE_SOUNDCARD_H)

if(HAVE_SYS_SOUNDCARD_H OR HAVE_SOUNDCARD_H)
    xmms_plugin(
        TARGET    OSS
        TYPE      output
        SOURCES
            OSS.c OSS.h
            mixer.c
            about.c
            configure.c
            audio.c
            init.c
            convert.c
    )

    if(HAVE_SYS_SOUNDCARD_H)
        target_compile_definitions(OSS PRIVATE HAVE_SYS_SOUNDCARD_H=1)
    else()
        target_compile_definitions(OSS PRIVATE HAVE_SOUNDCARD_H=1)
    endif()
else()
    message(STATUS "  OSS: soundcard.h not found, skipping")
endif()
