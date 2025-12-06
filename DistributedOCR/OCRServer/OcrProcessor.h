#pragma once

#include <string>
#include <thread>

struct OcrResult {
    std::string text;
    long long processingTimeMs;
};

OcrResult run_ocr_on_bytes(const std::string& imageBytes);
