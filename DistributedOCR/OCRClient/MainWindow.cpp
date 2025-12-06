#include "MainWindow.h"
#include "GrpcOcrClient.h"
#include <thread>

#include <QtCore/QCoreApplication>  
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

    // Copy paths to std::vector for the worker thread
    std::vector<std::string> paths;
    paths.reserve(imagePaths_.size());
    for (const QString& p : imagePaths_) {
        paths.push_back(p.toStdString());
    }

    // Disable buttons while running
    addButton_->setEnabled(false);
    runButton_->setEnabled(false);
    clearButton_->setEnabled(false);

    resultView_->clear();
    resultView_->append("Connecting to server " + serverAddr + "…");

    // Indeterminate progress bar while RPC is in flight
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(0);   // 0..0 = busy animation


    const std::string serverStr = serverAddr.toStdString();

    // Run the gRPC call on a background thread
    std::thread([this, serverStr, paths = std::move(paths)]() mutable {
        try {
            GrpcOcrClient client(serverStr);
            ocr::BatchResponse reply = client.sendBatch(paths);

            // Success: update UI on the Qt (GUI) thread
            QMetaObject::invokeMethod(this,
                [this, reply]() mutable {
                    const int n = reply.results_size();

                    // Now set a normal progress range
                    progressBar_->setMinimum(0);
                    progressBar_->setMaximum(n);
                    progressBar_->setValue(0);

                    resultView_->append(
                        QString("RPC succeeded. Got %1 results.\n").arg(n));

                    for (int i = 0; i < n; ++i) {
                        const ocr::BatchResult& r = reply.results(i);

                        resultView_->append(
                            QString("Result for id=%1:").arg(r.id()));
                        resultView_->append(
                            "text:\n" + QString::fromStdString(r.text()));
                        resultView_->append(
                            QString("processing_time_ms: %1\n")
                            .arg(r.processing_time_ms()));
                        resultView_->append(
                            "----------------------------------------\n");

                        if (i < imageList_->count()) {
                            auto* item = imageList_->item(i);
                            item->setText(QString::fromStdString(r.text()));
                        }

                        progressBar_->setValue(i + 1);
                    }

                    addButton_->setEnabled(true);
                    runButton_->setEnabled(true);
                    clearButton_->setEnabled(true);

                },
                Qt::QueuedConnection);
        }
        catch (const std::exception& ex) {
            // Error: report it on the GUI thread
            const std::string msg = ex.what();

            QMetaObject::invokeMethod(this,
                [this, msg]() {
                    // Reset bar
                    progressBar_->setMinimum(0);
                    progressBar_->setMaximum(1);
                    progressBar_->setValue(0);

                    resultView_->append("ERROR:");
                    resultView_->append(QString::fromStdString(msg));
                    resultView_->append("----------------------------------------\n");

                    QMessageBox::critical(
                        this,
                        "Error",
                        QString("Failed to run OCR:\n%1").arg(QString::fromStdString(msg))
                    );

                    addButton_->setEnabled(true);
                    runButton_->setEnabled(true);
                    clearButton_->setEnabled(true);

                },
                Qt::QueuedConnection);
        }
        }).detach();
}



void MainWindow::onClearImages()
{
    imagePaths_.clear();
    imageList_->clear();
    resultView_->clear();
    progressBar_->setMaximum(1);
    progressBar_->setValue(0);
}