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
        if(!_result) {
            _result = std::forward<UserResType>(_Result);
            _ready_signal.release();
        }
    }

    RetValType get() {
        _ready_signal.acquire();
        return std::move(*_result);
    }

private:
    std::binary_semaphore _ready_signal{0};
    std::optional<RetValType> _result;
};

template<>
class shared_state<void>
{
public:
   void set() {
        if(!_completed) {
            _completed = true;
           _ready_signal.release();
        }
    }

    void get() {
        _ready_signal.acquire();
    }

private:
    std::binary_semaphore _ready_signal{0};
    bool _completed = false;
};

template<typename RetValType>
class promise;

template<typename RetValType>
class future {
    friend class promise<RetValType>;

public:
    RetValType get() {
        assert(!_result_acquired);
        _result_acquired = true;
        return _p_state->get();
    }

private:
    future(std::shared_ptr<shared_state<RetValType>> in_state_ptr) : _p_state{in_state_ptr} {}

    bool _result_acquired = false;
    std::shared_ptr<shared_state<RetValType>> _p_state;
};

template<typename RetValType>
class promise {
public:
    promise() : _p_shared_state(std::make_shared<shared_state<RetValType>>()) {}

    template<typename... UserResType>
    void set(UserResType&&... _Result) {
        _p_shared_state->set(std::forward<UserResType>(_Result)...);
    }

    future<RetValType> get_future() {
        assert(_future_available);
        _future_available = false;
        return {_p_shared_state};
    }

private:
    bool _future_available = true;
    std::shared_ptr<shared_state<RetValType>> _p_shared_state;
};

class task {
public:
    task() = default;
    task(const task&) = delete;
    task(task&& in_donor) noexcept : _executor{std::move(in_donor._executor)} {}

    task& operator=(const task&) = delete;
    task& operator=(task&& rhs) noexcept {
        _executor = std::move(rhs._executor);
        return *this;
    }

    void operator()() {
        _executor();
    }

    operator bool() const {
        return static_cast<bool>(_executor);
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
        _executor = [
            _function = std::forward<FuncType>(_Executor),
            _promise = std::forward<PromType>(_Promise),
            ..._params = std::forward<Params>(_Params) ]() mutable
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
        };
    }

    std::function<void()> _executor;
};

class thread_pool {
public:
    thread_pool(std::size_t in_workers_count) {
        _workers.reserve(in_workers_count);
        for(size_t i = 0; i < in_workers_count; i++) {
            _workers.emplace_back(this);
        }
    }

    template<typename FuncType, typename... Params>
    auto run(FuncType&& _Function, Params&&... _Params)
    {
        auto [_Task, _Future] = tk::task::make(std::forward<FuncType>(_Function), std::forward<Params>(_Params)...);
        {
            std::lock_guard lock{_task_queue_mutex};
            _tasks.push_back(std::move(_Task));
        }
        _cvar_queue_task.notify_one();
        return _Future;
    }

    void wait_for_all_done() {
        std::unique_lock ulock{_task_queue_mutex};
        _cvar_all_done.wait(ulock, [this]{return _tasks.empty();});
    }

    ~thread_pool() {
        for(auto& worker : _workers) {
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
        std::unique_lock ulock{_task_queue_mutex};
        _cvar_queue_task.wait(ulock, in_stop_token, [this]{return !_tasks.empty();});

        if(!in_stop_token.stop_requested()) {
            task = std::move(_tasks.front());
            _tasks.pop_front();

            if(_tasks.empty()) {
                _cvar_all_done.notify_all();
            }
        }
        return task;
    }

    std::mutex _task_queue_mutex;
    std::condition_variable_any _cvar_queue_task;
    std::condition_variable _cvar_all_done;
    std::deque<task> _tasks;
    std::vector<worker> _workers;
};

} // namespace tk

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;

    tk::thread_pool pool{4};
    const auto spitt = [](int milisecond)
    {
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