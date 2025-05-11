file(GLOB VFS_SPIFFS_SOURCES *.c)
target_sources(${target} PRIVATE ${VFS_SPIFFS_SOURCES})
target_include_directories(${target} PRIVATE .)