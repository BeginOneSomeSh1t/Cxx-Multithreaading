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
    
    std::vector<std::jthread> threads;
    
    constexpr auto subset_size = DATASET_SIZE / 10'000;
    for(size_t i = 0;  i < DATASET_SIZE; i += subset_size)
    {
        for(size_t j = 0; j < 4; j++)
        {
            threads.push_back(std::jthread{ProcessData, std::span{ &datasets[j][i], subset_size }, std::ref(sum[j].v)}); // wrapp paramter to transfer the arr across threading dimensions)))
        }
        threads.clear();
    }

    const float t = timer.Peek();
    std::cout << "Result is " <<  sum[0].v + sum[1].v + sum[2].v + sum[3].v << std::endl;
    std::cout << "Time taken: " << t << std::endl;
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