//
// Created by lotus mile  on 16.02.2018.
//

#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <thread>

/**
 * Multithreaded task dispatcher space
 * */
namespace dispatch {

    struct Queue;

    typedef std::function<void()> function;

    /**
     * Task queue
     * */
    struct Queue
    {
        typedef long Priority;

        /**
         * Task priority
         * */
        const Priority priority;

        /**
         * Create task queue with background priority
         * */
        Queue();

        /**
         * Create task queue with certain priority
         * */
        Queue(Queue::Priority priority);

        /**
         * Access to main thread task queue
         * */
        static std::shared_ptr<Queue> main();

        /**
         * Launch new function asynchronously
         *
         * */
        virtual void async(function) const;

        /**
         * Cancel task
         */
        void cancel();

        /**
         * Get the current thread id
         * */
        const std::thread::id getId() const ;

        /**
         * Test the current thread is in the main thread
         * */
        const bool isMain() const ;

    };

    /**
     * Default priorities
     *
     * */
    namespace queue { namespace priority {
            /**
             * High queue priority
             * */
            Queue::Priority const high       = 255;

            /**
             * Normal queue priority
             * */
            Queue::Priority const normal     = 0;

            /**
             * Low queue priority
             * */
            Queue::Priority const low        = (-2);

            /**
             * Lowest queue priority
             * */
            Queue::Priority const background = (-255);
        };
    };

    /**
     * Main thread tasks utilities
     *
     * */
    namespace  main {

        /**
         *
         * Run main loop function
         *
         * */
        void loop(dispatch::function main_loop_function);

        /**
         *
         * Exit from main loop
         *
         * */
        void exit();

        /**
         *
         * Set main loop callback
         *
         * */
        void set_loop_callback(dispatch::function callback);
    }
}
