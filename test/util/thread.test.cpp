#include <mbgl/util/thread.hpp>

#include <mbgl/actor/actor_ref.hpp>
#include <mbgl/actor/scheduler.hpp>
#include <mbgl/platform/settings.hpp>
#include <mbgl/test/util.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/timer.hpp>

#include <atomic>
#include <memory>

using namespace mbgl;
using namespace mbgl::util;

class TestObject {
public:
    TestObject(ActorRef<TestObject>, std::thread::id otherTid)
        : tid(std::this_thread::get_id()) {
        EXPECT_NE(tid, otherTid); // Object is created on child thread
    }

    ~TestObject() {
        EXPECT_EQ(tid,
                  std::this_thread::get_id()); // Object is destroyed on child thread
    }

    void fn1(int val) const {
        EXPECT_EQ(tid, std::this_thread::get_id());
        EXPECT_EQ(val, 1);
    }

    void fn2(std::function<void(int)> cb) const {
        EXPECT_EQ(tid, std::this_thread::get_id());
        cb(1);
    }

    void transferIn(std::unique_ptr<int> val) const {
        EXPECT_EQ(tid, std::this_thread::get_id());
        EXPECT_EQ(*val, 1);
    }

    void transferInShared(std::shared_ptr<int> val) const {
        EXPECT_EQ(tid, std::this_thread::get_id());
        EXPECT_EQ(*val, 1);
    }

    void transferString(const std::string& string) const {
        EXPECT_EQ(tid, std::this_thread::get_id());
        EXPECT_EQ(string, "test");
    }

    void checkContext(std::promise<bool> result) const { result.set_value(tid == std::this_thread::get_id()); }

    void sync(std::promise<void> result) const { result.set_value(); }

    const std::thread::id tid;
};

TEST(Thread, invoke) {
    const std::thread::id tid = std::this_thread::get_id();
    Thread<TestObject> thread("Test", tid);

    thread.actor().invoke(&TestObject::fn1, 1);
    thread.actor().invoke(&TestObject::fn2, [](int result) { EXPECT_EQ(result, 1); });
    thread.actor().invoke(&TestObject::transferIn, std::make_unique<int>(1));
    thread.actor().invoke(&TestObject::transferInShared, std::make_shared<int>(1));

    std::string test("test");
    thread.actor().invoke(&TestObject::transferString, test);

    // Make sure the message queue was consumed before ending the test.
    std::promise<void> result;
    auto resultFuture = result.get_future();
    thread.actor().invoke(&TestObject::sync, std::move(result));
    resultFuture.get();
}

TEST(Thread, Context) {
    const std::thread::id tid = std::this_thread::get_id();
    Thread<TestObject> thread("Test", tid);

    std::promise<bool> result;
    auto resultFuture = result.get_future();

    thread.actor().invoke(&TestObject::checkContext, std::move(result));
    EXPECT_EQ(resultFuture.get(), true);
}

class TestWorker {
public:
    TestWorker(ActorRef<TestWorker>) {}

    void send(std::function<void()> cb) { cb(); }

    void sendDelayed(std::function<void()> cb) {
        timer.start(Milliseconds(300), mbgl::Duration::zero(), [cb] { cb(); });
    }

private:
    Timer timer;
};

TEST(Thread, ExecutesAfter) {
    RunLoop loop;
    Thread<TestWorker> thread("Test");

    bool didWork = false;
    bool didAfter = false;

    thread.actor().invoke(&TestWorker::send, [&] { didWork = true; });
    thread.actor().invoke(&TestWorker::send, [&] {
        didAfter = true;
        loop.stop();
    });

    loop.run();

    EXPECT_TRUE(didWork);
    EXPECT_TRUE(didAfter);
}

TEST(Thread, CanSelfWakeUp) {
    RunLoop loop;
    Thread<TestWorker> thread("Test");

    thread.actor().invoke(&TestWorker::sendDelayed, [&] { loop.stop(); });

    loop.run();
}

TEST(Thread, Concurrency) {
    auto loop = std::make_shared<RunLoop>();

    unsigned numMessages = 100000;
    std::atomic_uint completed(numMessages);

    auto& settings = platform::Settings::getInstance();
    if (!settings.get(platform::EXPERIMENTAL_THREAD_PRIORITY_WORKER).getDouble()) {
        settings.set(platform::EXPERIMENTAL_THREAD_PRIORITY_WORKER, 0.5);
    }

    Actor<TestWorker> poolWorker(Scheduler::GetBackground());
    auto poolWorkerRef = poolWorker.self();

    Thread<TestWorker> threadedObject("Test");
    auto threadedObjectRef = threadedObject.actor();

    // 10 threads sending 100k messages to the Thread. The
    // idea here is to test if the scheduler is handling concurrency
    // correctly, otherwise this test should crash.
    for (unsigned i = 0; i < numMessages; ++i) {
        poolWorkerRef.invoke(&TestWorker::send, [threadedObjectRef, loop, &completed]() mutable {
            threadedObjectRef.invoke(&TestWorker::send, [loop, &completed]() {
                if (!--completed) {
                    loop->stop();
                }
            });
        });
    };

    loop->run();
}

TEST(Thread, ThreadPoolMessaging) {
    auto loop = std::make_shared<RunLoop>();

    Actor<TestWorker> poolWorker(Scheduler::GetBackground());
    auto poolWorkerRef = poolWorker.self();

    Thread<TestWorker> threadedObject("Test");
    auto threadedObjectRef = threadedObject.actor();

    // This is sending a message to the Thread from the main
    // thread. Then the Thread will send another message to
    // a worker on the ThreadPool.
    threadedObjectRef.invoke(&TestWorker::send, [poolWorkerRef, loop]() mutable {
        poolWorkerRef.invoke(&TestWorker::send, [loop]() { loop->stop(); });
    });

    loop->run();

    // Same as before, but in the opposite direction.
    poolWorkerRef.invoke(&TestWorker::send, [threadedObjectRef, loop]() mutable {
        threadedObjectRef.invoke(&TestWorker::send, [loop]() { loop->stop(); });
    });

    loop->run();
}

TEST(Thread, ReferenceCanOutliveThread) {
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free" // See AspiringActor<>::object()
#endif
    auto thread = std::make_unique<Thread<TestWorker>>("Test");
    auto worker = thread->actor();

    thread.reset();

    for (unsigned i = 0; i < 1000; ++i) {
        worker.invoke(&TestWorker::send, [&] { ADD_FAILURE() << "Should never happen"; });
    }

    using namespace std::literals;
    std::this_thread::sleep_for(10s);
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic pop
#endif
}

TEST(Thread, DeletePausedThread) {
    std::atomic_bool flag(false);

    auto thread = std::make_unique<Thread<TestWorker>>("Test");
    thread->pause();
    thread->actor().invoke(&TestWorker::send, [&] { flag = true; });

    // Should not hang.
    thread.reset();

    // Should process the queue before destruction.
    ASSERT_TRUE(flag);
}

TEST(Thread, Pause) {
    RunLoop loop;

    std::atomic_bool flag(false);

    Thread<TestWorker> thread1("Test1");
    thread1.pause();

    Thread<TestWorker> thread2("Test2");

    for (unsigned i = 0; i < 100; ++i) {
        thread1.actor().invoke(&TestWorker::send, [&] { flag = true; });
        thread2.actor().invoke(&TestWorker::send, [&] { ASSERT_FALSE(flag); });
    }

    // Queue a message at the end of thread2 queue.
    thread2.actor().invoke(&TestWorker::send, [&] { loop.stop(); });
    loop.run();
}

TEST(Thread, Resume) {
    RunLoop loop;

    std::atomic_bool flag(false);

    Thread<TestWorker> thread("Test");
    thread.pause();

    for (unsigned i = 0; i < 100; ++i) {
        thread.actor().invoke(&TestWorker::send, [&] { flag = true; });
    }

    // Thread messages are ondered, when we resume, this is going
    // to me the last thing to run on the message queue.
    thread.actor().invoke(&TestWorker::send, [&] { loop.stop(); });

    // This test will be flaky if the thread doesn't get paused.
    ASSERT_FALSE(flag);

    thread.resume();
    loop.run();

    ASSERT_TRUE(flag);
}

TEST(Thread, PauseResume) {
    RunLoop loop;

    Thread<TestWorker> thread("Test");

    // Test if multiple pause/resume work.
    for (unsigned i = 0; i < 100; ++i) {
        thread.pause();
        thread.resume();
    }

    thread.actor().invoke(&TestWorker::send, [&] { loop.stop(); });
    loop.run();
}

class TestWorkerDelayedConstruction {
public:
    TestWorkerDelayedConstruction(ActorRef<TestWorkerDelayedConstruction>, std::future<void> start) { start.get(); }

    void send(std::function<void()> cb) { cb(); }

private:
    Timer timer;
};

TEST(Thread, InvokeBeforeChildStarts) {
    RunLoop loop;

    std::promise<void> start;
    Thread<TestWorkerDelayedConstruction> thread("Test", start.get_future());

    std::atomic<int> count{0};

    for (unsigned i = 0; i < 100; ++i) {
        thread.actor().invoke(&TestWorkerDelayedConstruction::send, [&] { ++count; });
    }

    thread.actor().invoke(&TestWorkerDelayedConstruction::send, [&] { loop.stop(); });

    // This test will be flaky if messages are consumed before the target object is constructed.
    ASSERT_EQ(count, 0);

    start.set_value();

    loop.run();

    ASSERT_EQ(count, 100);
}

TEST(Thread, DeleteBeforeChildStarts) {
    std::atomic_bool flag(false);
    std::promise<void> start;

    Thread<TestWorker> control("Control");
    auto thread = std::make_unique<Thread<TestWorkerDelayedConstruction>>("Test", start.get_future());

    thread->actor().invoke(&TestWorkerDelayedConstruction::send, [&] { flag = true; });

    control.actor().invoke(&TestWorker::sendDelayed, [&] { start.set_value(); });

    // Should not hang.
    thread.reset();

    // Should process the queue before destruction.
    ASSERT_TRUE(flag);
}

TEST(Thread, PoolWait) {
    auto pool = Scheduler::GetBackground();

    constexpr int threadCount = 10;
    for (int i = 0; i < threadCount; ++i) {
        pool->schedule([&] { std::this_thread::sleep_for(Milliseconds(100)); });
    }

    pool->waitForEmpty();
}

TEST(Thread, PoolWaitRecursiveAdd) {
    auto pool = Scheduler::GetBackground();

    pool->schedule([&] {
        // Scheduled tasks can add more tasks
        pool->schedule([&] {
            std::this_thread::sleep_for(Milliseconds(10));
            pool->schedule([&] { std::this_thread::sleep_for(Milliseconds(10)); });
        });
        std::this_thread::sleep_for(Milliseconds(10));
    });

    pool->waitForEmpty();
}

TEST(Thread, PoolWaitAdd) {
    auto pool = Scheduler::GetBackground();
    auto seq = Scheduler::GetSequenced();

    // add new tasks every few milliseconds
    struct State {
        std::atomic<bool> addActive{true};
        std::atomic<int> added{0};
        std::atomic<int> executed{0};
    };
    auto state = std::make_shared<State>();
    seq->schedule([pool, state] {
        while (state->addActive) {
            pool->schedule([state] { state->executed++; });
            state->added++;
        }
    });

    // Wait be sure some are added
    while (state->added < 1) {
        std::this_thread::sleep_for(Milliseconds(10));
    }

    // Add an item that should take long enough to be confident that
    // more items would be added by the sequential task if not blocked
    pool->schedule([&] { std::this_thread::sleep_for(Milliseconds(100)); });

    pool->waitForEmpty();

    state->addActive = false;
    pool->waitForEmpty();

    ASSERT_GE(state->added.load(), 1);
}

TEST(Thread, PoolWaitException) {
    const auto id = util::SimpleIdentity{};
    auto pool = Scheduler::GetBackground();

    std::atomic<int> caught{0};
    pool->setExceptionHandler([&](const auto) { caught++; });

    constexpr int threadCount = 3;
    for (int i = 0; i < threadCount; ++i) {
        pool->schedule(id, [=] {
            std::this_thread::sleep_for(Milliseconds(i));
            if (i & 1) {
                throw std::runtime_error("test");
            } else {
                throw 1;
            }
        });
    }

    // Exceptions shouldn't cause deadlocks by, e.g., abandoning locks.
    pool->waitForEmpty(id);
    EXPECT_EQ(threadCount, caught);
}

#if defined(NDEBUG)
TEST(Thread, WrongThread) {
    auto pool = Scheduler::GetBackground();

    // Asserts in debug builds, silently ignored in release.
    pool->schedule([&] { pool->waitForEmpty(); });

    pool->waitForEmpty();
}
#endif

std::function<void()> makeCounterThread(TaggedScheduler& pool,
                                        std::atomic<bool>* stopFlag,
                                        std::atomic<size_t>* counter) {
    return [=]() mutable {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        (*counter)++;
        if (!*stopFlag) {
            pool.schedule(makeCounterThread(pool, stopFlag, counter));
        }
    };
}

TEST(Thread, TaggedPools) {
    TaggedScheduler poolTag1{Scheduler::GetBackground(), {}};
    TaggedScheduler poolTag2{Scheduler::GetBackground(), {}};

    std::atomic<bool> stopTasks1{false};
    std::atomic<bool> stopTasks2{false};
    std::atomic<size_t> runCount1{0};
    std::atomic<size_t> runCount2{0};

    // Run a bunch of tasks on two different pool tags (that share the same thread pool)
    for (auto i = 0; i < 50; i++) {
        poolTag1.schedule(makeCounterThread(poolTag1, &stopTasks1, &runCount1));
        poolTag2.schedule(makeCounterThread(poolTag2, &stopTasks2, &runCount2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Stop tasks on queue 1 and wait for it to empty
    stopTasks1 = true;
    poolTag1.waitForEmpty();
    const auto totalRuns1 = runCount1.load();

    // Do the same for queue 2
    stopTasks2 = true;
    poolTag2.waitForEmpty();
    const auto totalRuns2 = runCount2.load();

    // No other tasks in queue 1 ran while waiting on queue 2.
    ASSERT_TRUE(totalRuns1 == runCount1);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Same for queue 2
    ASSERT_TRUE(totalRuns2 == runCount2);
}
