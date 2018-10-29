cmake_minimum_required(VERSION 3.9)
project(milecsa_dispatchq)
enable_testing()

set(CMAKE_CXX_STANDARD 14)

add_library(milecsa_dispatchq src/milecsa_queue.cpp)

#############################################################################
# Install                                                                   #
#############################################################################
install (TARGETS milecsa_dispatchq DESTINATION lib)
install (FILES include/milecsa_queue.h DESTINATION include/milecsa/dispatch)

#############################################################################
# Searching BOOST                                                           #
#############################################################################

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     ON)
add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})

find_package(Boost 1.66.0 REQUIRED COMPONENTS
        chrono
        thread system date_time  unit_test_framework)

if(Boost_FOUND)

    include_directories(
            include
            ${Boost_INCLUDE_DIRS}
            )
    link_directories("${Boost_LIBRARY_DIRS}")

    #############################################################################
    # unit test executables                                                     #
    #############################################################################

    add_executable(test_main_thread_async utests/test_main_thread_async.cpp)
    target_link_libraries(test_main_thread_async ${Boost_LIBRARIES} milecsa_dispatchq)


    #############################################################################
    # run unit tests                                                            #
    #############################################################################

    add_custom_target(
            run_utests ALL
            ctest WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )

    add_dependencies(run_utests test_main_thread_async)

endif()
