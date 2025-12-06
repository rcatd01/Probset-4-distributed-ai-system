#include "GrpcOcrClient.h"

#include <fstream>
#include <stdexcept>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using ocr::BatchRequest;
using ocr::BatchResponse;
using ocr::ImageTask;

static std::string read_file_bytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::vector<char> buf((std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    return std::string(buf.begin(), buf.end());
}

GrpcOcrClient::GrpcOcrClient(const std::string& serverAddress) {
    auto channel = grpc::CreateChannel(serverAddress,
        grpc::InsecureChannelCredentials());
    stub_ = ocr::OCRService::NewStub(channel);
}

BatchResponse GrpcOcrClient::sendBatch(const std::vector<std::string>& imagePaths) {
    BatchRequest request;

    int id = 1;
    for (const auto& path : imagePaths) {
        std::string bytes = read_file_bytes(path);

        ImageTask* task = request.add_tasks();
        task->set_id(id++);
        task->set_image_data(bytes);
    }

    BatchResponse reply;
    ClientContext ctx;
    Status status = stub_->ProcessBatch(&ctx, request, &reply);

    if (!status.ok()) {
        std::string friendly;

        switch (status.error_code()) {
        case grpc::StatusCode::UNAVAILABLE:
            // Server died / network lost while we were talking to it
            friendly = "Connection lost. Please try again later.";
            break;

        case grpc::StatusCode::DEADLINE_EXCEEDED:
            // Server took too long to respond
            friendly = "The OCR server took too long to respond (timeout).";
            break;

        default:
            // Fallback: show the raw gRPC message
            friendly = status.error_message();
            break;
        }

        // This is what MainWindow will display
        throw std::runtime_error(
            "RPC failed (code=" + std::to_string(status.error_code()) +
            "): " + friendly
        );
    }

    return reply;
}
