#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <limits>
#include "include/MyTimer.h"
#include <thread>
#include <span>

// experimental settings
constexpr size_t WORKER_COUNT = 4;
constexpr size_t CHUNK_SIZE = 100;
constexpr size_t CHUNK_COUNT = 100;
constexpr size_t LIGHT_ITERATIONS = 100;
constexpr size_t HEAVY_ITERATIONS = 1'000;
constexpr double PROBABILITY_HEAVY = .02;


// ensnure the chunk size is a multiple of 4
static_assert(CHUNK_SIZE >= WORKER_COUNT, "CHUNK_SIZE must be greater than or equal to WORKER_COUNT");
static_assert(CHUNK_SIZE % WORKER_COUNT == 0);

struct task
{
    double val;
    bool heavy;

    [[nodiscard]] double process() const
    {
        const auto iterations = heavy ? HEAVY_ITERATIONS : LIGHT_ITERATIONS;
        double result = val;
        for(size_t i = 0; i < iterations; i++)
        {
            result += std::sin(std::cos(val));
        }
        return result;
    }
    
};

void process_data(std::span<int> arr, int& sum)
{
    for(auto x : arr)
    {
        // lock the mutex, not to allow other guys to modify the value
        constexpr auto limit = static_cast<double>(std::numeric_limits<int>::max());
        const auto y = static_cast<double>(x) / limit;
        sum += static_cast<int>(std::sin(std::cos(y)) * limit);
        
    } 
}

std::vector<std::array<task, CHUNK_SIZE>> generate_data_sets()
{
    std::minstd_rand rne;
    std::uniform_real_distribution dist{-1., 1.};
    std::bernoulli_distribution bernouili_dist{ PROBABILITY_HEAVY };
    std::vector<std::array<task, CHUNK_SIZE>> chunks(CHUNK_COUNT);

    // fill in the data set
    for(auto& chunk : chunks)
    {
        // Fills each array with random numbers
        std::ranges::generate(chunk, [&]{return task{ .val = dist(rne), .heavy = bernouili_dist(rne) };});
        
    }

    return chunks;
}

int do_one_biggie()
{
    // Define random engine and data set
    auto datasets = generate_data_sets();
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
        threads.emplace_back(std::thread{process_data, std::span{datasets[i]}, std::ref(sum[i].v)}); // wrapp paramter to transfer the arr across threading dimensions)))
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
class master_control
{
public:
    master_control()
        :
        lk_{mtx_},
        done_count_{0}
    {}
    
    void signal_done()
    {
        bool needs_notification = false;
        {
            std::lock_guard lk{mtx_};
            ++done_count_;

            if(done_count_ == WORKER_COUNT)
            {
                needs_notification = true;
            }
        }
        if(needs_notification)
            cv_.notify_one();
    }
    
    void wait_for_all_done()
    {
        cv_.wait(lk_, [this]{return done_count_ == WORKER_COUNT;});

        done_count_ = 0;
    }
    
private:
    std::condition_variable cv_;
    std::mutex mtx_;
    std::unique_lock<std::mutex> lk_;
    
    // shared memory
    int done_count_;
};


// Interface for a seaprate thread (joins automatically)
class worker
{
public:
    worker(const std::shared_ptr<master_control>& sp_mctrl)
        :
    sp_mctrl_{sp_mctrl},
    thread_{&worker::run_, this}
    {}

    void set_job(std::span<int> dataset, int* p_output)
    {
        {
            std::lock_guard lk{mtx_};
            input_ = dataset;
            p_output_ = p_output;
        }
        cv_.notify_one();
    }
    void kill()
    {
        {
            std::lock_guard lk{mtx_};
            b_dying = true;
        }
        cv_.notify_one();
    }
private:
    void run_()
    {
        std::unique_lock lk{mtx_};
        while(true)
        {
            cv_.wait(lk, [this]{return p_output_ || b_dying; });

            if(b_dying)
                break;

            process_data(input_, *p_output_);

            p_output_ = nullptr;
            input_ = {};
            sp_mctrl_->signal_done();
        }
    }
    
    std::shared_ptr<master_control> sp_mctrl_;
    std::jthread thread_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
    std::span<int> input_;
    int* p_output_ = nullptr;
    bool b_dying = false;
};

int do_smallies()
{
    auto datasets = generate_data_sets();
  
    struct Value
    {
        int v = 0;
        char padding[60];
    };
    Value sum[4];
    
    MyTimer timer;
    timer.Mark();

    constexpr int workers_count{4};
    auto sp_mctrl {std::make_shared<master_control>(workers_count)};
    std::vector<std::unique_ptr<worker>> p_workers;
    for(size_t j = 0; j < workers_count; j++)
    {
        p_workers.push_back(std::make_unique<worker>(sp_mctrl));
    }
    
    constexpr auto subset_size = DATASET_SIZE / 10'000;
    for(size_t i = 0;  i < DATASET_SIZE; i += subset_size)
    {
        for(size_t j = 0; j < 4; j++)
        {
            p_workers[j]->set_job(std::span{&datasets[j][i], subset_size}, &sum[j].v);
        }
        sp_mctrl->wait_for_all_done();
    }

    const float t = timer.Peek();
    std::cout << "Result is " <<  sum[0].v + sum[1].v + sum[2].v + sum[3].v << std::endl;
    std::cout << "Time taken: " << t << std::endl;

    for(auto& w : p_workers)
        w->kill();
    
    getchar();
    return 0;
}


int main(int argc, char** argv)
{
    try
    {
        if(argc > 1 && std::string{argv[1]} == "smol")
            return do_smallies();
        
        return do_one_biggie();
    }
    catch (...)
    {
        std::cout << "Somethin went wrong!";
    }
    
}