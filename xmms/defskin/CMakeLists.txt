# xmms/defskin/CMakeLists.txt — install default skin data
# The defskin is a collection of bitmaps installed as XMMS's built-in skin.

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION "${XMMS_DATA_DIR}/Skins/Default"
    FILES_MATCHING
        PATTERN "*.bmp"
        PATTERN "*.BMP"
        PATTERN "*.png"
        PATTERN "*.PNG"
    PATTERN "CMakeFiles" EXCLUDE
)
