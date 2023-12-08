#pragma once
#include <iostream>
#include <thread>
#include <mutex>
#include <span>
#include <format>
#include "Constants.h"
#include "Task.h"
#include "Timing.h"
#include "../include/MyTimer.h"
#include "Logging.h"

namespace pre
{
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
                LOG(LogMasterControl, Info, "Work completed");
                ++done_count_;

                if(done_count_ == WORKER_COUNT)
                {
                    LOG(LogMasterControl, Info, "All work is done");
                    needs_notification = true;
                }
            }
            if(needs_notification)
                cv_.notify_one();
        }
    
        void wait_for_all_done()
        {
            LOG(LogMasterControl, Info, "Waiting for all other to be done.....");
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
        {
        }

        void set_job(std::span<const Task> dataset)
        {
            {
                std::lock_guard lk{mtx_};
                LOG(LogWorker, Info, "Setting job for Worker..");
                input_ = dataset;
                // Reset the accumulation every time a job is set
            }
            cv_.notify_one();
        }

        void kill()
        {
            {
                std::lock_guard lk{mtx_};
                LOG(LogWorker, Info, "Killing Worker...");
                b_dying = true;
            }
            cv_.notify_one();
        }

        unsigned int get_result() const
        {
            return accumulation_;
        }

        float get_job_work_time() const
        {
            return work_time_;
        }

        size_t get_num_heavy_items_processed() const
        {
            return num_heavy_items_processed;
        }

        ~worker()
        {
            kill();
        }

    private:
        void process_data_()
        {
            num_heavy_items_processed = 0;

            LOG(LogWorker, Info, "Process data for Worker");
            for (const auto& t : input_)
            {
                accumulation_ += t.process();
                num_heavy_items_processed += t._b_heavy ? 1 : 0;
            }
            LOG(LogWorker, Info, "Processed data: {} for Worker", accumulation_);
        }

        void run_()
        {
            std::unique_lock lk{mtx_};
            while (true)
            {
                MyTimer timer;
                cv_.wait(lk, [this] { return !input_.empty() || b_dying; });

                if (b_dying)
                    break;

                timer.Mark();

                process_data_();

                work_time_ = timer.Peek();

                input_ = {};
                sp_mctrl_->signal_done();
            }
        }

        std::shared_ptr<master_control> sp_mctrl_;
        std::jthread thread_;
        std::condition_variable cv_;
        std::mutex mtx_;

        // shared memoru
        std::span<const Task> input_;
        unsigned int accumulation_ = 0;
        bool b_dying = false;
        float work_time_ = -1.f;
        size_t num_heavy_items_processed = 0;
    };


    int do_experiment(Dataset chunks)
    {
        LOG(LogTemp, Info, "Starting experiment");
            
        MyTimer total_timer;
        total_timer.Mark();

       auto sp_mctrl = std::make_shared<master_control>();
        
        if(!sp_mctrl)
            throw std::exception("Failed to create MasterControl");

        LOG(LogTemp, Info, "Allocate p_workers");
        
        std::vector<std::unique_ptr<worker>> p_workers;
        for(size_t j = 0; j < WORKER_COUNT; j++)
        {
            p_workers.push_back(std::make_unique<worker>(sp_mctrl));
        }

        std::vector<chunk_timing_info> timings;
        timings.reserve(CHUNK_COUNT);
        
        MyTimer chunk_timer;
        for(const auto& chunk : chunks)
        {
            chunk_timer.Mark();
            for(size_t i_subs = 0; i_subs < WORKER_COUNT; i_subs++)
            {
                p_workers[i_subs]->set_job(std::span{&chunk[i_subs * SUBSET_SIZE], SUBSET_SIZE});
            }
            sp_mctrl->wait_for_all_done();
            
            // Report timing for threads
            const auto chunk_time = chunk_timer.Peek();
            timings.push_back
            (
              {}  
            );
            for(size_t i = 0; i < WORKER_COUNT; i++)
            {
                timings.back().number_of_heavy_items_per_thread[i] = p_workers[i]->get_num_heavy_items_processed();
                timings.back().time_spent_working_per_thread[i] = p_workers[i]->get_job_work_time();
                timings.back().total_chunk_time = chunk_time;
            }
        }

        const float t = total_timer.Peek();
        
        // Accumlate the overall result.
        // !! SIDE NOTE - I know there is a thing called std::accumulate but I thought up to this point they included it in ranges but they didn't, so I just said fuck you I am not gonna write this shit.being shit.end again!
        unsigned int final_result = 0;
        LOG_ALWAYS(LogTemp, Info, "Accumulating final result");
        for(const auto& w : p_workers)
        {
            final_result += w->get_result();
        }
        LOG_ALWAYS(LogTemp, Info, "Result is {}\n Time taken: {}", final_result, t);

        
        // Output csv of chunk timings
        // worktime, idletime, numberofheavies x workers = totaltime, total heavies

        if constexpr (CHUNK_MEASUREMENT_ENABLED)
        {
            write_csv(timings);
        }

        
        getchar();

        return 0;
    }
}
