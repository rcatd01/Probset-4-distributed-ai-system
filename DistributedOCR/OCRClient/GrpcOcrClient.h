#pragma once

#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"

#include <memory>
#include <string>
#include <vector>

class GrpcOcrClient {
public:
    explicit GrpcOcrClient(const std::string& serverAddress);

    // imagePaths = list of image file paths on the client machine
    ocr::BatchResponse sendBatch(const std::vector<std::string>& imagePaths);

private:
    std::unique_ptr<ocr::OCRService::Stub> stub_;
};
