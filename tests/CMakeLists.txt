function(add_cache_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE cache)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    TIMEOUT 600
    ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1")
endfunction()

add_cache_test(test_lru)
add_cache_test(test_lfu)
add_cache_test(stress_test)

# Deliberately racy demo: built only on request, never run by ctest.
add_executable(tsan_race_demo EXCLUDE_FROM_ALL tsan_race_demo.cpp)
target_link_libraries(tsan_race_demo PRIVATE cache)
