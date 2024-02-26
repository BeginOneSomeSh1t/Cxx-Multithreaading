#include <cassert>
#include <condition_variable>
#include <deque>
#include <optional>
#include <functional>
#include <iostream>
#include <semaphore>
#include <sstream>
#include <ranges>
#include <future>
#include <variant>

namespace rn = std::ranges;
namespace vi = std::views;

namespace tk {
    
class thread_pool {

    using task = std::move_only_function<void()>;
public:
    thread_pool(std::size_t in_workers_count) {
        workers_.reserve(in_workers_count);
        for(size_t i = 0; i < in_workers_count; i++) {
            workers_.emplace_back(this);
        }
    }

    template<typename FuncType, typename... Params>
    auto run(FuncType&& function, Params&&... params)
    {
        using ret_type = std::invoke_result_t<FuncType, Params...>;
        auto pak = std::packaged_task<ret_type()>{std::bind(
            std::forward<FuncType>(function), std::forward<Params>(params)...
        )};
        auto future = pak.get_future();
        task t = {
            [pak = std::move(pak)]() mutable
            {
                pak();
            }
        };
        
        {
            std::lock_guard lock{task_queue_mutex_};
            tasks_.push_back(std::move(t));
        }
        
        cvar_queue_task_.notify_one();
        return future;
    }

    void wait_for_all_done() {
        std::unique_lock ulock{task_queue_mutex_};
        cvar_all_done_.wait(ulock, [this]{return tasks_.empty();});
    }

    ~thread_pool() {
        for(auto& worker : workers_) {
            worker.request_stop();
        }
    }

private:
    class worker {
    public:
        worker(thread_pool* pool) : p_pool_(pool), thread_(std::bind_front(&worker::run_kernel_, this)){}

        void request_stop() {
            thread_.request_stop();
        }

    private:
        void run_kernel_(std::stop_token in_stop_token) {
            while(auto task = p_pool_->get_task(in_stop_token)) {
                task();
            }
        }

        thread_pool* p_pool_;
        std::jthread thread_;
    };

    task get_task(std::stop_token& in_stop_token) {
        task task;
        std::unique_lock ulock{task_queue_mutex_};
        cvar_queue_task_.wait(ulock, in_stop_token, [this]{return !tasks_.empty();});

        if(!in_stop_token.stop_requested()) {
            task = std::move(tasks_.front());
            tasks_.pop_front();

            if(tasks_.empty()) {
                cvar_all_done_.notify_all();
            }
        }
        return task;
    }

    std::mutex task_queue_mutex_;
    std::condition_variable_any cvar_queue_task_;
    std::condition_variable cvar_all_done_;
    std::deque<task> tasks_;
    std::vector<worker> workers_;
};

} // namespace tk

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;

    tk::thread_pool pool{4};
    const auto spitt = [](int milisecond)
    {
        if(milisecond && milisecond % 100 == 0)
        {
            throw std::runtime_error{"whee!"};
        }
        std::this_thread::sleep_for(1ms * milisecond);
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    };

    auto futures = vi::iota(0, 39) |
        vi::transform([&](int i){return pool.run(spitt, i*25);}) |
            rn::to<std::vector>();

    for(auto& future : futures)
    {
        try
        {
            std::cout << std::format("<< {} >>\n", future.get());
        }
        catch (...)
        {
            std::cout << "whee!\n";
        }
    }

    auto future = pool.run([]{std::this_thread::sleep_for(2000ms); return 69;});
    while(future.wait_for(250ms) != std::future_status::ready)
    {
        std::cout << "Waiting for result...\n";
    }

    std:: cout << "Result: " << future.get() << "\n";

    return 0;
}