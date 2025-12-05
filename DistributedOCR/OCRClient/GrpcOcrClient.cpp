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
        throw std::runtime_error(
            "RPC failed: code=" + std::to_string(status.error_code()) +
            " msg=" + status.error_message());
    }
    return reply;
}
