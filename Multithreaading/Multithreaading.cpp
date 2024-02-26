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
    
class task {
public:
    task() = default;
    task(const task&) = delete;
    task(task&& in_donor) noexcept : executor_{std::move(in_donor.executor_)} {}

    task& operator=(const task&) = delete;
    task& operator=(task&& rhs) noexcept {
        executor_ = std::move(rhs.executor_);
        return *this;
    }

    void operator()() {
        executor_();
    }

    operator bool() const {
        return static_cast<bool>(executor_);
    }

    template<typename FuncType, typename... Params>
    static auto make(FuncType&& in_executor, Params&&... in_Params) {
        std::promise<std::invoke_result_t<FuncType, Params...>> promise;
        auto future = promise.get_future();
        return std::make_pair(
            task{std::forward<FuncType>(in_executor), std::move(promise), std::forward<Params>(in_Params)...},
            std::move(future));
    }

private:
    template<typename FuncType, typename PromType, typename ...Params>
    task(FuncType&& _Executor, PromType&& _Promise, Params&&... _Params) {
        executor_ = [
            _function = std::forward<FuncType>(_Executor),
            _promise = std::forward<PromType>(_Promise),
            ..._params = std::forward<Params>(_Params) ]() mutable
        {
            try
            {
                if constexpr(std::is_void_v<std::invoke_result_t<FuncType, Params...>>)
                {
                    _function(std::forward<Params>(_params)...);
                    _promise.set_value();
                }
                else
                {
                    _promise.set_value(_function(std::forward<Params>(_params)...));
                }
            }
            catch (...)
            {
                _promise.set_exception(std::current_exception());
            }
        };
    }

    std::move_only_function<void()> executor_;
};

class thread_pool {
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
        auto [task, future] = tk::task::make(std::forward<FuncType>(function), std::forward<Params>(params)...);
        {
            std::lock_guard lock{task_queue_mutex_};
            tasks_.push_back(std::move(task));
        }
        cvar_queue_task_.notify_one();
        return std::move(future);
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