find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

file(GLOB_RECURSE PINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(pine SHARED ${PINE_SOURCES})

target_compile_features(pine PUBLIC cxx_std_17)

set_target_properties(pine PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(pine PRIVATE
    ${MYSQL_INCLUDE_DIRS}
)
target_include_directories(pine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}/tcp
        ${CMAKE_CURRENT_SOURCE_DIR}/http
        ${CMAKE_CURRENT_SOURCE_DIR}/timer
        ${CMAKE_CURRENT_SOURCE_DIR}/log
        ${CMAKE_CURRENT_SOURCE_DIR}/sql
)

target_link_libraries(pine
    PUBLIC Threads::Threads
    PRIVATE ${MYSQL_LIBRARIES}
)
