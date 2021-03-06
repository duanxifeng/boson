set(LIBRARIES "${LIBRARIES}" ${CMAKE_THREAD_LIBS_INIT} boson wfqueue)

set(catch_exe_source_list)
add_custom_target(build-test)
macro(add_project_test name)
  if("${ARGV1}" STREQUAL "CATCH")
    list(APPEND catch_exe_source_list ${name}.cc)
  else()
    add_executable(${name} ${name}.cc)
    target_link_libraries(${name} ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
    add_dependencies(build-test ${name})
    add_test(${name} ${name})  
  endif()
endmacro()

# Reference test sources
#add_project_test(test1 CATCH)
add_project_test(channel CATCH)
add_project_test(event_loop CATCH)
add_project_test(memory_flat_unordered_set CATCH)
add_project_test(memory_sparse_vector CATCH)
add_project_test(queues_weakrb CATCH)
add_project_test(queues_vectorized_queue CATCH)
add_project_test(routine CATCH)
add_project_test(select CATCH)
add_project_test(semaphore CATCH)
add_project_test(static CATCH)
add_project_test(test_local_ptr CATCH)
add_project_test(test_wfqueue CATCH)
add_project_test(test_mpsc CATCH)
add_project_test(shared_buffer CATCH)
add_project_test(sockets CATCH)

# Create main test executable
add_executable(unit_tests ${catch_exe_source_list})
target_link_libraries(unit_tests boson bosonqueues testlib catch wfqueue)
add_dependencies(build-test unit_tests)
project_add_test(lib${PROJECT_NAME}_unit_tests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/unit_tests)


# Perf test
macro(add_perf_test_exe name)
  add_executable(${name} perf/${name}.cc)
  target_link_libraries(${name} boson bosonqueues testlib catch wfqueue)
endmacro()

add_perf_test_exe(ramgrowth01)
