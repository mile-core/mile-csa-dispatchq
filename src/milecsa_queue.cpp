//
// Created by lotus mile on 16.02.2018.
//

#include "../include/milecsa_queue.h"

#include <queue>
#include <thread>
#include <condition_variable>
#include <map>
#include <mutex>
#include <math.h>

namespace dispatch {

    /*
     *  Internal queue
     *
     * */
    struct QueueImpl
    {
        const Queue          *q;
        bool                  is_running;
        std::queue<function>  tasks;

        QueueImpl(const Queue *q): q(q){};
    };


    class SharedMainSingleton {
    public:

        static SharedMainSingleton& Instance() {
            static SharedMainSingleton singleton;
            return singleton;
        }

        std::thread::id get_main_id() {
            return main_id;
        }

    private:

        /*
         * Other non-static member functions
         * */

        std::thread::id main_id;
        SharedMainSingleton() {
            main_id = std::this_thread::get_id();
        }                                                // Private constructor
        ~SharedMainSingleton() {}
        SharedMainSingleton(const SharedMainSingleton&){};                 // Prevent copy-construction
        SharedMainSingleton& operator=(const SharedMainSingleton&){
            return *this;
        };
    };

    /*
     *  Internal thread pool
     *
     * */
    struct ThreadPool
    {
        typedef std::shared_ptr<QueueImpl> queue_ptr;

        ThreadPool();
        virtual ~ThreadPool();

        void push_task_with_priority(const dispatch::function&, const Queue *);

        bool get_free_queue(queue_ptr*) const;

        void start_task_in_queue(const queue_ptr&);

        void stop_task_in_queue(const queue_ptr&);

        void add_worker();

        bool is_stopped() { return stop_; };

        void stop();

        static std::shared_ptr<ThreadPool>& shared_pool();
        std::mutex mutex;
        std::map<Queue::Priority, queue_ptr> queues;

        std::mutex main_thread_mutex;
        std::queue<dispatch::function> main_queue;

        std::condition_variable condition;
        std::vector<std::thread> threads;

        dispatch::function main_loop_need_update;

    private:
        bool stop_;
    };

    bool ThreadPool::get_free_queue(queue_ptr* out_queue) const
    {
        auto finded = std::find_if(queues.rbegin(), queues.rend(), [](const std::pair<Queue::Priority, queue_ptr>& iterator)
        {
            return !iterator.second->is_running;
        });

        bool is_free_queue_exist = (finded != queues.rend());
        if (is_free_queue_exist)
            *out_queue = finded->second;

        return  is_free_queue_exist;
    }

    void ThreadPool::start_task_in_queue(const queue_ptr& queue)
    {
        queue->is_running = true;
    }

    void ThreadPool::push_task_with_priority(const dispatch::function& task, const Queue *q)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);

            auto queue = queues[q->priority];
            if (!queue) {
                queue = std::make_shared<QueueImpl>(q);
                queues[q->priority] = queue;
            }

            queue->tasks.push(task);

            unsigned max_number_of_threads = std::max<unsigned>(std::thread::hardware_concurrency(), 2);
            unsigned number_of_threads_required = round(log(queues.size()) + 1);
            while (threads.size() < std::min<unsigned>(max_number_of_threads, number_of_threads_required)) {
                add_worker();
            }
        }
        condition.notify_one();
    }

    void ThreadPool::stop_task_in_queue(const queue_ptr& queue)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);

            queue->is_running = false;
            if ( queue->tasks.size() ==0 )
            {
                queues.erase(queues.find(queue->q->priority));
            }
        }
        condition.notify_one();
    }

    void ThreadPool::stop() {
        stop_ = true;
    };

    void ThreadPool::add_worker()
    {
        threads.push_back(std::thread([=]
                                      {
                                          dispatch::function task;
                                          ThreadPool::queue_ptr queue;
                                          while(true)
                                          {
                                              {
                                                  std::unique_lock<std::mutex> lock(mutex);

                                                  while(!stop_ && !get_free_queue(&queue))
                                                      condition.wait(lock);

                                                  if(stop_)
                                                      return;

                                                  task = queue->tasks.front();
                                                  queue->tasks.pop();

                                                  start_task_in_queue(queue);
                                              }
                                              task();
                                              stop_task_in_queue(queue);
                                          }
                                      }));
    }

    ThreadPool::ThreadPool(){}

    ThreadPool::~ThreadPool()
    {
        stop_ = true;
        condition.notify_all();
        for (auto& thread: threads)
        {
            thread.join();
        }
    }

    std::shared_ptr<ThreadPool>& ThreadPool::shared_pool()
    {
        static std::once_flag flag;
        static std::shared_ptr<ThreadPool> shared_pool;
        std::call_once(flag, []
        {
            shared_pool = std::make_shared<ThreadPool>();
        });
        return shared_pool;
    }

    struct MainQueue : Queue
    {
        virtual void async(dispatch::function task) const override;
        MainQueue(): Queue(0) {};
    };

    void MainQueue::async(dispatch::function task) const
    {
        auto pool = ThreadPool::shared_pool();
        std::unique_lock<std::mutex> lock(pool->main_thread_mutex);
        pool->main_queue.push(task);
        if (pool->main_loop_need_update != nullptr)
            pool->main_loop_need_update();
    }

    std::shared_ptr<Queue> Queue::main()
    {
        return std::static_pointer_cast<dispatch::Queue>(std::make_shared<dispatch::MainQueue>());
    }

    /*
     *  Main Queue
     *
     * */
    namespace main {
        void process_loop(const Queue *q) {
            auto pool = ThreadPool::shared_pool();
            std::unique_lock<std::mutex> lock(pool->main_thread_mutex);
            while (!pool->main_queue.empty()) {
                auto task = pool->main_queue.front();
                pool->main_queue.pop();
                task();
            }
        }

        void loop(dispatch::function main_loop_function) {
            auto main_queue = Queue::main();
            while (!ThreadPool::shared_pool()->is_stopped()) {
                main_queue->async(main_loop_function);
                process_loop(main_queue.get());
            }
        }

        void exit() {
            ThreadPool::shared_pool()->stop();
        }

        void set_loop_callback(dispatch::function update_callback) {
            ThreadPool::shared_pool()->main_loop_need_update = update_callback;
        }
    }

    /*
     *  Queue implementation
     *
     * */
    Queue::Queue() : priority(-255) {};
    Queue::Queue(Queue::Priority priority) : priority(priority) {};

    void Queue::async(dispatch::function task) const {
        ThreadPool::shared_pool()->push_task_with_priority(task, this);
    };

    void Queue::cancel() {
        ThreadPool::shared_pool()->stop();
    };
    const std::thread::id Queue::getId() const {
        return std::this_thread::get_id();
    }

    const  bool  Queue::isMain() const {
        return getId() == SharedMainSingleton::Instance().get_main_id();
    }
};