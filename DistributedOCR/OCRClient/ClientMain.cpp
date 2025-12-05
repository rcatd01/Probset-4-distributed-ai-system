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
using ocr::BatchRequest;
using ocr::BatchResponse;
using ocr::ImageTask;
using ocr::BatchResult;

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

    void SendBatch(const std::string& serverAddress,
        const std::vector<std::string>& imagePaths) {

        BatchRequest request;

        // Build BatchRequest from all image paths
        int idCounter = 1;
        for (const auto& path : imagePaths) {
            std::cout << "Adding image " << path << " as task id=" << idCounter << "\n";

            std::string bytes = read_file_bytes(path);

            ImageTask* task = request.add_tasks();
            task->set_id(idCounter);
            task->set_image_data(bytes);

            ++idCounter;
        }

        BatchResponse reply;
        ClientContext context;

        Status status = stub_->ProcessBatch(&context, request, &reply);

        if (!status.ok()) {
            std::cout << "  RPC failed\n";
            std::cout << "  error_code: " << status.error_code() << "\n";
            std::cout << "  error_message: " << status.error_message() << "\n";
            return;
        }

        std::cout << "RPC succeeded\n";
        std::cout << "Received " << reply.results_size() << " results:\n\n";

        for (int i = 0; i < reply.results_size(); ++i) {
            const BatchResult& r = reply.results(i);
            std::cout << "Result for id=" << r.id() << ":\n";
            std::cout << "  text:\n" << r.text() << "\n";
            std::cout << "  processing_time_ms: " << r.processing_time_ms() << "\n";
            std::cout << "----------------------------------------\n";
        }
    }

private:
    std::unique_ptr<OCRService::Stub> stub_;
};

int main() {
    std::string serverAddress;
    std::cout << "Enter server address (e.g. localhost:50051): ";
    std::getline(std::cin, serverAddress);
    if (serverAddress.empty()) {
        serverAddress = "localhost:50051";
    }

    int count = 0;
    std::cout << "How many images do you want to send in this batch? ";
    std::cin >> count;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::vector<std::string> paths;
    paths.reserve(count);

    for (int i = 0; i < count; ++i) {
        std::string path;
        std::cout << "Enter path for image " << (i + 1) << ": ";
        std::getline(std::cin, path);
        paths.push_back(path);
    }

    auto channel = grpc::CreateChannel(serverAddress,
        grpc::InsecureChannelCredentials());

    OCRClient client(channel);

    try {
        client.SendBatch(serverAddress, paths);
    }
    catch (const std::exception& ex) {
        std::cout << "Exception: " << ex.what() << std::endl;
    }

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
