#include <mbgl/actor/scheduler.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_observer.hpp>
#include <mbgl/gfx/headless_frontend.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/style/sources/vector_source.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/thread.hpp>
#include <mbgl/util/thread_pool.hpp>
#include <mbgl/util/scoped.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/tile_range.hpp>
#include <mbgl/util/logging.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/util/identity.hpp>
#include <mbgl/util/platform.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <random>
#include <format>

#ifdef WRITE_DEBUG_HELPERS
#include <iostream>
#include <fstream>
#endif

namespace mbgl {
namespace test {

class MapThreadObserver : public mbgl::MapObserver {};

struct TaskBucket {
    struct Task {
        mbgl::CanonicalTileID location{0, 0, 0};
        std::function<void(mbgl::CanonicalTileID, mbgl::PremultipliedImage)> callback;
    };

    std::atomic_bool runFlag{true};
    std::condition_variable taskCondition;
    std::atomic_int64_t taskCount{0};
    std::mutex queueMutex;
    std::queue<Task> taskQueue;

    void scheduleTask(Task&& task) {
        {
            std::lock_guard<std::mutex> queueLock{queueMutex};
            taskQueue.push(std::move(task));
            taskCount++;
        }
        taskCondition.notify_one();
    }

    bool tryTakeOneTask(Task& task) {
        bool hasWork{false};
        {
            std::lock_guard<std::mutex> queueLock{queueMutex};
            if (taskQueue.size()) {
                task = std::move(taskQueue.front());
                taskQueue.pop();
                hasWork = true;
            }
        }
        return hasWork;
    }
};

class TestThread {
public:
    TestThread(TaskBucket& taskBucket_, mbgl::Size viewSize, float pixelRatio, std::string mbtilesPath)
        : runLoop(mbgl::util::RunLoop::Type::New),
          taskBucket(taskBucket_) {
        frontend = std::make_unique<mbgl::HeadlessFrontend>(viewSize, pixelRatio);
        map = std::make_unique<mbgl::Map>(*frontend,
                                          observer,
                                          mbgl::MapOptions()
                                              .withSize(frontend->getSize())
                                              .withPixelRatio(pixelRatio)
                                              .withMapMode(mbgl::MapMode::Static),
                                          mbgl::ResourceOptions(),
                                          mbgl::ClientOptions());
        map->getStyle().loadURL("https://americanamap.org/style.json");
        waitFor([this]() { return !map->getStyle().isLoaded(); });

        map->getStyle().loadJSON(patchStyleJSON(map->getStyle().getJSON(), mbtilesPath));
        waitFor([this]() { return !map->getStyle().isLoaded(); });
    }

    template <typename Pred>
    void erasePred(rapidjson::Value& val, Pred&& pred) {
        for (auto it = val.Begin(); it != val.End(); ++it) {
            if (pred(it)) {
                it = val.Erase(it);
            }
        }
    }

    // Patch out DEM sources and swap the vector source with a local mbtiles path
    std::string patchStyleJSON(const std::string& json, const std::string& mbtilesPath) {
        rapidjson::Document doc;
        doc.Parse<0>(json);

        auto sources = doc.FindMember("sources");
        sources->value.RemoveMember("dem");

        auto layers = doc.FindMember("layers");
        erasePred(layers->value, [](rapidjson::Value* it) {
            const auto str = it->FindMember("source")->value.GetString();
            return (std::string(str) == "dem");
        });

        const auto mbPath = "mbtiles://" + mbtilesPath;
        doc["sources"]["openmaptiles"]["url"].SetString(mbPath.c_str(), mbPath.length());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        const auto patchedJSON = buffer.GetString();
#ifdef WRITE_DEBUG_HELPERS
        std::ofstream out("debug.json");
        out << patchedJSON << std::endl;
        out.close();
#endif
        return patchedJSON;
    }

    template <typename Pred>
    void waitFor(Pred&& cond) {
        while (cond()) {
            runLoop.runOnce();
        }
    }

    void run() {
        while (taskBucket.runFlag) {
            if (!taskBucket.taskCount) {
                std::unique_lock<std::mutex> taskLock(taskMutex);
                taskBucket.taskCondition.wait(taskLock,
                                              [this]() { return taskBucket.taskCount || !taskBucket.runFlag; });
                taskLock.unlock();
            }

            if (!taskBucket.runFlag) {
                break;
            }

            TaskBucket::Task task;
            if (taskBucket.tryTakeOneTask(task)) {
                runTask(std::move(task));
            }
        }
    }

private:
    void runTask(TaskBucket::Task&& task) {
        try {
            map->jumpTo(mbgl::CameraOptions()
                            .withCenter(mbgl::LatLngBounds(task.location).center())
                            .withZoom(static_cast<double>(task.location.z)));

            auto result = frontend->render(*map);
            task.callback(task.location, std::move(result.image));
        } catch (const std::exception& e) {
            mbgl::Log::Error(mbgl::Event::Render, std::format("Exception thrown while rendering task: {}", e.what()));
            taskBucket.runFlag = false; // stop all renderers
            taskBucket.taskCondition.notify_all();
            throw e; // rethrow the exception after logging it
        }
    }

    MapThreadObserver observer;
    std::unique_ptr<mbgl::HeadlessFrontend> frontend;
    std::unique_ptr<mbgl::Map> map;
    mbgl::util::RunLoop runLoop;

    std::mutex taskMutex;
    TaskBucket& taskBucket;
};

/* A pool of TestThreads */
class RenderPool : public mbgl::ParallelScheduler {
public:
    RenderPool(int64_t numThreads, mbgl::Size viewSize, float pixelRatio, std::string mbtilesPath)
        : mbgl::ParallelScheduler(numThreads - 1 /* extraThreads */) {
        for (auto i = 0; i < numThreads; i++) {
            mbgl::Log::Info(mbgl::Event::General, "Creating render thread " + std::to_string(i + 1));
            schedule([viewSize, pixelRatio, mbtilesPath, this]() {
                livingWorkers++;
                mbgl::Scoped counter{[this]() {
                    livingWorkers--;
                }};

                auto worker = TestThread(taskBucket, viewSize, pixelRatio, mbtilesPath);
                worker.run();
            });
        }
    }

    ~RenderPool() {
        taskBucket.runFlag = false;
        taskBucket.taskCondition.notify_all();

        do {
            auto currentWorkers = livingWorkers.load();
            if (currentWorkers == 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (livingWorkers.load());
    }

    void blockUntilEmpty() { waitForEmpty(mbgl::util::SimpleIdentity::Empty); }

    TaskBucket& getTaskBucket() { return taskBucket; }

private:
    TaskBucket taskBucket;
    std::atomic_int64_t livingWorkers{0};
};

class Test {
public:
    Test(std::string mbtilesPath, uint8_t zoom, uint32_t numThreads)
        : tileRange(mbgl::util::TileRange::fromLatLngBounds(mbgl::LatLngBounds::world(), zoom)),
          threadPool(numThreads, {512, 512} /* size */, 2.0f /* pixelRatio */, mbtilesPath) {
        rng = decltype(rng)(rd());
        rng.seed(0xF0CACC1A);

        // Schedule a bunch of work
        mbgl::Log::Info(mbgl::Event::General, "Scheduling work");
        for (auto i = 0; i < 1'000'000; i++) {
            threadPool.getTaskBucket().scheduleTask(
                {randomLocation(zoom),
                 [](mbgl::CanonicalTileID location, [[maybe_unused]] mbgl::PremultipliedImage image) {
                     const auto png = encodePNG(image);
#ifdef WRITE_DEBUG_HELPERS
                     std::ofstream out(std::format("debug-thread-{}.png", mbgl::platform::getCurrentThreadName()),
                                       std::ios::binary);
                     out << png << std::endl;
                     out.close();
#endif
                     mbgl::Log::Info(mbgl::Event::Render,
                                     std::format("Rendered tile: {}/{}/{} - PNG size: {}b",
                                                 location.z,
                                                 location.x,
                                                 location.y,
                                                 png.size()));
                 }});
        }

        threadPool.blockUntilEmpty();
    }

    mbgl::CanonicalTileID randomLocation(uint8_t zoom) {
        return {zoom,
                std::uniform_int_distribution(tileRange.range.min.x, tileRange.range.max.x - 1)(rng),
                std::uniform_int_distribution(tileRange.range.min.y, tileRange.range.max.y - 1)(rng)};
    }

private:
    const mbgl::util::TileRange tileRange;

    RenderPool threadPool;
    std::random_device rd;
    std::mt19937 rng;
};

} // namespace test
} // namespace mbgl

int main(int argv, char** argc) {
    if (argv < 2) {
        mbgl::Log::Error(mbgl::Event::General, "Missing argument 1 - provide the absolute path to an mbtiles file.");
        return -1;
    }
    mbgl::Log::Info(mbgl::Event::General, "Using mbtiles file: " + std::string(argc[1]));
    auto test = mbgl::test::Test(argc[1] /* mbtilesPath */, 10 /* zoom */, 16 /* numThreads */);
    return 0;
}