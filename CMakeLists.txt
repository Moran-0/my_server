cmake_minimum_required(VERSION 3.10)

project(my_server VERSION 0.1 DESCRIPTION "my server project" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy during CMake builds when available" OFF)
# 导出编译数据库（compile_commands.json）让 IDE 和分析工具[代码补全工具（如 clangd）、静态分析工具（如 Clang-Tidy）]准确理解项目的编译方式
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src/include)

find_program(CLANG_FORMAT_BIN NAMES clang-format)
find_program(CLANG_TIDY_BIN NAMES clang-tidy)

if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY_BIN)
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_BIN};
            --config-file=${PROJECT_SOURCE_DIR}/.clang-tidy
        )
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found.")
    endif()
endif()

include_directories(${INCLUDE_DIRECTORIES})
add_subdirectory(src)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE pine)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE pine)

add_subdirectory(test)

if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE PROJECT_FORMAT_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/*.cpp"
        "${PROJECT_SOURCE_DIR}/*.h"
        "${PROJECT_SOURCE_DIR}/*.hpp"
    )

    list(FILTER PROJECT_FORMAT_FILES EXCLUDE REGEX "/build/")

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i ${PROJECT_FORMAT_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Formatting C++ sources with clang-format"
    )
else()
    message(STATUS "clang-format not found, 'format' target will not be available.")
endif()

if(CLANG_TIDY_BIN)
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "Run clang-tidy by configuring with -DENABLE_CLANG_TIDY=ON and rebuilding."
        COMMENT "Helper target for clang-tidy usage"
    )
else()
    message(STATUS "clang-tidy not found, tidy integration is configured but currently unavailable.")
endif()
