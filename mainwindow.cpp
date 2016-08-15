#include "mainwindow.hpp"
#include "ui_mainwindow.h"

void MainWindow::takeScreenshot()
{
    emit pauseAll(true);

    // Get pixmap
    QPixmap originalPixmap = vulkan_window->screen()->grabWindow(vulkan_window->winId());

    // Save
    QDateTime dateTime = dateTime.currentDateTime();
    QString   path     = QDir::currentPath() + "/screenshot_nbody_" + dateTime.toString("yyyy_MM_dd_hh_mm_ss") + ".png";

    if (!originalPixmap.save(path))
    {
        qWarning("The image could not be saved");
    }

    ui->statusBar->showMessage("Saved screenshot in " + path, 5000);

    emit pauseAll(false);
}


void MainWindow::help()
{
    QMessageBox msgBox;

    msgBox.setWindowTitle("Hopefully helpful information");
    msgBox.setInformativeText("<p><strong>Movement</strong></p>"
                              "<ul>"
                              "<li>Hold down the <strong>right mouse button</strong> to look around</li>"
                              "<li>Use the keys <strong>W</strong>, <strong>A</strong>, <strong>S</strong>, <strong>D</strong> to move</li>"
                              "<li>Use <strong>Q</strong> and <strong>E</strong> to roll</li>"
                              "<li>Hold <strong>shift </strong>to move faster</li>"
                              "<li>Scroll the <strong>mouse wheel</strong> to change the camera&#39;s center of rotation</li>"
                              "<li>Mouse sensitivity can be adjusted in the controls</li>"
                              "</ul>"
                              "<p><strong>Performance</strong></p>"
                                                            "<ul>"
                              "<li>The two bars show relative time spent on graphics (top) and compute. The different colour represent different operations, like blurring and drawing </li>"
                              "<li>Higher fps or cps (computations per second) can be achieved for example by reducing the number of pareticles or setting the blur extent to zero</li>"
                              "</ul>");
    msgBox.setStandardButtons(QMessageBox::Ok);
    int ret = msgBox.exec();
    switch (ret)
    {
    case QMessageBox::Ok:
        msgBox.close();
        break;

    default:
        // should never be reached
        break;
    }
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    vulkan_window = new VulkanWindow;

    window_container = QWidget::createWindowContainer(vulkan_window);

    ui->widgetLayout->addWidget(window_container);

    connect(vulkan_window, &VulkanWindow::fpsStringChanged, this, &QWidget::setWindowTitle);

    connect(ui->doubleSpinBoxGravityConst, SIGNAL(valueChanged(double)), vulkan_window, SLOT(setGravitationalConstant(double)));
    connect(ui->doubleSpinBoxTimeStep, SIGNAL(valueChanged(double)), vulkan_window, SLOT(setTimeStep(double)));
    connect(ui->horizontalSliderBloomStrength, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setBloomStrength(int)));
    connect(ui->horizontalSliderBloomExtent, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setBloomExtent(int)));
    connect(ui->comboBoxToneMappingMode, SIGNAL(currentIndexChanged(int)), vulkan_window, SLOT(setToneMappingMode(int)));
    connect(ui->doubleSpinBoxSoftening, SIGNAL(valueChanged(double)), vulkan_window, SLOT(setSoftening(double)));
    connect(ui->horizontalSliderExposure, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setExposure(int)));
    connect(ui->horizontalSliderGamma, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setGamma(int)));
    connect(ui->spinBoxParticleCount, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setParticleCount(int)));
    connect(ui->pushButtonPause, SIGNAL(toggled(bool)), vulkan_window, SLOT(pauseCompute(bool)));
    connect(this, SIGNAL(pauseAll(bool)), vulkan_window, SLOT(pauseAll(bool)), Qt::DirectConnection);
    connect(ui->pushButtonLaunch, SIGNAL(clicked()), vulkan_window, SLOT(launch()));
    connect(ui->comboBoxInitialCondition, SIGNAL(currentIndexChanged(int)), vulkan_window, SLOT(setInitialCondition(int)));
    connect(ui->horizontalSliderPower, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setPower(int)));
    connect(ui->horizontalSliderParticleSize, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setParticleSize(int)));
    connect(ui->horizontalSliderMouseSensitivity, SIGNAL(valueChanged(int)), vulkan_window, SLOT(setMouseSensitivity(int)));
    connect(ui->pushButtonScreenshot, SIGNAL(clicked()), this, SLOT(takeScreenshot()));
    connect(ui->pushButtonHelp, SIGNAL(clicked()), this, SLOT(help()));

    vulkan_window->initialize();

    loadSettings();
}


MainWindow::~MainWindow()
{
    writeSettings();
    delete ui;
}


void MainWindow::loadSettings()
{
    QSettings settings("settings.ini", QSettings::IniFormat);

    this->restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    this->restoreState(settings.value("MainWindow/state").toByteArray());
    ui->splitter->restoreState(settings.value("MainWindow/splitter/state").toByteArray());
}


void MainWindow::writeSettings()
{
    QSettings settings("settings.ini", QSettings::IniFormat);

    settings.setValue("MainWindow/geometry", this->saveGeometry());
    settings.setValue("MainWindow/state", this->saveState());
    settings.setValue("MainWindow/splitter/state", ui->splitter->saveState());
}
