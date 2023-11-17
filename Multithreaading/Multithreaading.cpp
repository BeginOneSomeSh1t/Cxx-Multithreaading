#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <limits>
#include "include/MyTimer.h"

constexpr size_t DATASET_SIZE = 5'000'000;

int main()
{
    // Define random engine and data set
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{4};

    MyTimer timer;
    timer.Mark();
    
    // fill in the data set
    for(auto& arr : datasets)
    {
        // Fills each array with random numbers
        std::ranges::generate(arr, rne);
        
    }
    
    // Requires a good bit of a cpu power
    for(auto& arr : datasets)
    {
        for(int x : arr)
        {
            constexpr auto limit = static_cast<double>(std::numeric_limits<int>::max());
            const auto y = static_cast<double>(x) / limit;
            arr[0] += static_cast<int>(std::sin(std::cos(y)) * limit);
        }
    }
    
    const float t = timer.Peek();
    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    return 0;
}
