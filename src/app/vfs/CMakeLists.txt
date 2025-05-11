file(GLOB APP_VFS_SOURCES *.c)
target_sources(${target} PRIVATE ${APP_VFS_SOURCES})

target_include_directories(${target} PRIVATE .)