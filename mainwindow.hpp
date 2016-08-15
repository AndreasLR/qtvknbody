/*
 * The Qt mainwindow. Parent GUI object
 * */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardPaths>
#include <QFileDialog>
#include <QSettings>
#include <QDateTime>
#include <QScreen>
#include <QMessageBox>

#include "platform.hpp"
#include "vulkanbase.hpp"
#include "vulkanwindow.hpp"

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void pauseAll(bool);

public slots:
    void takeScreenshot();
    void help();

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    void loadSettings();
    void writeSettings();

    VulkanWindow *vulkan_window;

    QWidget *window_container;
};

#endif // MAINWINDOW_H
