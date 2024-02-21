#include <condition_variable>

#include "Public/AtomicQueued.h"
#include "Public/popl.hpp"
#include "Public/Preassigned.h"
#include "Public/Queued.h"
#include "Public/Task.h"
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
        void Run(Task<void()> InTask)
        {
            // Find one worker that isn't busy
            if(auto It = std::ranges::find_if(pWorkers, [](const auto& pWorker) -> bool {return !pWorker->IsBusy();});
                It != pWorkers.end())
            {
                (*It)->Run(std::move(InTask));
            }
            // Otherwise, add a new worker and run the task
            else
            {
                pWorkers.push_back
                (
                    std::make_unique<Worker>()
                );
                
                pWorkers.back()->Run(std::move(InTask));
            }
        }
        bool IsRunningTasks()
        {
            return std::ranges::any_of(pWorkers, [](const auto& pW){return pW->IsBusy();});
        }
    private:
        // data
        class Worker
        {
        public:

            Worker() : Thread(&Worker::RunKernel, this){}
            
            bool IsBusy() const
            {
                return bBusy;
            }
            void Run(Task<void()> InTask)
            {
                Task = std::move(InTask);
                bBusy = true;
                CVar.notify_one();
            }
        private:
            // functions
            void RunKernel()
            {
                std::unique_lock Lk{Mtx};

                auto InStopToken = Thread.get_stop_token();
                while(CVar.wait(Lk, InStopToken, [this]() -> bool {return bBusy;}))
                {
                    // Execute the task
                    Task();

                    // Empty the task
                    Task = {};

                    bBusy = false;
                }
            }

            // data
            std::atomic<bool> bBusy = false;
            std::condition_variable_any CVar;

            // Using mutex only for the condition variable
            std::mutex Mtx;
            Task<void()> Task;

            /*Should be below any parameters
             * because to avoid destruction of
             * those when the thread finishes
             */
            std::jthread Thread;
        };
        
        std::vector<std::unique_ptr<Worker>> pWorkers;
    };

    
}


int main(int argc, char** argv)
{
   tk::ThreadPool Pool;
    Pool.Run([]{std::cout << "Hello" << std::endl;});
    Pool.Run([]{std::cout << "World" << std::endl;});

    /*while(Pool.IsRunningTasks())
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(16ms);
    }*/
    
    return 0;    
}