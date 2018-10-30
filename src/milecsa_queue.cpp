//
// Created by lotus mile on 30/10/2018.
//

#include "milecsa_queue.h"

using namespace dispatch;


struct _DefaultQueue :  Queue
{
    _DefaultQueue(): Queue(1), is_exited(false) {};
    virtual const bool is_default() const override { return true; };
    bool is_exited;
};

std::shared_ptr<Queue> Queue::get_default()
{
    static std::once_flag flag;
    static std::shared_ptr<_DefaultQueue> _queue;
    std::call_once(flag, []
    {
        _queue = std::make_shared<_DefaultQueue>();
    });
    return std::static_pointer_cast<Queue>(_queue);
}

Queue::Queue(size_t size)
        :pool_(new dispatch::base_thread_pool(size)),
         queue_(std::make_shared<queued_tasks>()) {
}

void Queue::async(dispatch::function task) const {
    queue_->push_back(pool_->submit(task));
};

void Queue::wait() {
    queue_->clear();
    pool_->stop();
}

void Queue::stop() {
    pool_->exit();
}

const std::thread::id Queue::get_id() const {
    return std::this_thread::get_id();
}

const  bool  Queue::is_default() const {
    return false;
}

bool Queue::is_running() const {
    return pool_->is_running() && queue_->size()>0;
}

namespace dispatch {
    namespace Default {

        void async(dispatch::function task) {
            Queue::get_default()->async(task);
        }

        namespace loop {
            void exit()  {
                std::dynamic_pointer_cast<_DefaultQueue>(Queue::get_default())->is_exited = true;
            }

            void run(){
                while (!std::dynamic_pointer_cast<_DefaultQueue>(Queue::get_default())->is_exited){
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
    }
}
