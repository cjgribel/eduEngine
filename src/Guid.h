#pragma once

#include <cstdint>      // uint64_t
#include <atomic>       // std::atomic
#include <functional>   // std::hash

namespace eeng
{
    class Guid
    {
    public:
        Guid() : value(invalid().value) {}
        explicit Guid(uint64_t val) : value(val) {}

        static Guid generate()
        {
            static std::atomic<uint64_t> counter{ 1 }; // 0 = invalid
            return Guid(counter.fetch_add(1, std::memory_order_relaxed));
        }

        static Guid invalid() { return Guid(0); }

        bool valid() const { return value != 0; }

        bool operator==(const Guid& other) const { return value == other.value; }
        bool operator!=(const Guid& other) const { return value != other.value; }
        bool operator<(const Guid& other) const { return value < other.value; }

        uint64_t raw() const { return value; }

    private:
        uint64_t value;
    };
}

namespace std {
    template<>
    struct hash<eeng::Guid>
    {
        size_t operator()(const eeng::Guid& guid) const noexcept
        {
            return std::hash<uint64_t>()(guid.raw());
        }
    };
}