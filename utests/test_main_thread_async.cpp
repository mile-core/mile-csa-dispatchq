//
// Created by lotus mile on 16.02.2018.
//

#include "milecsa_queue.h"

#define BOOST_TEST_THREAD_SAFE 1
#define BOOST_TEST_MODULE main_thread_async

#include <string>
#include <streambuf>
#include <strstream>
#include <thread>
#include <boost/test/included/unit_test.hpp>

int random(int min, int max)
{
    static bool first = true;
    if (first)
    {
        srand( time(NULL) );
        first = false;
    }
    return min + rand() % (( max + 1 ) - min);
}

BOOST_AUTO_TEST_CASE( test_init )
{
    auto main_thread_id = std::this_thread::get_id();

    auto q1   =  dispatch::Queue(1);
    auto q100 =  dispatch::Queue(100);
    auto q10  =  dispatch::Queue(10);

    auto qs = {q1,q100,q10};

    for (auto it = qs.begin(); it!=qs.end(); it++) {
        it->async([it]{
            for (int i = 0; i < 100 ; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(random(100,1000)));
                std::cerr << "\nQ-"<<i<<"-[" << it->priority << "]" << std::endl << std::flush;
            }
        });
    }

    int w = 10;
    for (int j = 0; j < w ; ++j) {

        dispatch::Queue::main()->async([=]{

            std::this_thread::sleep_for(std::chrono::seconds(1));

            BOOST_CHECK_EQUAL(std::this_thread::get_id(), main_thread_id );
            std::cerr << "\nM-"<<j<<"-[" << dispatch::Queue::main()->priority << "]" << std::endl << std::flush;

            if (j==(w-1)) {
                std::cerr << "Exiting ......" << std::endl;
                dispatch::main::exit();
            }
        });
    }

    dispatch::main::loop([main_thread_id]{
        BOOST_CHECK_EQUAL(std::this_thread::get_id(), main_thread_id );
    });

    std::cerr << "Exited !" << std::endl;
}
