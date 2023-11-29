#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <limits>
#include "include/MyTimer.h"
#include <thread>
#include <span>
#include <algorithm>


// experimental settings
constexpr size_t WORKER_COUNT = 4;
constexpr size_t CHUNK_SIZE = 1000;
constexpr size_t CHUNK_COUNT = 100;
constexpr size_t SUBSET_SIZE = CHUNK_SIZE / WORKER_COUNT;
constexpr size_t LIGHT_ITERATIONS = 100;
constexpr size_t HEAVY_ITERATIONS = 1'000;
constexpr double PROBABILITY_HEAVY = .02;


// ensnure the chunk size is a multiple of 4
static_assert(CHUNK_SIZE >= WORKER_COUNT, "CHUNK_SIZE must be greater than or equal to WORKER_COUNT");
static_assert(CHUNK_SIZE % WORKER_COUNT == 0);

struct task
{
    unsigned int val;
    bool heavy;

    [[nodiscard]] unsigned int process() const
    {
        const auto iterations = heavy ? HEAVY_ITERATIONS : LIGHT_ITERATIONS;
        double intermediate =2 * (static_cast<double>(val) / static_cast<double>(std::numeric_limits<unsigned int>::max())) - 1.;
        for(size_t i = 0; i < iterations; i++)
        {
            intermediate += std::sin(std::cos(intermediate));
        }
        return static_cast<unsigned int>((1. + intermediate) / 2. * static_cast<double>(std::numeric_limits<unsigned int>::max()));
    }
    
};

std::vector<std::array<task, CHUNK_SIZE>> generate_data_sets()
{
    std::minstd_rand rne;
    std::bernoulli_distribution bernouili_dist{ PROBABILITY_HEAVY };
    std::vector<std::array<task, CHUNK_SIZE>> chunks(CHUNK_COUNT);

    // fill in the data set
    for(auto& chunk : chunks)
    {
        // Fills each array with random numbers
        std::ranges::generate(chunk, [&]{return task{ .val = rne(), .heavy = bernouili_dist(rne) };});
        
    }

    return chunks;
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

    void set_job(std::span<const task> dataset)
    {
        {
            std::lock_guard lk{mtx_};
            input_ = dataset;
            // Reset the accumulation every time a job is set
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
    unsigned int get_result() const
    {
        return accumulation_;
    }
private:
    void process_data_()
    {
        for(const auto& t : input_)
        {
            accumulation_ += t.process();
        } 
    }
    void run_()
    {
        std::unique_lock lk{mtx_};
        while(true)
        {
            cv_.wait(lk, [this]{return !input_.empty() || b_dying; });

            if(b_dying)
                break;
            
            process_data_();
            
            input_ = {};
            sp_mctrl_->signal_done();
        }
    }
    
    std::shared_ptr<master_control> sp_mctrl_;
    std::jthread thread_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
    // shared memoru
    std::span<const task> input_;
    unsigned int accumulation_ = 0;
    bool b_dying = false;
};

/*
 * To conduct the experiment we divide one data set in chunks and allow different threads to process each own chunk and then to go to another data set
 */
int do_experiment()
{
    const auto chunks = generate_data_sets();
    
    MyTimer timer;
    timer.Mark();

   auto sp_mctrl = std::make_shared<master_control>();
    
    if(!sp_mctrl)
        throw std::exception("Failed to create master_control");

    std::vector<std::unique_ptr<worker>> p_workers;
    for(size_t j = 0; j < WORKER_COUNT; j++)
    {
        p_workers.push_back(std::make_unique<worker>(sp_mctrl));
    }

    
    for(const auto& chunk : chunks)
    {
        for(size_t i_subs = 0; i_subs < WORKER_COUNT; i_subs++)
        {
            p_workers[i_subs]->set_job(std::span{&chunk[i_subs * SUBSET_SIZE], SUBSET_SIZE});
        }
        sp_mctrl->wait_for_all_done();
    }

    const float t = timer.Peek();
    
    // Accumlate the overall result.
    // !! SIDE NOTE - I know there is a thing called std::accumulate but I thought up to this point they included it in ranges but they didn't, so I just said fuck you I am not gonna write this shit.being shit.end again!
    unsigned int final_result = 0;
    for(const auto& w : p_workers)
    {
        final_result += w->get_result();
    }
    std::cout << "Result is " <<  final_result << std::endl;
    std::cout << "Time taken: " << t << std::endl;

    // Kill the workers
    for(auto& w : p_workers)
    {
        w->kill();
    }
    
    getchar();

    return 0;
}

int main(int argc, char** argv)
{
    try
    {
        return do_experiment();
    }
    catch (...)
    {
        std::cout << "Somethin went wrong!";
    }
    
}