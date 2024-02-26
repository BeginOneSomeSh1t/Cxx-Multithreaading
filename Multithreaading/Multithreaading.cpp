#include <cassert>
#include <condition_variable>
#include <deque>
#include <optional>
#include <functional>
#include <iostream>
#include <semaphore>
#include <sstream>
#include <ranges>

namespace rn = std::ranges;
namespace vi = std::views;

namespace tk {

template<typename RetValType>
class shared_state {
public:
    template<typename UserResType>
    void set(UserResType&& _Result) {
        if(!result_) {
            result_ = std::forward<UserResType>(_Result);
            ready_signal_.release();
        }
    }

    RetValType get() {
        ready_signal_.acquire();
        return std::move(*result_);
    }

private:
    std::binary_semaphore ready_signal_{0};
    std::optional<RetValType> result_;
    std::exception_ptr exception_;
};

template<>
class shared_state<void>
{
public:
   void set() {
        if(!completed_) {
            completed_ = true;
           ready_signal_.release();
        }
    }

    void get() {
        ready_signal_.acquire();
    }

private:
    std::binary_semaphore ready_signal_{0};
    bool completed_ = false;
};

template<typename RetValType>
class promise;

template<typename RetValType>
class future {
    friend class promise<RetValType>;

public:
    RetValType get() {
        assert(!result_acquired_);
        result_acquired_ = true;
        return p_state_->get();
    }

private:
    future(std::shared_ptr<shared_state<RetValType>> in_state_ptr) : p_state_{in_state_ptr} {}

    bool result_acquired_ = false;
    std::shared_ptr<shared_state<RetValType>> p_state_;
};

template<typename RetValType>
class promise {
public:
    promise() : p_shared_state_(std::make_shared<shared_state<RetValType>>()) {}

    template<typename... UserResType>
    void set(UserResType&&... _Result) {
        p_shared_state_->set(std::forward<UserResType>(_Result)...);
    }

    future<RetValType> get_future() {
        assert(_future_available);
        _future_available = false;
        return {p_shared_state_};
    }

private:
    bool _future_available = true;
    std::shared_ptr<shared_state<RetValType>> p_shared_state_;
};

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
        promise<std::invoke_result_t<FuncType, Params...>> promise;
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
                    _promise.set();
                }
                else
                {
                    _promise.set(_function(std::forward<Params>(_params)...));
                }
            }
            catch (...)
            {
                
            }
        };
    }

    std::function<void()> executor_;
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
    auto run(FuncType&& _Function, Params&&... _Params)
    {
        auto [_Task, _Future] = tk::task::make(std::forward<FuncType>(_Function), std::forward<Params>(_Params)...);
        {
            std::lock_guard lock{task_queue_mutex_};
            tasks_.push_back(std::move(_Task));
        }
        cvar_queue_task_.notify_one();
        return _Future;
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
        worker(thread_pool* in_pool_ptr) : _p_pool(in_pool_ptr), _thread(std::bind_front(&worker::run_kernel, this)){}

        void request_stop() {
            _thread.request_stop();
        }

    private:
        void run_kernel(std::stop_token in_stop_token) {
            while(auto task = _p_pool->get_task(in_stop_token)) {
                task();
            }
        }

        thread_pool* _p_pool;
        std::jthread _thread;
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

    for(auto& future : futures) {
        std::cout << std::format("<< {} >>\n", future.get());
    }

    return 0;
}