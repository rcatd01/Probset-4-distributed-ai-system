#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

#include "OcrProcessor.h"
#include "OcrWorkerPool.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using ocr::OCRService;
using ocr::BatchRequest;
using ocr::BatchResponse;
using ocr::ImageTask;
using ocr::BatchResult;

class OCRServiceImpl final : public OCRService::Service {
public:
    explicit OCRServiceImpl(std::size_t numThreads)
        : pool_(numThreads, 100) {
    }

    Status ProcessBatch(ServerContext* context,
        const BatchRequest* request,
        BatchResponse* reply) override {
        int taskCount = request->tasks_size();
        std::cout << "Received batch with " << taskCount << " tasks.\n";

        if (taskCount == 0) {
            return Status(grpc::StatusCode::INVALID_ARGUMENT,
                "BatchRequest.tasks is empty");
        }

        // Enqueue all tasks into the worker pool
        std::vector<std::future<OcrResult>> futures;
        futures.reserve(taskCount);
        std::vector<int> ids;
        ids.reserve(taskCount);

        for (const auto& task : request->tasks()) {
            std::cout << "  Enqueue task id=" << task.id()
                << " (" << task.image_data().size() << " bytes)\n";

            ids.push_back(task.id());
            futures.push_back(pool_.enqueue(task.id(), task.image_data()));
        }

        // Collect results in the same order
        for (std::size_t i = 0; i < futures.size(); ++i) {
            using namespace std::chrono;

            // Dynamically set timeout based on image size
            const auto& task = request->tasks(i);
            int timeoutSeconds = 30; // Default 30 seconds

            // LOWER threshold so the demo works with normal 4K images
            const std::size_t LARGE_IMAGE_BYTES = 500 * 1024;  // 500 KB threshold

            if (task.image_data().size() > LARGE_IMAGE_BYTES) {
                timeoutSeconds = 120; // Extended timeout for large images
                std::cout << "[LARGE IMAGE] Using extended timeout (" << timeoutSeconds
                    << "s) for id=" << ids[i]
                    << " (" << task.image_data().size() << " bytes)\n";
            }


            auto status = futures[i].wait_for(seconds(timeoutSeconds));

            if (status == std::future_status::timeout) {
                std::cerr << "Timeout processing task id=" << ids[i]
                    << " after " << timeoutSeconds << "s\n";

                BatchResult* out = reply->add_results();
                out->set_id(ids[i]);
                out->set_text("[TIMEOUT] OCR took too long");
                out->set_processing_time_ms(0);
                continue;
            }

            try {
                OcrResult result = futures[i].get();

                BatchResult* out = reply->add_results();
                out->set_id(ids[i]);
                out->set_text(result.text);
                out->set_processing_time_ms(result.processingTimeMs);

                std::cout << "Successfully processed task id=" << ids[i]
                    << " in " << result.processingTimeMs << "ms\n";
            }
            catch (const std::exception& ex) {
                std::cerr << "Error processing task id=" << ids[i]
                    << ": " << ex.what() << "\n";

                BatchResult* out = reply->add_results();
                out->set_id(ids[i]);
                out->set_text(std::string("[ERROR] ") + ex.what());
                out->set_processing_time_ms(0);
            }
        }

        std::cout << "Batch processing complete. Sent " << reply->results_size()
            << " results.\n";
        return Status::OK;
    }

private:
    OcrWorkerPool pool_;
};

void RunServer(const std::string& address) {
    //set number of threads
    std::size_t numThreads = 4; //std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    OCRServiceImpl service(numThreads);

    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening at: " << address
        << " with " << numThreads << " worker threads.\n";
    server->Wait();
}

int main() {
    RunServer("0.0.0.0:50051");
    return 0;
}