find_package(Threads REQUIRED)

file(GLOB_RECURSE PINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(pine SHARED ${PINE_SOURCES})

target_compile_features(pine PUBLIC cxx_std_17)

set_target_properties(pine PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(pine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}/tcp
        ${CMAKE_CURRENT_SOURCE_DIR}/http
)

target_link_libraries(pine
    PUBLIC Threads::Threads
)
