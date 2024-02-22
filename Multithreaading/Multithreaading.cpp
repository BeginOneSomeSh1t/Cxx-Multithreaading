#include <condition_variable>
#include <deque>
#include "Public/AtomicQueued.h"
#include "Public/popl.hpp"
#include "Public/Preassigned.h"
#include <sstream>
#include <functional>
#include <iostream>

template<typename... Args>
void print_anything(Args&&... args)
{
    (std::cout << ... << args) << std::endl;
}

namespace tk
{
    template<typename UserPolicy>
    using Task = std::function<UserPolicy>;

    // Thread pool is a single thread for now
    class ThreadPool
    {
    public:
        
        /**
         * Constructor for ThreadPool class.
         *
         * @param InWorkersCount the number of workers in the thread pool
         *
         * @return 
         *
         * @throws 
         */
        ThreadPool(std::size_t InWorkersCount)
        {
            Workers.reserve(InWorkersCount);
            for(size_t i = 0; i < InWorkersCount; i++)
            {
                Workers.emplace_back(this);
            }
        }
        
        /**
         * Run the given task by pushing it to the task queue and notifying the task queue thread.
         *
         * @param InTask the task to be run
         *
         * @return void
         *
         * @throws None
         */        
        void Run(Task<void()> InTask)
        {
            {
                std::lock_guard Lock{TaskQueueMutex};
                Tasks.push_back(std::move(InTask));
            }
            CVarQueueTask.notify_one();
        }
        
        /**
         * Waits for all tasks to be done.
         */        
        void WaitForAllDone()
        {
            std::unique_lock ULock{TaskQueueMutex};
            CVarAllDone.wait(ULock, [this]{return Tasks.empty();});
        }

        // Override destructor because the joined threads
        // block other threads from running
        ~ThreadPool()
        {
            for(auto& Worker : Workers)
            {
                Worker.RequestStop();
            }
        }
        
    private:
        // functions
        /**
        * GetTask function retrieves a task using the provided stop token.
        *
        * @param InStopToken reference to the stop token
        *
        * @return the retrieved task
        *
        * @throws N/A
        */
        Task<void()> GetTask(std::stop_token& InStopToken)
        {
            Task<void()> Task;
            std::unique_lock ULock{TaskQueueMutex};
            CVarQueueTask.wait(ULock, InStopToken, [this]{return !Tasks.empty();});

            // If stop token is not requested, get the task
            if(!InStopToken.stop_requested())
            {
                Task = std::move(Tasks.front());
                Tasks.pop_front();

                // Notify all when all the tasks are done
                if(Tasks.empty())
                {
                    CVarAllDone.notify_all();
                }
            }
            return Task;
            
        }
        
        // data
        class Worker
        {
        public:
            
            /**
             * Constructor for the Worker class.
             *
             * @param InPoolPtr pointer to the ThreadPool
             */
            Worker(ThreadPool* InPoolPtr) : pPool(InPoolPtr), Thread(std::bind_front(&Worker::RunKernel, this)){}

            void RequestStop()
            {
                Thread.request_stop();
            }
            
        private:
            // functions
            void RunKernel(std::stop_token InStopToken)
            {
                while(auto Task = pPool->GetTask(InStopToken))
                {
                    // Execute the task
                    Task();
                }
            }
            
            //data
            ThreadPool* pPool;
            std::jthread Thread;
        };

        std::mutex TaskQueueMutex;
        std::condition_variable_any CVarQueueTask;
        std::condition_variable CVarAllDone;
        std::deque<Task<void()>> Tasks;
        std::vector<Worker> Workers;
    };

    
}


int main(int argc, char** argv)
{
    using namespace std::chrono_literals;

    const auto Spit = []
    {
        std::this_thread::sleep_for(100ms);
        std::ostringstream Ss;
        Ss << std::this_thread::get_id();
        std::cout << std::format("<< {} >>\n", Ss.str()) << std::flush;
    };
    
   tk::ThreadPool Pool{4};
    
    for(int i = 0; i < 160; ++i)
        Pool.Run(Spit);

    Pool.WaitForAllDone();
    return 0;    
}