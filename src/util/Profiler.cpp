// Profiler.cpp
#include "Profiler.hpp"

using namespace eeng::util;

std::mutex Profiler::mtx_;
std::unordered_map<std::string, Profiler::CategoryData> Profiler::data_;
std::unordered_map<std::string, Profiler::TimePoint>     Profiler::active_;

void Profiler::start(const std::string& category,
    const std::string& subtask)
{
    auto now = Clock::now();
    std::lock_guard<std::mutex> lock(mtx_);
    active_[makeKey(category, subtask)] = now;
    // ensure the category exists
    data_[category];
}

void Profiler::stop(const std::string& category,
    const std::string& subtask)
{
    auto now = Clock::now();
    std::lock_guard<std::mutex> lock(mtx_);
    auto key = makeKey(category, subtask);
    auto it = active_.find(key);
    if (it == active_.end()) return;  // no matching start()

    double ms = std::chrono::duration<double, std::milli>(now - it->second).count();
    active_.erase(it);

    auto& acc = data_[category].accum[subtask];
    acc.totalMs += ms;
    acc.count += 1;
}

void Profiler::log(const std::string& category)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_.find(category);
    if (it == data_.end()) {
        std::cout << "[" << category << "] no data to report\n";
        return;
    }

    auto& accums = it->second.accum;
    // Determine the “overall” total time:
    double totalMs = 0.0;
    auto overallIt = accums.find(category);
    if (overallIt != accums.end()) {
        totalMs = overallIt->second.totalMs;
    } else {
        // fallback: sum all subtasks
        for (auto& p : accums)
            totalMs += p.second.totalMs;
    }

    std::cout << "[" << category << "] total="
              << std::fixed << std::setprecision(2)
              << totalMs << "ms\n";

    for (auto& p : accums) {
        const auto& name = p.first;
        const auto& acc  = p.second;
        double avg = acc.count ? (acc.totalMs / acc.count) : 0.0;
        double pct = totalMs > 0.0
                   ? (acc.totalMs / totalMs) * 100.0
                   : 0.0;

        std::cout
            << "  " << std::setw(15) << name
            << " total=" << std::setw(7) << std::fixed << std::setprecision(2)
            << acc.totalMs << "ms"
            << "  count=" << std::setw(3) << acc.count
            << "  avg="   << std::setw(7) << std::fixed << std::setprecision(2)
            << avg << "ms"
            << "  (" << std::setw(5) << std::fixed << std::setprecision(1)
            << pct << "%)\n";
    }
}

void Profiler::reset(const std::string& category)
{
    std::lock_guard<std::mutex> lock(mtx_);
    data_.erase(category);
}
