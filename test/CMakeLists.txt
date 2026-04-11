# message(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
add_executable(ThreadPoolTest ThreadPoolTest.cpp)
target_link_libraries(ThreadPoolTest PRIVATE pine)

add_executable(mytest test.cpp)
target_link_libraries(mytest PRIVATE pine)
