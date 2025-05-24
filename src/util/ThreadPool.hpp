#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <future>
#include <queue>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <type_traits>
#include <utility>

class ThreadPool
{
public:
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename Func>
    auto queue_task(Func task) -> std::future<std::invoke_result_t<Func>>;

    bool is_task_queue_empty() const;

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> task_queue;
    mutable std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};

// Template definition remains in the header
template <typename Func>
auto ThreadPool::queue_task(Func task) -> std::future<std::invoke_result_t<Func>>
{
    using ResultType = std::invoke_result_t<Func>;

    auto packaged_task = std::make_shared<std::packaged_task<ResultType()>>(std::move(task));
    auto future = packaged_task->get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace([packaged_task]()
        {
            std::invoke(*packaged_task);
        });
    }
    cv.notify_one();
    return future;
}

#endif // THREAD_POOL_HPP
