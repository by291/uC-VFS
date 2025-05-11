file(GLOB VFS_SOURCES *.c)
target_sources(${target} PRIVATE ${VFS_SOURCES})
target_include_directories(${target} PRIVATE .)

add_subdirectory(fatfs)
add_subdirectory(littlefs)
add_subdirectory(spiffs)