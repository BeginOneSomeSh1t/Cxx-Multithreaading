#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <limits>
#include "include/MyTimer.h"
#include <thread>
#include <span>

constexpr size_t DATASET_SIZE = 50'000'000;

void ProcessData(std::span<int> arr, int& sum)
{
    for(auto x : arr)
    {
        // lock the mutex, not to allow other guys to modify the value
        constexpr auto limit = static_cast<double>(std::numeric_limits<int>::max());
        const auto y = static_cast<double>(x) / limit;
        sum += static_cast<int>(std::sin(std::cos(y)) * limit);
        
       
    } 
}

std::vector<std::array<int, DATASET_SIZE>> GenerateDataSets()
{
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{4};

    // fill in the data set
    for(auto& arr : datasets)
    {
        // Fills each array with random numbers
        std::ranges::generate(arr, rne);
        
    }

    return datasets;
}

int DoOneBiggie()
{
    // Define random engine and data set
    auto datasets = GenerateDataSets();
    std::vector<std::thread> threads;
    
    MyTimer timer;
    timer.Mark();
    
  
    struct Value
    {
        int v = 0;
        char padding[60];
    };
    Value sum[4];
    
    
    // Requires a good bit of a cpu power
    for(size_t i = 0; i < 4; i++)
    {
        threads.emplace_back(std::thread{ProcessData, std::span{datasets[i]}, std::ref(sum[i].v)}); // wrapp paramter to transfer the arr across threading dimensions)))
    }

    
    // Make sure all the slaves have done their work
    for(auto& s : threads)
    {
        s.join();
    }
    const float t = timer.Peek();
    std::cout << "Result is " << (sum[0].v + sum[1].v + sum[2].v + sum[3].v) << std::endl;
    std::cout << "Time taken: " << t << std::endl;
    getchar();
    return 0;
}

// Interface for the main thread
class MasterControl
{
public:
    MasterControl(int workers_count)
        :
    workers_count_{workers_count},
    lk_{mtx_},
    done_count_{0}
    {}
    
    void SignalDone()
    {
        {
            std::lock_guard lk{mtx_};
            ++done_count_;
        }

        if(done_count_ == workers_count_)
            cv_.notify_one();
    }
    
    void WaitForAllDone()
    {
        cv_.wait(lk_, [this]{return done_count_ == workers_count_;});

        done_count_ = 0;
    }
    
private:
    std::condition_variable cv_;
    std::mutex mtx_;
    std::unique_lock<std::mutex> lk_;
    int workers_count_;
    
    // shared memory
    int done_count_;
};


// Interface for a seaprate thread (joins automatically)
class Worker
{
public:
    Worker(const std::shared_ptr<MasterControl>& sp_mctrl)
        :
    sp_mctrl_{sp_mctrl},
    thread_{&Worker::Run_, this}
    {}

    void SetJob(std::span<int> dataset, int* p_output)
    {
        {
            std::lock_guard lk{mtx_};
            input_ = dataset;
            p_output_ = p_output;
        }
        cv_.notify_one();
    }
    void Kill()
    {
        {
            std::lock_guard lk{mtx_};
            b_dying = true;
        }
        cv_.notify_one();
    }
private:
    void Run_()
    {
        std::unique_lock lk{mtx_};
        while(true)
        {
            cv_.wait(lk, [this]{return p_output_ || b_dying; });

            if(b_dying)
                break;

            ProcessData(input_, *p_output_);

            p_output_ = nullptr;
            input_ = {};
            sp_mctrl_->SignalDone();
        }
    }
    
    std::shared_ptr<MasterControl> sp_mctrl_;
    std::jthread thread_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
    std::span<int> input_;
    int* p_output_ = nullptr;
    bool b_dying = false;
};

int DoSmallies()
{
    auto datasets = GenerateDataSets();
  
    struct Value
    {
        int v = 0;
        char padding[60];
    };
    Value sum[4];
    
    MyTimer timer;
    timer.Mark();

    constexpr int workers_count{4};
    auto sp_mctrl {std::make_shared<MasterControl>(workers_count)};
    std::vector<std::unique_ptr<Worker>> p_workers;
    for(size_t j = 0; j < workers_count; j++)
    {
        p_workers.push_back(std::make_unique<Worker>(sp_mctrl));
    }
    
    constexpr auto subset_size = DATASET_SIZE / 10'000;
    for(size_t i = 0;  i < DATASET_SIZE; i += subset_size)
    {
        for(size_t j = 0; j < 4; j++)
        {
            p_workers[j]->SetJob(std::span{&datasets[j][i], subset_size}, &sum[j].v);
        }
        sp_mctrl->WaitForAllDone();
    }

    const float t = timer.Peek();
    std::cout << "Result is " <<  sum[0].v + sum[1].v + sum[2].v + sum[3].v << std::endl;
    std::cout << "Time taken: " << t << std::endl;

    for(auto& w : p_workers)
        w->Kill();
    
    getchar();
    return 0;
}


int main(int argc, char** argv)
{
    try
    {
        if(argc > 1 && std::string{argv[1]} == "smol")
            return DoSmallies();
        
        return DoOneBiggie();
    }
    catch (...)
    {
        std::cout << "Somethin went wrong!";
    }
    
}