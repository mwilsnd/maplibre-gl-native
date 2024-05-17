#include <mbgl/util/thread_pool.hpp>

#include <mbgl/platform/settings.hpp>
#include <mbgl/platform/thread.hpp>
#include <mbgl/util/monotonic_timer.hpp>
#include <mbgl/util/platform.hpp>
#include <mbgl/util/string.hpp>

namespace mbgl {

ThreadedSchedulerBase::~ThreadedSchedulerBase() = default;

void ThreadedSchedulerBase::terminate() {
    // Run any leftover render jobs
    runRenderJobs();

    terminated = true;

    // Wake up all threads so that they shut down
    cvAvailable.notify_all();
}

std::thread ThreadedSchedulerBase::makeSchedulerThread(size_t index) {
    return std::thread([this, index] {
        auto& settings = platform::Settings::getInstance();
        auto value = settings.get(platform::EXPERIMENTAL_THREAD_PRIORITY_WORKER);
        if (auto* priority = value.getDouble()) {
            platform::setCurrentThreadPriority(*priority);
        }

        platform::setCurrentThreadName("Worker " + util::toString(index + 1));
        platform::attachThread();

        owningThreadPool.set(this);

        while (!terminated) {
            std::vector<std::shared_ptr<Queue>> pending;
            bool didWork = false;

            {
                // 1. Gather buckets for us to visit this iteration
                std::shared_lock<std::shared_mutex> lock(taggedQueueLock);
                for (auto& [tag, q] : taggedQueue) {
                    pending.push_back(q);
                }
            }

            // 2. Visit a task from each
            for (auto q : pending) {
                std::function<void()> tasklet;

                {
                    std::unique_lock<std::mutex> lock(q->lock);
                    if (q->queue.size()) {
                        tasklet = std::move(q->queue.front());
                        q->queue.pop();
                    }
                    if (!tasklet) continue;
                }

                q->runningCount++;

                try {
                    didWork = true;
                    tasklet();

                    // destroy the function and release its captures before unblocking `waitForEmpty`
                    tasklet = {};
                    if (!--q->runningCount) {
                        std::unique_lock<std::mutex> lock(q->lock);
                        if (q->queue.empty()) {
                            q->cv.notify_all();
                        }
                    }
                } catch (...) {
                    std::unique_lock<std::mutex> lock(q->lock);
                    if (handler) {
                        handler(std::current_exception());
                    }

                    tasklet = {};
                    if (!--q->runningCount && q->queue.empty()) {
                        q->cv.notify_all();
                    }

                    if (handler) {
                        continue;
                    }
                    throw;
                }
            }

            if (!didWork) {
                std::unique_lock<std::mutex> conditionLock(workerMutex);
                cvAvailable.wait(conditionLock);
            }
        }

        platform::detachThread();
    });
}

void ThreadedSchedulerBase::schedule(std::function<void()>&& fn) {
    schedule(static_cast<const void*>(this), std::move(fn));
}

void ThreadedSchedulerBase::schedule(const void* tag, std::function<void()>&& fn) {
    assert(fn);
    if (!fn) return;

    decltype(taggedQueue)::const_iterator it;
    std::shared_ptr<Queue> q;

    {
        std::unique_lock<std::shared_mutex> lock(taggedQueueLock);
        it = taggedQueue.find(tag);
        if (it == taggedQueue.end()) {
            q = std::make_shared<Queue>();
            taggedQueue.insert({tag, q});
        } else {
            q = it->second;
        }
    }

    {
        std::unique_lock<std::mutex> lock(q->lock);
        q->queue.push(std::move(fn));
    }

    cvAvailable.notify_one();
}

void ThreadedSchedulerBase::waitForEmpty(const void* tag = nullptr) {
    // Must not be called from a thread in our pool, or we would deadlock
    assert(!thisThreadIsOwned());
    if (!thisThreadIsOwned()) {
        if (!tag) {
            tag = static_cast<const void*>(this);
        }

        decltype(taggedQueue)::const_iterator it;
        std::shared_ptr<Queue> q;
        {
            std::shared_lock<std::shared_mutex> lock(taggedQueueLock);
            it = taggedQueue.find(tag);
            if (it == taggedQueue.end()) {
                return;
            }
            q = it->second;
        }

        std::unique_lock<std::mutex> lock(q->lock);
        while (q->queue.size() + q->runningCount) {
            q->cv.wait(lock);
        }

        // After waiting for the queue to empty, go ahead and erase it from the map.
        taggedQueue.erase(tag);
    }

    return;
}

} // namespace mbgl
