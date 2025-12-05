#pragma once

#include <QtWidgets/QMainWindow>
#include <QtCore/QStringList>

class QWidget;
class QLineEdit;
class QPushButton;
class QListWidget;
class QTextEdit;
class QProgressBar;


class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void onAddImages();
    void onRunOcr();
    void onClearImages();   

    QStringList   imagePaths_;
    QLineEdit* serverEdit_;
    QPushButton* addButton_;
    QPushButton* runButton_;
    QPushButton* clearButton_;   
    QListWidget* imageList_;
    QTextEdit* resultView_;
    QProgressBar* progressBar_;
};
