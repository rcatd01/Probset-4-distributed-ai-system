#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

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
        std::cout << "Received request id=" << request->id()
            << " (" << request->image_data().size() << " bytes)" << std::endl;

        reply->set_id(request->id());
        reply->set_text("Dummy OCR reply");
        reply->set_processing_time_ms(0);
        return Status::OK;
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
