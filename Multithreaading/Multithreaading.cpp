#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <limits>
#include "include/MyTimer.h"
#include <thread>
#include <mutex>

constexpr size_t DATASET_SIZE = 5'000'000;

void ProcessData(std::array<int, DATASET_SIZE>& arr, int& sum)
{
    for(int x : arr)
    {
        // lock the mutex, not to allow other guys to modify the value
        constexpr auto limit = static_cast<double>(std::numeric_limits<int>::max());
        const auto y = static_cast<double>(x) / limit;
        sum += static_cast<int>(std::sin(std::cos(y)) * limit);
        
       
    } 
}

int main()
{
    // Define random engine and data set
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{4};
    std::vector<std::thread> threads;
    
    MyTimer timer;
    timer.Mark();
    
    // fill in the data set
    for(auto& arr : datasets)
    {
        // Fills each array with random numbers
        std::ranges::generate(arr, rne);
        
    }
    struct Value
    {
        int v = 0;
        char padding[60];
    };
    Value sum[] = {0,0,0,0};
    
    
    // Requires a good bit of a cpu power
    for(size_t i = 0; i < 4; i++)
    {
        threads.emplace_back(std::thread{ProcessData, std::ref(datasets[i]), std::ref(sum[i].v)}); // wrapp paramter to transfer the arr across threading dimensions)))
    }

    
    // Make sure all the slaves have done their work
    for(auto& s : threads)
    {
        s.join();
    }
    const float t = timer.Peek();
    std::cout << "Result is " << sum[0].v + sum[1].v + sum[2].v + sum[3].v << std::endl;
    std::cout << "Time taken: " << t << std::endl;
    return 0;
}
