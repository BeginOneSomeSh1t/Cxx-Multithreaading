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

void ProcessData(std::array<int, DATASET_SIZE>& arr, int& sum, std::mutex& mtx)
{
    for(int x : arr)
    {
        // lock the mutex, not to allow other guys to modify the value
        std::lock_guard g{mtx};
        
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
    
    std::mutex mtx;
    int sum = 0;
    
    
    // Requires a good bit of a cpu power
    for(auto& arr : datasets)
    {
        threads.push_back(std::thread{ProcessData, std::ref(arr), std::ref(sum), std::ref(mtx)}); // wrapp paramter to transfer the arr across threading dimensions)))
    }
    // Make sure all the slaves have done their work
    for(auto& s : threads)
    {
        s.join();
    }
    const float t = timer.Peek();
    std::cout << "Result is " << sum << std::endl;
    std::cout << "Time taken: " << t << std::endl;
    return 0;
}
