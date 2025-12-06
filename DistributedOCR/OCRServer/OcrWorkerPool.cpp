#include "OcrWorkerPool.h"

#include <iostream>
#include <stdexcept>


OcrWorkerPool::OcrWorkerPool(std::size_t numThreads, std::size_t maxQueueSize)
    : maxQueueSize_(maxQueueSize) {
    if (numThreads == 0) numThreads = 1;
    if (maxQueueSize_ == 0) maxQueueSize_ = 100; // sensible default

    for (std::size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&OcrWorkerPool::workerLoop, this, static_cast<int>(i));
    }

    std::cout << "OcrWorkerPool started with " << numThreads
        << " threads, maxQueueSize=" << maxQueueSize_ << ".\n";
}

OcrWorkerPool::~OcrWorkerPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }

    std::cout << "OcrWorkerPool stopped.\n";
}

std::future<OcrResult> OcrWorkerPool::enqueue(int id, const std::string& imageBytes) {
    auto job = std::make_shared<OcrJob>();
    job->id = id;
    job->imageBytes = imageBytes;

    std::future<OcrResult> fut = job->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= maxQueueSize_) {
            throw std::runtime_error("Server overloaded: job queue is full");
        }
        queue_.push(job);
    }
    cv_.notify_one();

    return fut;
}

void OcrWorkerPool::workerLoop(int workerIndex) {
    while (true) {
        std::shared_ptr<OcrJob> job;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stopping_ || !queue_.empty();
                });

            if (stopping_ && queue_.empty()) {
                return; // exit thread
            }

            job = queue_.front();
            queue_.pop();
        }

        try {
            std::cout << "[Worker " << workerIndex
                << "] processing id=" << job->id << "\n";

            OcrResult result = run_ocr_on_bytes(job->imageBytes);
            job->promise.set_value(std::move(result));
        }
        catch (...) {
            job->promise.set_exception(std::current_exception());
        }
    }
}

