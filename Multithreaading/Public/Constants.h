#pragma once

inline constexpr bool CHUNK_MEASUREMENT_ENABLED = true;

// experimental settings
inline constexpr size_t WORKER_COUNT = 4;
inline constexpr size_t CHUNK_SIZE = 8000;
inline constexpr size_t CHUNK_COUNT = 100;
inline constexpr size_t SUBSET_SIZE = CHUNK_SIZE / WORKER_COUNT;
inline constexpr size_t LIGHT_ITERATIONS = 100;
inline constexpr size_t HEAVY_ITERATIONS = 1'000;
inline constexpr double PROBABILITY_HEAVY = .15;


// ensnure the chunk size is a multiple of 4
static_assert(CHUNK_SIZE >= WORKER_COUNT, "CHUNK_SIZE must be greater than or equal to WORKER_COUNT");
static_assert(CHUNK_SIZE % WORKER_COUNT == 0);