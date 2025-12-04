#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

#include "OcrProcessor.h"
#include "OcrWorkerPool.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using ocr::OCRService;
using ocr::ImageRequest;
using ocr::ImageResponse;

class OCRServiceImpl final : public OCRService::Service {
public:
    explicit OCRServiceImpl(std::size_t numThreads)
        : pool_(numThreads) {
    }

    Status ProcessImage(ServerContext* context,
        const ImageRequest* request,
        ImageResponse* reply) override {
        std::cout << "Received request id=" << request->id()
            << " (" << request->image_data().size() << " bytes)"
            << std::endl;

        // Enqueue job into the worker pool
        std::future<OcrResult> fut = pool_.enqueue(request->id(), request->image_data());

        try {
            // Wait for OCR result from worker thread
            OcrResult result = fut.get();

            reply->set_id(request->id());
            reply->set_text(result.text);
            reply->set_processing_time_ms(result.processingTimeMs);
            return Status::OK;
        }
        catch (const std::exception& ex) {
            std::cerr << "OCR error in worker: " << ex.what() << std::endl;
            return Status(grpc::StatusCode::INTERNAL, ex.what());
        }
    }

private:
    OcrWorkerPool pool_;
};


void RunServer(const std::string& address) {
    std::size_t numThreads = std::thread::hardware_concurrency();
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
