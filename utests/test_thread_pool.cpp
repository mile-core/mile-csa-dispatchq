//
// Created by lotus mile on 30/10/2018.
//


#include "milecsa_queue.h"

#define BOOST_TEST_THREAD_SAFE 1
#define BOOST_TEST_MODULE thread_pool

#include <string>
#include <streambuf>
#include <strstream>
#include <thread>
#include <boost/test/included/unit_test.hpp>
#include <mutex>

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


using namespace dispatch;

BOOST_AUTO_TEST_CASE( test_init_thread_pool )
{
    auto main_thread_id = std::this_thread::get_id();

    auto q99 = Queue();
    auto q10 = Queue();

    int w = 10;
    for (int j = 0; j < w ; ++j) {
        q10.async([=,&q10]{
            std::cerr << "\n=Q10-[" << j << "] " << q99.is_default() << ", " << q99.get_id()<< std::endl << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
    }

    for (int j = 0; j < 100 ; ++j) {
        q99.async([=,&q99]{
            std::cerr << "\n#Q99-[" << j << "] " << q99.is_default() << ", " << q99.get_id() << std::endl << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }


    for (int j = 0; j < 100 ; ++j) {
        Default::async([=]{
            std::cerr << "\n@A00-[" << j << "] " << main_thread_id << ", " << Queue::get_default()->is_default() << ", " << Queue::get_default()->get_id() << std::endl << std::flush;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (j>20) {
                Default::loop::exit();
            }

        });
    }


    Default::loop::run();

    std::cerr << "Exited ......" << std::endl;

    q10.wait();
    q99.wait();

    exit(0);
}
