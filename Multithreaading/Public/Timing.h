#pragma once
#include <array>
#include <span>
#include <format>
#include <fstream>
#include "Constants.h"
#include <iostream>
#include "Logging.h"


struct chunk_timing_info
{
    std::array<float, WORKER_COUNT> time_spent_working_per_thread;
    std::array<size_t, WORKER_COUNT> number_of_heavy_items_per_thread;
    float total_chunk_time;
};

inline void write_csv(const std::span<const chunk_timing_info> timings)
{
    // Create a file
    std::ofstream csv{ "timings.csv", std::ios_base::trunc };
    
    LOG(LogTemp, Info, "Start outputing csv file");
    
    for (size_t i = 0; i < WORKER_COUNT; i++)
    {
        csv << std::format(" work_{0:}, idle_{0:}, heavy_{0:},", i);
    }

    csv << "chunk_time, totalidle, total_heavy\n";

    for(const auto& chunk : timings)
    {
        float total_idle {0.f};
        size_t total_heavy {0};
        for (size_t i = 0; i < WORKER_COUNT; i++)
        {
            const auto idle {chunk.total_chunk_time - chunk.time_spent_working_per_thread[i]};
            const auto heavy {chunk.number_of_heavy_items_per_thread[i]};
            
            csv << std::format("{}, {}, ", chunk.time_spent_working_per_thread[i], idle);
            csv << std::format("{},", heavy);

            total_idle += idle;
            total_heavy += heavy;
        }
        
        csv << std::format("{}, {}, {}\n", chunk.total_chunk_time, total_idle, total_heavy);
    }
}
