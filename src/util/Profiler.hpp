// Profiler.hpp
#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <string>
#include <unordered_map>

namespace eeng::util {

    /**
     * @brief A simple global profiler that accumulates timing intervals
     *        for named categories and subtasks.
     *
     * You can call either:
     *   - Profiler::start("cat");           // overall interval = "cat"
     *     …
     *     Profiler::stop("cat");
     *
     *   - Profiler::start("cat","sub");     // named subtask
     *     …
     *     Profiler::stop("cat","sub");
     *
     * Then:
     *   - Profiler::log("cat");             // prints total/count/avg/% for each subtask
     *   - Profiler::reset("cat");           // clears all data for that category
     */
    class Profiler {
    public:
        /**
         * @brief One-arg overload: start timing an interval for the given category as its own subtask.
         *
         * @param category  Name of the category (and subtask) to start.
         */
        static void start(const std::string& category) {
            start(category, category);
        }

        /**
         * @brief Stop timing the overall interval for the given category.
         *
         * Matches a prior call to start(category). Accumulates the elapsed
         * time into the "/overall" bucket for that category.
         *
         * @param category  Name of the category to stop.
         */
        static void stop(const std::string& category) {
            stop(category, category);
        }

        /**
         * @brief Start timing a named subtask within a category.
         *
         * If this is the first subtask for the category, the category’s
         * data map is created. Records a timestamp to be matched by stop().
         *
         * @param category  Name of the category.
         * @param subtask   Name of the subtask within the category.
         */
        static void start(const std::string& category,
            const std::string& subtask);

        /**
         * @brief Stop timing a previously started subtask.
         *
         * Finds the matching start(category, subtask) timestamp, computes
         * the elapsed milliseconds, and accumulates them into the subtask’s
         * total and increments its count.
         *
         * @param category  Name of the category.
         * @param subtask   Name of the subtask to stop.
         */
        static void stop(const std::string& category,
            const std::string& subtask);

        /**
         * @brief Log all accumulated data for a category to the given output stream.
         *
         * Prints to std::cout:
         *   - Total time for the category
         *   - For each subtask: total, count, average, and percentage of total
         *
         * If no data exists, prints a "no data" message.
         *
         * @param category  Name of the category to log.
         * @param os        Output stream to write the report to.
         */
        static void log(const std::string& category, std::ostream& os = std::cout);

        /**
         * @brief Reset all accumulated data for a category.
         *
         * Erases all subtasks and their accumulators. Does not remove
         * any in-flight start() entries unless you extend it.
         *
         * @param category  Name of the category to reset.
         */
        static void reset(const std::string& category);

    private:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = Clock::time_point;

        struct Accum {
            double totalMs = 0.0;  ///< total accumulated milliseconds
            int    count = 0;    ///< how many intervals contributed
        };

        struct CategoryData {
            std::unordered_map<std::string, Accum> accum;  ///< per-subtask accumulators
        };

        static std::mutex                                      mtx_;     ///< guards all maps
        static std::unordered_map<std::string, CategoryData>   data_;    ///< category → data
        static std::unordered_map<std::string, TimePoint>      active_;  ///< active starts

        /**
         * @brief Helper to create a unique map key from category and subtask.
         *
         * @param category  Category name
         * @param subtask   Subtask name
         * @return Combined key string
         */
        static std::string makeKey(const std::string& category,
            const std::string& subtask) {
            return category + "#" + subtask;
        }
    };

} // namespace eeng::util