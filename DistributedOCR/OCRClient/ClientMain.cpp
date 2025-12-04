#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using ocr::OCRService;
using ocr::ImageRequest;
using ocr::ImageResponse;

class OCRClient {
public:
    explicit OCRClient(std::shared_ptr<Channel> channel)
        : stub_(OCRService::NewStub(channel)) {}

    void SendDummyRequest() {
        ImageRequest request;
        request.set_id(1);
        // Just sending dummy bytes for now
        request.set_image_data("hello-from-client");

        ImageResponse reply;
        ClientContext context;

        Status status = stub_->ProcessImage(&context, request, &reply);

        if (status.ok()) {
            std::cout << "? RPC succeeded\n";
            std::cout << "  id: " << reply.id() << "\n";
            std::cout << "  text: " << reply.text() << "\n";
            std::cout << "  processing_time_ms: " << reply.processing_time_ms() << "\n";
        } else {
            std::cout << "? RPC failed\n";
            std::cout << "  error_code: " << status.error_code() << "\n";
            std::cout << "  error_message: " << status.error_message() << "\n";
        }
    }

private:
    std::unique_ptr<OCRService::Stub> stub_;
};

int main() {
    // For now use localhost while server runs on same machine
    auto channel = grpc::CreateChannel("localhost:50051",
                                       grpc::InsecureChannelCredentials());

    OCRClient client(channel);
    client.SendDummyRequest();

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
