// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include "bench/utils.h"

#include <xnnpack/AlignedAllocator.h>
#include <xnnpack/common.h>
#include <xnnpack/params.h>
#include <xnnpack/lut.h>


static void x8_lut(
  benchmark::State& state,
  xnn_x8_lut_ukernel_function lut,
  benchmark::utils::IsaCheckFunction isa_check = nullptr)
{
  if (isa_check && !isa_check(state)) {
    return;
  }

  const size_t num_elements = state.range(0);
  std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> input(num_elements);
  std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> output(num_elements);
  std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> table(256);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto u8rng = std::bind(
    std::uniform_int_distribution<uint32_t>(0, std::numeric_limits<uint8_t>::max()), std::ref(rng));
  std::generate(input.begin(), input.end(), std::ref(u8rng));
  std::generate(table.begin(), table.end(), std::ref(u8rng));
  std::fill(output.begin(), output.end(), UINT8_C(0xAA));

  for (auto _ : state) {
    lut(num_elements * sizeof(uint8_t), input.data(), output.data(), table.data());
  }

  const uint64_t cpu_frequency = benchmark::utils::GetCurrentCpuFrequency();
  if (cpu_frequency != 0) {
    state.counters["cpufreq"] = cpu_frequency;
  }

  const size_t elements_per_iteration = num_elements;
  state.counters["elements"] =
    benchmark::Counter(uint64_t(state.iterations()) * elements_per_iteration, benchmark::Counter::kIsRate);

  const size_t bytes_per_iteration = 2 * num_elements * sizeof(uint8_t);
  state.counters["bytes"] =
    benchmark::Counter(uint64_t(state.iterations()) * bytes_per_iteration, benchmark::Counter::kIsRate);
}

BENCHMARK_CAPTURE(x8_lut, scalar_x1,
                  xnn_x8_lut_ukernel__scalar_x1)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<uint8_t, uint8_t>)
  ->UseRealTime();
BENCHMARK_CAPTURE(x8_lut, scalar_x2,
                  xnn_x8_lut_ukernel__scalar_x2)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<uint8_t, uint8_t>)
  ->UseRealTime();
BENCHMARK_CAPTURE(x8_lut, scalar_x4,
                  xnn_x8_lut_ukernel__scalar_x4)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<uint8_t, uint8_t>)
  ->UseRealTime();
BENCHMARK_CAPTURE(x8_lut, scalar_x8,
                  xnn_x8_lut_ukernel__scalar_x8)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<uint8_t, uint8_t>)
  ->UseRealTime();
BENCHMARK_CAPTURE(x8_lut, scalar_x16,
                  xnn_x8_lut_ukernel__scalar_x16)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<uint8_t, uint8_t>)
  ->UseRealTime();

#ifndef XNNPACK_BENCHMARK_NO_MAIN
BENCHMARK_MAIN();
#endif