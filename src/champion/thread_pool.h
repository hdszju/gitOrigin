#ifndef CHAMPION_THREAD_POOL_H
#define CHAMPION_THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <future>
#include <functional>

using namespace std;

namespace champion
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t threads)
            : stop(false)
        {
            for (size_t i = 0; i < threads; ++i)
            {
                workers.emplace_back([this]
                {
                    for (;;)
                    {
                        function<void()> task;
                        {
                            unique_lock<mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            task = move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                });
            }
        }

        template<typename TFunction, typename... TArgs>
        auto QueueTask(TFunction&& function, TArgs&&... args) -> future<typename result_of<TFunction(TArgs...)>::type>
        {
            // todo(hds): handle exception from task
            using return_type = typename result_of<TFunction(TArgs...)>::type;
            auto task = make_shared< packaged_task<return_type()> >(
                bind(forward<TFunction>(function), forward<TArgs>(args)...)
                );

            future<return_type> res = task->get_future();
            {
                unique_lock<mutex> lock(queue_mutex);

                if (stop)
                    throw std::runtime_error("enqueue on stopped ThreadPool");

                tasks.emplace([task](){ (*task)(); });
            }
            condition.notify_one();
            return res;
        }

        ~ThreadPool()
        {
            {
                unique_lock<mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (thread & worker : workers)
                worker.join();
        }

    private:
        vector<thread> workers;

        queue<function<void()>> tasks;

        mutex queue_mutex; //全局互斥锁

        condition_variable condition; //全局条件变量

        bool stop;
    };
}

#endif
