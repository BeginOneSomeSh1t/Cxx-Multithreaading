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

namespace que
{
    // Interface for the main thread
    class MasterControl
    {
    public:
        MasterControl()
            :
            _lk{_mtx},
            _done_count{0}
        {}
    
        void signal_Done()
        {
            bool needs_notification = false;
            {
                std::lock_guard lk{_mtx};
                LOG(LogMasterControl, Info, "Work completed");
                ++_done_count;

                if(_done_count == WORKER_COUNT)
                {
                    LOG(LogMasterControl, Info, "All work is done");
                    needs_notification = true;
                }
            }
            if(needs_notification)
                _cv.notify_one();
        }
    
        void wait_For_All_Done()
        {
            LOG(LogMasterControl, Info, "Waiting for all other to be done.....");
            _cv.wait(_lk, [this]{return _done_count == WORKER_COUNT;});

            _done_count = 0;
        }

        void set_Chunk(std::span<const Task> chunk)
        {
            _idx = 0;
            _current_chunk = chunk;
        }

        const Task* get_Task()
        {
            std::lock_guard lck{_mtx};
            const auto i = _idx++;
            
            if(i >= CHUNK_SIZE)
            {
                return nullptr;
            }
            
            return &_current_chunk[i];
        }
    private:
        std::condition_variable _cv;
        std::mutex _mtx;
        std::unique_lock<std::mutex> _lk;
        std::span<const Task> _current_chunk;
        // shared memory
        int _done_count;
        size_t _idx = 0;
    };

    // Interface for a seaprate thread (joins automatically)
    class Worker
    {
    public:
        Worker(const std::shared_ptr<MasterControl>& p_Mctrl)
            :
            _p_Mctrl{p_Mctrl},
            _thread{&Worker::_run, this}
        {
        }

        void start_Work()
        {
            {
                std::lock_guard lk{_mtx};
                _b_working = true;
            }
            _cv.notify_one();
            
        }
        void kill()
        {
            {
                std::lock_guard lk{_mtx};
                LOG(LogWorker, Info, "Killing Worker...");
                _b_dying = true;
            }
            
            _cv.notify_one();
        }

        unsigned int get_Result() const
        {
            return _accumulation;
        }

        float get_Job_Work_Time() const
        {
            return _work_time;
        }

        size_t get_Num_Heavy_Items_Processed() const
        {
            return _num_heavy_items_processed;
        }

        ~Worker()
        {
            kill();
        }

    private:
        void _process_Data()
        {
            _num_heavy_items_processed = 0;

            LOG(LogWorker, Info, "Process data for Worker");
            while(auto p_task = _p_Mctrl->get_Task())
            {
                _accumulation += p_task->process();
                _num_heavy_items_processed += p_task->_b_heavy ? 1 : 0;
            }
            
            LOG(LogWorker, Info, "Processed data: {} for Worker", _accumulation);
        }

        void _run()
        {
            std::unique_lock lk{_mtx};
            while (true)
            {
                MyTimer timer;
                _cv.wait(lk, [this] { return _b_working || _b_dying; });

                if (_b_dying)
                    break;

                timer.Mark();

                _process_Data();

                _work_time = timer.Peek();

                _b_working = false;
                _p_Mctrl->signal_Done();
            }
        }

        std::shared_ptr<MasterControl> _p_Mctrl;
        std::jthread _thread;
        std::condition_variable _cv;
        std::mutex _mtx;

        // shared memoru
        unsigned int _accumulation = 0;
        bool _b_dying = false;
        bool _b_working = false;
        float _work_time = -1.f;
        size_t _num_heavy_items_processed = 0;
    };


    int do_Experiment(Dataset chunks)
    {
        LOG(LogTemp, Info, "Starting experiment");
            
        MyTimer total_timer;
        total_timer.Mark();

       auto sp_mctrl = std::make_shared<MasterControl>();
        
        if(!sp_mctrl)
            throw std::exception("Failed to create MasterControl");

        LOG(LogTemp, Info, "Allocate p_workers");
        
        std::vector<std::unique_ptr<Worker>> p_workers;
        for(size_t j = 0; j < WORKER_COUNT; j++)
        {
            p_workers.push_back(std::make_unique<Worker>(sp_mctrl));
        }

        std::vector<chunk_timing_info> timings;
        timings.reserve(CHUNK_COUNT);
        
        MyTimer chunk_timer;
        for(const auto& chunk : chunks)
        {
            chunk_timer.Mark();
            sp_mctrl->set_Chunk(chunk);
            for(auto& p_worker : p_workers)
            {
                p_worker->start_Work();
            }
            sp_mctrl->wait_For_All_Done();
            
            // Report timing for threads
            const auto chunk_time = chunk_timer.Peek();
            timings.push_back
            (
              {}  
            );
            for(size_t i = 0; i < WORKER_COUNT; i++)
            {
                timings.back().number_of_heavy_items_per_thread[i] = p_workers[i]->get_Num_Heavy_Items_Processed();
                timings.back().time_spent_working_per_thread[i] = p_workers[i]->get_Job_Work_Time();
                timings.back().total_chunk_time = chunk_time;
            }
        }

        const float t = total_timer.Peek();
        
        // Accumlate the overall result.
        unsigned int final_result = 0;
        LOG_ALWAYS(LogTemp, Info, "Accumulating final result");
        for(const auto& w : p_workers)
        {
            final_result += w->get_Result();
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
