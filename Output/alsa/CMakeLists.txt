# Output/alsa/CMakeLists.txt

xmms_plugin(
    TARGET    ALSA
    TYPE      output
    SOURCES
        alsa.c alsa.h
        about.c
        audio.c
        configure.c
        init.c
    LINK_LIBS
        PkgConfig::ALSA
        Threads::Threads
)

# Work around a GCC internal compiler error (vectorizable_simd_clone_call)
# triggered by the alsa_loop function on some GCC versions.
target_compile_options(ALSA PRIVATE -fno-tree-vectorize)
