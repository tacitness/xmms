# Input/mpg123/CMakeLists.txt — bundled mpg123 MPEG audio decoder plugin
# The XMMS 1.x tree ships its own mpg123 sources.
# On x86/x86_64 we support optional ASM optimisations via ENABLE_ASM flag per target.

# Common C sources (all platforms)
set(MPG123_COMMON_SRC
    mpg123.c
    common.c
    tabinit.c
    configure.c
    fileinfo.c
    decode_2to1.c
    decode_4to1.c
    layer1.c
    layer2.c
    layer3.c
    equalizer.c
    http.c
    dxhead.c
    id3.c
    id3_frame.c
    id3_frame_content.c
    id3_frame_text.c
    id3_frame_url.c
    id3_tag.c
    unicode.c
)

# Architecture-dependent decode/dct sources
if(ARCH_X86 AND USE_X86ASM)
    # x86 C shims + optional .s assembly files
    set(MPG123_ARCH_SRC
        decode_i386.c
        dct64_i386.c
    )
    # .s files — only include when the assembler can handle them
    # (set ENABLE_SIMD=OFF for safety during GTK3 porting)
    if(USE_SIMD)
        list(APPEND MPG123_ARCH_SRC
            decode_i586.s
            decode_3dnow.s
            dct64_3dnow.s
            dct36_3dnow.s
            getcpuflags.s
            dct64_MMX.s
            decode_MMX.s
        )
    else()
        list(APPEND MPG123_ARCH_SRC decode_i586.s)
    endif()
else()
    # Portable pure-C fallback
    set(MPG123_ARCH_SRC
        decode.c
        dct64.c
    )
endif()

xmms_plugin(
    TARGET    mpg123
    TYPE      input
    SOURCES
        ${MPG123_COMMON_SRC}
        ${MPG123_ARCH_SRC}
    LINK_LIBS
        Threads::Threads
        ZLIB::ZLIB
        libxmms
)
