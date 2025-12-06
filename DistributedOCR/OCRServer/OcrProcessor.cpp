#include "OcrProcessor.h"

#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <vector>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <iostream>

// Thread-local Tesseract instance - initialized once per thread
thread_local std::unique_ptr<tesseract::TessBaseAPI> tess_instance;

tesseract::TessBaseAPI* get_tess_instance() {
    if (!tess_instance) {
        std::cout << "[Thread " << std::this_thread::get_id()
            << "] Initializing Tesseract...\n";

        tess_instance = std::make_unique<tesseract::TessBaseAPI>();

        // CHANGE TESSERACT FILE PATH HERE (where eng.traineddata is)
        if (tess_instance->Init("C:/Users/Rain/AppData/Local/Programs/Tesseract-OCR/tessdata", "eng") != 0) {
            tess_instance.reset();
            throw std::runtime_error("Could not initialize Tesseract");
        }

        std::cout << "[Thread " << std::this_thread::get_id()
            << "] Tesseract initialized successfully\n";
    }
    return tess_instance.get();
}

OcrResult run_ocr_on_bytes(const std::string& imageBytes)
{
    std::cout << "[OCR] Processing image (" << imageBytes.size() << " bytes)\n";

    // 1) Decode bytes into cv::Mat
    std::vector<uchar> buffer(imageBytes.begin(), imageBytes.end());
    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image data");
    }

    std::cout << "[OCR] Decoded image: " << img.cols << "x" << img.rows << "\n";

    // 2) Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    std::cout << "[OCR] Converted to grayscale\n";

    // 3) Get thread-local Tesseract instance
    tesseract::TessBaseAPI* tess = get_tess_instance();

    std::cout << "[OCR] Got Tesseract instance, setting image...\n";

    // 4) Run OCR with timing
    auto start = std::chrono::high_resolution_clock::now();

    tess->SetImage(gray.data,
        gray.cols,
        gray.rows,
        1,                        // bytes per pixel
        static_cast<int>(gray.step));

    std::cout << "[OCR] Running recognition...\n";

    char* outText = tess->GetUTF8Text();
    std::string text = outText ? std::string(outText) : "";

    if (outText) {
        delete[] outText;
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "[OCR] Recognition complete in " << ms << "ms, extracted "
        << text.length() << " characters\n";

    return { text, ms };
}