#include "OcrProcessor.h"

#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <vector>
#include <chrono>
#include <stdexcept>

OcrResult run_ocr_on_bytes(const std::string& imageBytes)
{
    // 1) Decode bytes into cv::Mat
    std::vector<uchar> buffer(imageBytes.begin(), imageBytes.end());
    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image data");
    }

    // optional: grayscale
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    // 2) Init Tesseract
    tesseract::TessBaseAPI tess;
    const char* lang = "eng";

    // CHANGE TESSERACT FILE PATH HERE (where eng.traineddata is)
    if (tess.Init("C:/Users/Rain/AppData/Local/Programs/Tesseract-OCR/tessdata", "eng") != 0) {
        throw std::runtime_error("Could not initialize Tesseract");
    }

    // 3) Run OCR with timing
    auto start = std::chrono::high_resolution_clock::now();

    tess.SetImage(gray.data,
                  gray.cols,
                  gray.rows,
                  1,                        // bytes per pixel
                  static_cast<int>(gray.step));

    char* outText = tess.GetUTF8Text();
    std::string text = outText ? std::string(outText) : "";
    tess.End();
    if (outText) {
        delete[] outText;
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return { text, ms };
}
