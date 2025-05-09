// Profiler.hpp
#pragma once

#include <chrono>
#include <iostream>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <mutex>
#include <string>
#include <unordered_map>

namespace eeng
{
namespace util
{

/**
 * @brief A global profiler that accumulates timing intervals
 *        for named categories and subtasks.
 *
 * Usage examples:
 *   eeng::util::Profiler::start("cat");            // overall interval
 *   …
 *   eeng::util::Profiler::stop("cat");
 *
 *   eeng::util::Profiler::start("cat","sub");    // named subtask
 *   …
 *   eeng::util::Profiler::stop("cat","sub");
 *
 * Then:
 *   eeng::util::Profiler::log("cat");              // write report to std::cout
 *   eeng::util::Profiler::log("cat", myStream);    // write to custom ostream
 *   eeng::util::Profiler::reset("cat");            // clear data
 */
class Profiler
{
public:

    /**
     * @brief Start timing an interval for the category as its own subtask.
     *
     * @param category  Category name (used also as subtask name).
     */
    static void start(const std::string& category)
    {
        start(category, category);
    }

    /**
     * @brief Stop timing the overall interval for the category.
     *
     * @param category  Category name (must match prior start).
     */
    static void stop(const std::string& category)
    {
        stop(category, category);
    }

    /**
     * @brief Start timing a named subtask within a category.
     *
     * @param category  Category name.
     * @param subtask   Subtask name.
     */
    static void start(const std::string& category,
                      const std::string& subtask);

    /**
     * @brief Stop timing a previously started subtask.
     *
     * @param category  Category name.
     * @param subtask   Subtask name.
     */
    static void stop(const std::string& category,
                     const std::string& subtask);

    /**
     * @brief Log accumulated data for a category to an output stream.
     *
     * @param category  Category name to log.
     * @param os        Output stream (default: std::cout).
     */
    static void log(const std::string& category,
                    std::ostream& os = std::cout);

    /**
     * @brief Reset all accumulated data for a category.
     *
     * @param category  Category name to reset.
     */
    static void reset(const std::string& category);

private:
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    struct Accum
    {
        double totalMs = 0.0;
        int    count   = 0;
    };

    struct CategoryData
    {
        std::unordered_map<std::string, Accum> accum;
    };

    static std::mutex                                    mtx_;
    static std::unordered_map<std::string, CategoryData> data_;
    static std::unordered_map<std::string, TimePoint>    active_;

    static std::string makeKey(const std::string& category,
                               const std::string& subtask)
    {
        return category + "#" + subtask;
    }
};

} // namespace util
} // namespace eeng
