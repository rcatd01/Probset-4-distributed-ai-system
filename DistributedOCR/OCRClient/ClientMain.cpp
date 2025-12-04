#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <stdexcept>

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using ocr::OCRService;
using ocr::ImageRequest;
using ocr::ImageResponse;

// Helper: read a binary file into a std::string
std::string read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    return std::string(buffer.begin(), buffer.end());
}

class OCRClient {
public:
    explicit OCRClient(std::shared_ptr<Channel> channel)
        : stub_(OCRService::NewStub(channel)) {
    }

    void SendImage(const std::string& imagePath) {
        // 1) Read image bytes from disk
        std::string bytes = read_file_bytes(imagePath);

        // 2) Build request
        ImageRequest request;
        request.set_id(1);
        request.set_image_data(bytes);

        ImageResponse reply;
        ClientContext context;

        // 3) Call RPC
        Status status = stub_->ProcessImage(&context, request, &reply);

        // 4) Handle response
        if (status.ok()) {
            std::cout << "RPC succeeded\n";
            std::cout << "  id: " << reply.id() << "\n";
            std::cout << "  text:\n" << reply.text() << "\n";
            std::cout << "  processing_time_ms: " << reply.processing_time_ms() << "\n";
        }
        else {
            std::cout << "RPC failed\n";
            std::cout << "  error_code: " << status.error_code() << "\n";
            std::cout << "  error_message: " << status.error_message() << "\n";
        }
    }

private:
    std::unique_ptr<OCRService::Stub> stub_;
};

int main() {
    // Change this to any PNG/JPG with text
    std::string imagePath = "C:\\Users\\Rain\\Downloads\\datasets\\dataset1\\img0001.png";

    // For now, server and client run on same machine
    auto channel = grpc::CreateChannel("localhost:50051",
        grpc::InsecureChannelCredentials());

    OCRClient client(channel);

    try {
        client.SendImage(imagePath);
    }
    catch (const std::exception& ex) {
        std::cout << "Exception: " << ex.what() << std::endl;
    }

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
