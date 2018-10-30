//
// Created by lotus mile  on 16.02.2018.
//

#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * Multithreaded task dispatcher space
 * */
namespace dispatch {

    struct Queue;

    template <typename T>
    class dispatch_queue
    {
    public:
        /**
         * Destructor.
         */
        ~dispatch_queue(void)
        {
            invalidate();
        }

        /**
         * Attempt to get the first value in the queue.
         * Returns true if a value was successfully written to the out parameter, false otherwise.
         */
        bool tryPop(T& out)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            if(m_queue.empty() || !m_valid)
            {
                return false;
            }
            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        /**
         * Get the first value in the queue.
         * Will block until a value is available unless clear is called or the instance is destructed.
         * Returns true if a value was successfully written to the out parameter, false otherwise.
         */
        bool waitPop(T& out)
        {
            std::unique_lock<std::mutex> lock{m_mutex};
            m_condition.wait(lock, [this]()
            {
                return !m_queue.empty() || !m_valid;
            });
            /*
             * Using the condition in the predicate ensures that spurious wakeups with a valid
             * but empty queue will not proceed, so only need to check for validity before proceeding.
             */
            if(!m_valid)
            {
                return false;
            }
            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        /**
         * Push a new value onto the queue.
         */
        void push(T value)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            m_queue.push(std::move(value));
            m_condition.notify_one();
        }

        /**
         * Check whether or not the queue is empty.
         */
        bool empty(void) const
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            return m_queue.empty();
        }

        /**
         * Clear all items from the queue.
         */
        void clear(void)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            while(!m_queue.empty())
            {
                m_queue.pop();
            }
            m_condition.notify_all();
        }

        /**
         * Invalidate the queue.
         * Used to ensure no conditions are being waited on in waitPop when
         * a thread or the application is trying to exit.
         * The queue is invalid after calling this method and it is an error
         * to continue using a queue after this method has been called.
         */
        void invalidate(void)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            m_valid = false;
            m_condition.notify_all();
        }

        /**
         * Returns whether or not this queue is valid.
         */
        bool isValid(void) const
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            return m_valid;
        }

    private:
        std::atomic_bool m_valid{true};
        mutable std::mutex m_mutex;
        std::queue<T> m_queue;
        std::condition_variable m_condition;
    };

    class base_thread_pool
    {
    private:
        class task_impl
        {
        public:
            task_impl(void) = default;
            virtual ~task_impl(void) = default;
            task_impl(const task_impl& rhs) = delete;
            task_impl& operator=(const task_impl& rhs) = delete;
            task_impl(task_impl&& other) = default;
            task_impl& operator=(task_impl&& other) = default;

            /**
             * Run the task.
             */
            virtual void execute() = 0;
        };

        template <typename Func>
        class thread_task: public task_impl
        {
        public:
            thread_task(Func&& func)
                    :m_func{std::move(func)}
            {
            }

            ~thread_task(void) override = default;
            thread_task(const thread_task& rhs) = delete;
            thread_task& operator=(const thread_task& rhs) = delete;
            thread_task(thread_task&& other) = default;
            thread_task& operator=(thread_task&& other) = default;

            /**
             * Run the task.
             */
            void execute() override
            {
                m_func();
            }

        private:
            Func m_func;
        };

    public:
        /**
         * A wrapper around a std::future that adds the behavior of futures returned from std::async.
         * Specifically, this object will block and wait for execution to finish before going out of scope.
         */
        template <typename T>
        class task_yield
        {
        public:
            task_yield(std::future<T>&& future)
                    :m_future{std::move(future)}
            {
            }

            task_yield(const task_yield& rhs) = delete;
            task_yield& operator=(const task_yield& rhs) = delete;
            task_yield(task_yield&& other) = default;
            task_yield& operator=(task_yield&& other) = default;
            ~task_yield(void)
            {
                if(m_future.valid())
                {
                    m_future.get();
                }
            }

            auto get(void)
            {
                return m_future.get();
            }


        private:
            std::future<T> m_future;
        };


        static std::shared_ptr<base_thread_pool::task_yield<void>>& shared_pool();

    public:
        /**
         * Constructor.
         */
        base_thread_pool(void)
                :base_thread_pool{std::max(std::thread::hardware_concurrency(), 2u) - 1u}
        {
            /*
             * Always create at least one thread.  If hardware_concurrency() returns 0,
             * subtracting one would turn it to UINT_MAX, so get the maximum of
             * hardware_concurrency() and 2 before subtracting 1.
             */
        }

        /**
         * Constructor.
         */
        explicit base_thread_pool(const std::uint32_t numThreads)
                :m_done{false},
                 m_workQueue{},
                 m_threads{}
        {
            try
            {
                for(std::uint32_t i = 0u; i < numThreads; ++i)
                {
                    m_threads.emplace_back(&base_thread_pool::worker, this);
                }
            }
            catch(...)
            {
                exit();
                throw;
            }
        }

        /**
         * Non-copyable.
         */
        base_thread_pool(const base_thread_pool& rhs) = delete;

        /**
         * Non-assignable.
         */
        base_thread_pool& operator=(const base_thread_pool& rhs) = delete;

        /**
         * Destructor.
         */
        ~base_thread_pool(void)
        {
            exit();
        }

        /**
         * Submit a job to be run by the thread pool.
         */
        template <typename Func, typename... Args>
        auto submit(Func&& func, Args&&... args)
        {
            auto boundTask = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
            using ResultType = std::result_of_t<decltype(boundTask)()>;
            using PackagedTask = std::packaged_task<ResultType()>;
            using TaskType = thread_task<PackagedTask>;

            PackagedTask task{std::move(boundTask)};
            task_yield<ResultType> result{task.get_future()};
            m_workQueue.push(std::make_unique<TaskType>(std::move(task)));
            return result;
        }

        /**
        * Invalidates the queue and joins all running threads.
        */
        void exit(void)
        {
            m_done = true;
            m_workQueue.invalidate();
            for(auto& thread : m_threads)
            {
                if(thread.joinable())
                {
                    thread.join();
                }
            }
            m_threads.clear();
        }

        void stop(void) {
            m_done = true;
        }

        bool is_running() const {
            return !m_done;
        }

    private:
        /**
         * Constantly running function each thread uses to acquire work items from the queue.
         */
        void worker(void)
        {
            while(!m_done)
            {
                std::unique_ptr<task_impl> pTask{nullptr};
                if(m_workQueue.waitPop(pTask))
                {
                    pTask->execute();
                }
            }
        }


    private:
        std::atomic_bool m_done;
        dispatch_queue<std::unique_ptr<task_impl>> m_workQueue;
        std::vector<std::thread> m_threads;
    };

    namespace DefaultThreadPool
    {
        /**
         * Get the default thread pool for the application.
         * This pool is created with std::thread::hardware_concurrency() - 1 threads.
         */
        inline std::shared_ptr<base_thread_pool>& getThreadPool(void)
        {
            static std::once_flag flag;
            static std::shared_ptr<base_thread_pool> shared_pool;
            std::call_once(flag, []
            {
                shared_pool = std::make_shared<base_thread_pool>();
            });
            return shared_pool;
        }

        /**
         * Submit a job to the default thread pool.
         */
        template <typename Func, typename... Args>
        inline auto submitJob(Func&& func, Args&&... args)
        {
            return getThreadPool()->submit(std::forward<Func>(func), std::forward<Args>(args)...);
        }
    }

    typedef std::function<void()> function;
    typedef std::vector<dispatch::base_thread_pool::task_yield<void>> queued_tasks;

    /**
     * Task queue
     * */
    struct Queue
    {
        /**
         * Create task queue with background priority
         * */
        Queue(size_t size = 1);

        /**
         * Access to main thread task queue
         * */
        static std::shared_ptr<Queue> get_default();

        /**
         * Launch new function asynchronously
         *
         * */
        virtual void async(function) const;

        /**
         * Cancel all tasks in queue
         */
        void stop();

        /**
        * Wait until all tasks finish
        */
        void wait();

        /**
         * Get the current thread id
         * */
        virtual const std::thread::id get_id() const ;

        /**
         * Test the current thread is in the default thread
         * */
        virtual const bool is_default() const ;

        /**
        * Test the current thread is active
        * */
        virtual bool is_running() const ;

    protected:
        std::shared_ptr<dispatch::base_thread_pool>  pool_;
        std::shared_ptr<queued_tasks>          queue_;
    };

    namespace Default {
        /**
         * Asnc task in default queue.
         *
         * @param task
         */
        void async(dispatch::function task);

        namespace loop {
            /**
             * Run default loop.
             */
            void run();

            /**
             * Stop default loop and join to main thread.
             */
            void exit();
        }

    }
}