file(GLOB VFS_LITTLEFS_SOURCES *.c)
target_sources(${target} PRIVATE ${VFS_LITTLEFS_SOURCES})
target_include_directories(${target} PRIVATE .)