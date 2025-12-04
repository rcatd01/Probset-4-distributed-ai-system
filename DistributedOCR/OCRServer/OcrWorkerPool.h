#pragma once

#include "OcrProcessor.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

struct OcrJob {
    int id;
    std::string imageBytes;
    std::promise<OcrResult> promise;
};

class OcrWorkerPool {
public:
    explicit OcrWorkerPool(std::size_t numThreads);
    ~OcrWorkerPool();

    std::future<OcrResult> enqueue(int id, const std::string& imageBytes);

private:
    void workerLoop(int workerIndex);

    std::vector<std::thread> workers_;
    std::queue<std::shared_ptr<OcrJob>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};
