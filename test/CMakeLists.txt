# message(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
add_executable(ThreadPoolTest ThreadPoolTest.cpp)
target_link_libraries(ThreadPoolTest PRIVATE pine)

add_executable(StressTest StressTest.cpp)
target_link_libraries(StressTest PRIVATE pine)


add_executable(HttpParseTest HttpParseTest.cpp)
target_link_libraries(HttpParseTest PRIVATE pine)

add_executable(HttpServerTest HttpServerTest.cpp)
target_link_libraries(HttpServerTest PRIVATE pine)

