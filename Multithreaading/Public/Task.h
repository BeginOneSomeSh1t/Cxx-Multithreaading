#pragma once
#include <random>
#include <array>
#include <ranges>
#include <cmath>
#include <numbers>
#include "Constants.h"
#include "Logging.h"

struct Task
{
    double val;
    bool _b_heavy;

    [[nodiscard]] unsigned int process() const
    {
        const auto iterations = _b_heavy ? HEAVY_ITERATIONS : LIGHT_ITERATIONS;
        double intermediate =2 * (static_cast<double>(val) / static_cast<double>(std::numeric_limits<unsigned int>::max())) - 1.;
        for(size_t i = 0; i < iterations; i++)
        {
            const auto digits =  unsigned int (std::abs(std::sin(std::cos(intermediate)) * 10'000'000)) % 100'000;
            intermediate = double(digits) / 10'000.;
        }
        return unsigned int{ static_cast<unsigned int>(std::exp(intermediate)) };
    }
    
};


enum class DatasetType
{
    random,
    evenly,
    stacked
};


using Dataset = std::vector<std::array<Task, CHUNK_SIZE>>;

Dataset generate_data_sets_random()
{
    std::minstd_rand rne;
    std::bernoulli_distribution bernouili_dist{ PROBABILITY_HEAVY };
    std::uniform_real_distribution r_dist{0., std::numbers::pi};
    std::vector<std::array<Task, CHUNK_SIZE>> chunks(CHUNK_COUNT);

    // fill in the data set
    for(auto& chunk : chunks)
    {
        // Fills each array with random numbers
        std::ranges::generate(chunk, [&]{return Task{ .val = r_dist(rne), ._b_heavy = bernouili_dist(rne) };});
        
    }

    return chunks;
}

Dataset generate_data_sets_evenly()
{
    std::minstd_rand rne;
    std::uniform_real_distribution r_dist{0., std::numbers::pi};
    std::vector<std::array<Task, CHUNK_SIZE>> chunks(CHUNK_COUNT);

    const int every_nth = int(1. / PROBABILITY_HEAVY);
    // fill in the data set
    for(auto& chunk : chunks)
    {
        // Fills each array with random numbers
        std::ranges::generate(chunk, [&, i = 0]() mutable
            {
            const bool heavy = i++ % every_nth == 0;
                return Task
                {
                    
                .val = r_dist(rne), ._b_heavy = heavy
                };
            });
    }

    return chunks;
}

Dataset generate_data_sets_stacked()
{
    auto data = generate_data_sets_evenly();
    // Partition each chunk in the data
    for (auto& chunk : data)
        std::ranges::partition(chunk, std::identity{}, &Task::_b_heavy);
    
    return data;
}

// Helper func to call different generare functions
Dataset generate_data_sets_by_type(DatasetType type)
{
    switch (type)
    {
    case DatasetType::random:
        return generate_data_sets_random();
    case DatasetType::evenly:
        return generate_data_sets_evenly();
    case DatasetType::stacked:
        return generate_data_sets_stacked();
    default:
            LOG_ALWAYS(LogTemp, Error, "Unknown Dataset type");
            throw std::exception("Unknown Dataset type");
    }
}