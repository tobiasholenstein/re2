#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "re2/re2.h"
#include "re2/set.h"

static const std::vector<std::string>& Patterns() {
  static const std::vector<std::string> p = {
      "foo.*bar", "baz[0-9]+", "qux(a|b|c)", "hello\\s+world",
      "test_[a-z]+", "\\d{3}-\\d{4}", "prefix.*suffix",
      "alpha|beta|gamma",
  };
  return p;
}

int main() {
  const std::string text = "this text won't match any pattern";
  const int MAX_REPS = 500'000;

  // Baseline: per-thread RE2::Set (no sharing, no lock contention)
  std::printf("--- SEPARATE (per-thread RE2::Set, ideal baseline) ---\n");
  for (int num_workers : {1, 2, 4, 8, 16, 32}) {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; i++) {
      workers.push_back(std::thread([&] {
        re2::RE2::Options opts(re2::RE2::Quiet);
        re2::RE2::Set local_set(opts, re2::RE2::ANCHOR_BOTH);
        for (auto& p : Patterns()) local_set.Add(p, nullptr);
        local_set.Compile();
        std::vector<int> matches;
        for (int rep = 0; rep < MAX_REPS; rep++) {
          matches.clear();
          local_set.Match(text, &matches);
        }
      }));
    }
    for (auto& w : workers) w.join();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    double ms =
        std::chrono::duration<double, std::milli>(elapsed).count();
    std::printf("%2d threads: %7.1f ms  (%.1f ms/thread)\n",
                num_workers, ms, ms / num_workers);
  }

  // Shared RE2::Set (tests lock contention)
  std::printf("\n--- SHARED (single RE2::Set, tests lock scaling) ---\n");
  re2::RE2::Options opts(re2::RE2::Quiet);
  re2::RE2::Set set(opts, re2::RE2::ANCHOR_BOTH);
  for (auto& p : Patterns()) set.Add(p, nullptr);
  set.Compile();

  for (int num_workers : {1, 2, 4, 8, 16, 32}) {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; i++) {
      workers.push_back(std::thread([&] {
        std::vector<int> matches;
        for (int rep = 0; rep < MAX_REPS; rep++) {
          matches.clear();
          set.Match(text, &matches);
        }
      }));
    }
    for (auto& w : workers) w.join();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    double ms =
        std::chrono::duration<double, std::milli>(elapsed).count();
    std::printf("%2d threads: %7.1f ms  (%.1f ms/thread)\n",
                num_workers, ms, ms / num_workers);
  }
}
