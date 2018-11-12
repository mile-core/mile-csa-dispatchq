cmake_minimum_required(VERSION 3.9)
project(milecsa_dispatchq)
enable_testing()

set(CMAKE_CXX_STANDARD 14)

add_library(milecsa_dispatchq
        src/milecsa_queue.cpp
        )

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
set(CMAKE_CXX_FLAGS -pthread)

add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})

find_package(Boost 1.66.0 REQUIRED COMPONENTS
        chrono
        thread system date_time  unit_test_framework)

if(Boost_FOUND)

    include_directories(
            include
            ./src
            ${Boost_INCLUDE_DIRS}
            )
    link_directories("${Boost_LIBRARY_DIRS}")

#
#    add_subdirectory(utests)
#
    
endif()
