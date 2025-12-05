#include "MainWindow.h"
#include "GrpcOcrClient.h"

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QListView>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);

    //style
    setStyleSheet(
        "QMainWindow { background-color: #2b2b2b; }"
        "QWidget { background-color: #2b2b2b; color: #f0f0f0; }"
        "QListWidget { background-color: #333333; border: none; }"
        "QTextEdit { background-color: #222222; border: 1px solid #555555; }"
        "QLineEdit { background-color: #444444; border: 1px solid #666666; }"
        "QPushButton { background-color: #444444; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #555555; }"
        "QProgressBar { background-color: #333333; border: 1px solid #555555; }"
        "QProgressBar::chunk { background-color: #3b84ff; }"
    );

    //server address and buttons 
    auto* topRow = new QHBoxLayout();

    auto* serverLabel = new QLabel("Server:", this);
    serverEdit_ = new QLineEdit(this);
    serverEdit_->setText("localhost:50051");
    serverEdit_->setPlaceholderText("host:port");

    addButton_ = new QPushButton("Upload Images", this);
    runButton_ = new QPushButton("Run OCR", this);
    clearButton_ = new QPushButton("Clear", this);     

    topRow->addWidget(serverLabel);
    topRow->addWidget(serverEdit_, /*stretch*/ 1);
    topRow->addSpacing(16);
    topRow->addWidget(addButton_);
    topRow->addWidget(runButton_);
    topRow->addWidget(clearButton_);

    mainLayout->addLayout(topRow);

    // 0rogress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(1);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(6);
    mainLayout->addWidget(progressBar_);

    // Image grid 
    imageList_ = new QListWidget(this);
    imageList_->setViewMode(QListView::IconMode);
    imageList_->setResizeMode(QListView::Adjust);
    imageList_->setMovement(QListView::Static);
    imageList_->setIconSize(QSize(220, 80));     // thumbnail size
    imageList_->setSpacing(16);
    imageList_->setSelectionMode(QAbstractItemView::NoSelection);
    imageList_->setUniformItemSizes(true);

    mainLayout->addWidget(imageList_, /*stretch*/ 3);

    // Results text area at bottom 
    resultView_ = new QTextEdit(this);
    resultView_->setReadOnly(true);
    resultView_->setPlaceholderText("OCR results and logs will appear here…");
    mainLayout->addWidget(resultView_, /*stretch*/ 2);

    setCentralWidget(central);
    setWindowTitle("Distributed OCR Qt Client");

    // Button connections 
    QObject::connect(addButton_, &QPushButton::clicked, [this]() {
        onAddImages();
        });
    QObject::connect(runButton_, &QPushButton::clicked, [this]() {
        onRunOcr();
        });
    QObject::connect(clearButton_, &QPushButton::clicked, [this]() {  
        onClearImages();
        });
}

void MainWindow::onAddImages() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Select images",
        QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)");

    if (files.isEmpty())
        return;

    for (const QString& f : files) {
        if (!imagePaths_.contains(f)) {
            imagePaths_.append(f);

           
            auto* item = new QListWidgetItem();
            item->setData(Qt::UserRole, f);     // store path
            item->setText("Pending…");

            QPixmap pix(f);
            if (!pix.isNull()) {
                QPixmap scaled = pix.scaled(
                    imageList_->iconSize(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
                item->setIcon(QIcon(scaled));
            }

            item->setToolTip(f);
            imageList_->addItem(item);
        }
    }
}

void MainWindow::onRunOcr() {
    if (imagePaths_.isEmpty()) {
        QMessageBox::warning(this, "No images",
            "Please add at least one image first.");
        return;
    }

    const QString serverAddr = serverEdit_->text().trimmed();
    if (serverAddr.isEmpty()) {
        QMessageBox::warning(this, "No server address",
            "Please enter something like localhost:50051.");
        return;
    }

    std::vector<std::string> paths;
    paths.reserve(imagePaths_.size());
    for (const QString& p : imagePaths_) {
        paths.push_back(p.toStdString());
    }

    // progress bar
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(0); 

    resultView_->clear();
    resultView_->append("Connecting to server " + serverAddr + "…");

    try {
        GrpcOcrClient client(serverAddr.toStdString());
        ocr::BatchResponse reply = client.sendBatch(paths);

        progressBar_->setMaximum(1);
        progressBar_->setValue(1);

        resultView_->append(
            QString("RPC succeeded. Got %1 results.\n")
            .arg(reply.results_size()));

        // Update both the tiled grid and the log text
        for (int i = 0; i < reply.results_size(); ++i) {
            const ocr::BatchResult& r = reply.results(i);

            // 1) Log in the bottom text area
            resultView_->append(QString("Result for id=%1:").arg(r.id()));
            resultView_->append("text:\n" +
                QString::fromStdString(r.text()));
            resultView_->append(
                QString("processing_time_ms: %1\n")
                .arg(r.processing_time_ms()));
            resultView_->append(
                "----------------------------------------\n");

            // 2) Show recognized text under each image tile
            if (i < imageList_->count()) {
                auto* item = imageList_->item(i);
                item->setText(QString::fromStdString(r.text()));
            }
        }
    }
    catch (const std::exception& ex) {
        progressBar_->setMaximum(1);
        progressBar_->setValue(0);

        QMessageBox::critical(this, "Error",
            QString("Failed to run OCR:\n%1").arg(ex.what()));
    }
}

void MainWindow::onClearImages()
{
    imagePaths_.clear();
    imageList_->clear();
    resultView_->clear();
    progressBar_->setMaximum(1);
    progressBar_->setValue(0);
}