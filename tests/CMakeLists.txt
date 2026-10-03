add_executable(test_lru test_lru.cpp)
target_link_libraries(test_lru PRIVATE cache)
add_test(NAME test_lru COMMAND test_lru)

add_executable(test_lfu test_lfu.cpp)
target_link_libraries(test_lfu PRIVATE cache)
add_test(NAME test_lfu COMMAND test_lfu)
