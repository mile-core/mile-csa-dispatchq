__**Mile Dispatch Queue is a multithreaded task dispatcher of queues**

The lib is developed to optimize application support for systems with multi-core processors and other symmetric multiprocessing systems.
It is an implementation of task parallelism based on the thread pool pattern. The fundamental idea is to move the management 
of the thread pool out of the hands of the developer, and closer to the operating system. The developer injects "work packages" into the pool oblivious of the pool's architecture. 
This model improves simplicity, portability and performance.

*This version is a naive implementation of Apple GCD develops to study event-driven model of p2p networks are used in blockchain core applications*

---

## Code example

```c++
       auto main_thread_id = std::this_thread::get_id();
   
       for (int i = 0; i < 100 ; ++i) {
           dispatch::Queue::main()->async([main_thread_id,i]{
               BOOST_CHECK_EQUAL(std::this_thread::get_id(), main_thread_id );
               if (i==99) {
                   dispatch::main::exit();
              }
           });
       }
   
       dispatch::main::loop([main_thread_id]{
           BOOST_CHECK_EQUAL(std::this_thread::get_id(), main_thread_id );
           sleep(1);
       });        
 ```