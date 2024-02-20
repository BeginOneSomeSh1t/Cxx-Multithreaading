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
            if(auto It = std::ranges::find_if(pWorkers, [](const auto& pWorker) -> bool {return pWorker->IsBusy();});
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
                while(true)
                {
                    CVar.wait(Lk, InStopToken, [this]{return !bBusy;});

                    // If someone requested stop, bail out
                    if(InStopToken.stop_requested())
                        return;

                    // Execute the task
                    Task();

                    // Empty the task
                    Task = {};

                    bBusy = false;
                }
            }

            // data
            std::jthread Thread;
            std::atomic<bool> bBusy = false;
            std::condition_variable_any CVar;

            // Using mutex only for the condition variable
            std::mutex Mtx;
            Task<void()> Task;
        };
        
        std::vector<std::unique_ptr<Worker>> pWorkers;
    };

    
}


int main(int argc, char** argv)
{
    using namespace popl;

    // define and parle cli options
    OptionParser op("Allowed options");
    auto stacked = op.add<Switch>("", "stacked", "Generate a stacked Dataset");
    auto even = op.add<Switch>("", "even", "Generate an even Dataset");
    auto queued = op.add<Switch>("", "queued", "Use queued approach");
    auto atomic = op.add<Switch>("", "atomic-queued", "Used for atomic queued approach");

    op.parse(argc, argv);
    
    // Determine the data type form the command args
    DatasetType data_type;
    if(stacked->is_set())
    {
        data_type = DatasetType::stacked;
    }
    else if(even->is_set())
    {
        data_type = DatasetType::evenly;
    }
    else
    {
        data_type = DatasetType::random;
    }

    Dataset data = generate_data_sets_by_type(data_type);
    
    // Run experiment
    if(queued->is_set())
    {
        LOG_ALWAYS(LogTemp, Info, "Start queued experiment");
        return que::do_Experiment(std::move(data));
    }
    else if(atomic->is_set())
    {
        LOG_ALWAYS(LogTemp, Info, "Start atomic-queued experiment");
        return atq::do_Experiment(std::move(data));
    }
    else
    {
        LOG_ALWAYS(LogTemp, Info, "Start random experiment");
        return pre::do_experiment(std::move(data));
    }
    
}