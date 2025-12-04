#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

#include "OcrProcessor.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using ocr::OCRService;
using ocr::ImageRequest;
using ocr::ImageResponse;

class OCRServiceImpl final : public OCRService::Service {
public:
    Status ProcessImage(ServerContext* context,
        const ImageRequest* request,
        ImageResponse* reply) override {
        try {
            std::cout << "Received request id=" << request->id()
                << " (" << request->image_data().size() << " bytes)"
                << std::endl;

            // Call your real OCR helper
            OcrResult result = run_ocr_on_bytes(request->image_data());

            reply->set_id(request->id());
            reply->set_text(result.text);
            reply->set_processing_time_ms(result.processingTimeMs);
            return Status::OK;
        }
        catch (const std::exception& ex) {
            std::cerr << "OCR error: " << ex.what() << std::endl;
            return Status(grpc::StatusCode::INTERNAL, ex.what());
        }
    }
};

void RunServer(const std::string& address) {
    OCRServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening at: " << address << std::endl;
    server->Wait();
}

int main() {
    RunServer("0.0.0.0:50051");
    return 0;
}
