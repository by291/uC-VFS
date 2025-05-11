file(GLOB VFS_FATFS_SOURCES *.c)
target_sources(${target} PRIVATE ${VFS_FATFS_SOURCES})
target_include_directories(${target} PRIVATE .)