cmake_minimum_required(VERSION 3.20)
project(MinecraftModOption
    VERSION 1.0.0
    DESCRIPTION "Minecraft模组管理器 - 检测、更新、回滚与删除"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_0)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(LIBCURL REQUIRED libcurl)

set(SOURCES
    src/main.c
    src/app.c
    src/main_window.c
    src/mod_list.c
    src/mod_list_view.c
    src/dialogs.c
    src/scanner.c
    src/modrinth.c
    src/curseforge.c
    src/updater.c
    src/rollback.c
    src/deleter.c
    src/cache.c
    src/config.c
    src/network.c
    src/hash.c
)

set(HEADERS
    src/app.h
    src/main_window.h
    src/mod_list.h
    src/mod_list_view.h
    src/dialogs.h
    src/scanner.h
    src/modrinth.h
    src/curseforge.h
    src/updater.h
    src/rollback.h
    src/deleter.h
    src/cache.h
    src/config.h
    src/network.h
    src/hash.h
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${GTK4_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    src
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${GTK4_LIBRARY_DIRS}
    ${CJSON_LIBRARY_DIRS}
    ${LIBCURL_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GTK4_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${LIBCURL_LIBRARIES}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
