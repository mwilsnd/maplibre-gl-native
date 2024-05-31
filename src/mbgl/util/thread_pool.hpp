#pragma once

#include <mbgl/actor/mailbox.hpp>
#include <mbgl/actor/scheduler.hpp>
#include <mbgl/util/thread_local.hpp>
#include <mbgl/util/containers.hpp>

#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mbgl {

class ThreadedSchedulerBase : public Scheduler {
public:
    /// @brief Schedule a generic task not assigned to any particular owner.
    /// The scheduler itself will own the task.
    /// @param fn Task to run
    void schedule(std::function<void()>&& fn) override;

    /// @brief Schedule a task assigned to the given owner `tag`.
    /// @param tag Address of any object used to identify ownership of `fn`
    /// @param fn Task to run
    void schedule(const void* tag, std::function<void()>&& fn) override;

protected:
    ThreadedSchedulerBase() = default;
    ~ThreadedSchedulerBase() override;

    void terminate();
    std::thread makeSchedulerThread(size_t index);

    /// @brief Wait until there's nothing pending or in process
    /// Must not be called from a task provided to this scheduler.
    /// @param tag Address of the owner identifying the collection of tasks to
    // wait for. Waiting on nullptr waits on tasks owned by the scheduler.
    void waitForEmpty(const void* tag = nullptr) override;

    /// Returns true if called from a thread managed by the scheduler
    bool thisThreadIsOwned() const { return owningThreadPool.get() == this; }

    // Signal when an item is added to the queue
    std::condition_variable cvAvailable;
    std::mutex workerMutex;
    std::mutex taggedQueueLock;
    util::ThreadLocal<ThreadedSchedulerBase> owningThreadPool;
    bool terminated{false};

    // Task queues bucketed by tag address
    struct Queue {
        std::atomic<std::size_t> runningCount;   /* running tasks */
        std::condition_variable cv;              /* queue empty condition */
        std::mutex lock;                         /* lock */
        std::queue<std::function<void()>> queue; /* pending task queue */
    };
    mbgl::unordered_map<const void*, std::shared_ptr<Queue>> taggedQueue;
};

/**
 * @brief ThreadScheduler implements Scheduler interface using a lightweight event loop
 *
 * @tparam N number of threads
 *
 * Note: If N == 1 all scheduled tasks are guaranteed to execute consequently;
 * otherwise, some of the scheduled tasks might be executed in parallel.
 */
class ThreadedScheduler : public ThreadedSchedulerBase {
public:
    ThreadedScheduler(std::size_t n)
        : threads(n) {
        for (std::size_t i = 0u; i < threads.size(); ++i) {
            threads[i] = makeSchedulerThread(i);
        }
    }

    ~ThreadedScheduler() override {
        assert(!thisThreadIsOwned());
        terminate();
        for (auto& thread : threads) {
            assert(std::this_thread::get_id() != thread.get_id());
            thread.join();
        }
    }

    void runOnRenderThread(std::function<void()>&& fn) override {
        std::lock_guard<std::mutex> lock(renderMutex);
        renderThreadQueue.push(std::move(fn));
    }

    void runRenderJobs() override {
        std::lock_guard<std::mutex> lock(renderMutex);
        while (renderThreadQueue.size()) {
            auto fn = std::move(renderThreadQueue.front());
            renderThreadQueue.pop();
            if (fn) {
                fn();
            }
        }
    }

    mapbox::base::WeakPtr<Scheduler> makeWeakPtr() override { return weakFactory.makeWeakPtr(); }

private:
    std::vector<std::thread> threads;
    mapbox::base::WeakPtrFactory<Scheduler> weakFactory{this};

    std::queue<std::function<void()>> renderThreadQueue;
    std::mutex renderMutex;
};

class SequencedScheduler : public ThreadedScheduler {
public:
    SequencedScheduler()
        : ThreadedScheduler(1) {}
};

class ParallelScheduler : public ThreadedScheduler {
public:
    ParallelScheduler(std::size_t extra)
        : ThreadedScheduler(1 + extra) {}
};

class ThreadPool : public ParallelScheduler {
public:
    ThreadPool()
        : ParallelScheduler(3) {}
};

} // namespace mbgl
