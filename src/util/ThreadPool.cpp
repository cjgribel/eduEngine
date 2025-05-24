#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t thread_count)
{
    for (size_t i = 0; i < thread_count; ++i)
    {
        workers.emplace_back([this]()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    cv.wait(lock, [this]()
                    {
                        return !task_queue.empty() || stop;
                    });

                    if (stop && task_queue.empty())
                    {
                        return;
                    }

                    task = std::move(task_queue.front());
                    task_queue.pop();
                }

                try
                {
                    task();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Exception in thread: " << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "Unknown exception in thread." << std::endl;
                }
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    cv.notify_all();
    for (auto& worker : workers)
    {
        worker.join();
    }
}

bool ThreadPool::is_task_queue_empty() const
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return task_queue.empty();
}
