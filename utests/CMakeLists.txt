 #############################################################################
 # unit test executables                                                     #
 #############################################################################

 add_executable(test_thread_pool test_thread_pool.cpp)
 target_link_libraries(test_thread_pool ${Boost_LIBRARIES} milecsa_dispatchq)

 #############################################################################
 # run unit tests                                                            #
 #############################################################################

 add_custom_target(
         run_utests ALL
         ctest WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )

 add_dependencies(run_utests test_thread_pool)

 add_test (NAME test_thread_pool COMMAND test_thread_pool)
 enable_testing ()
