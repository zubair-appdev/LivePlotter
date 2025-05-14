
#include "mainwindow.h"
#include "ui_mainwindow.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serialObj =   new serialPortHandler(this);

    connect(ui->pushButton_clear,&QPushButton::clicked,ui->textEdit_rawBytes,&QTextEdit::clear);

    ui->comboBox_ports->addItems(serialObj->availablePorts());

    connect(ui->pushButton_portsRefresh,&QPushButton::clicked,this,&MainWindow::refreshPorts);

    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));

    connect(this,&MainWindow::sendMsgId,serialObj,&serialPortHandler::recvMsgId);


    //writeToNotes from serial class
    connect(serialObj,&serialPortHandler::executeWriteToNotes,this,&MainWindow::writeToNotes);

    //debugging signals
    connect(serialObj,&serialPortHandler::portOpening,this,&MainWindow::portStatus);

    //reset previous notes #Notes things : Logging file
    resetLogFile();
    writeToNotes("*****  Application Started  *****");
    //#################################################

    //Response Timer *********************************************##############
    responseTimer = new QTimer(this);
    responseTimer->setSingleShot(true); // Ensure it fires only once per use

    // Connect the timer's timeout signal to a slot that handles the timeout
    connect(responseTimer, &QTimer::timeout, this, &MainWindow::handleTimeout);

    connect(serialObj, &serialPortHandler::dataReceived, this, &MainWindow::onDataReceived);
    //************************************************************##############

    //Plotting signals
    connect(serialObj,&serialPortHandler::plotLiveData,this,&MainWindow::recvLivePlotData);
    //power command
    connect(serialObj,&serialPortHandler::sendPowerData,this,&MainWindow::receivePowerData);

    initializePlot();

}

MainWindow::~MainWindow()
{
    writeToNotes("****** Application Closed ******");
    delete ui;
    delete serialObj;
    delete responseTimer;
    closeLogFile();
}

void MainWindow::initializeLogFile() {
    if (!logFile.isOpen()) {
        logFile.setFileName("debug_notes.txt");
        if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open log file.";
        } else {
            logStream.setDevice(&logFile);
        }
    }
}

void MainWindow::resetLogFile() {
    // Close the log file if it is open
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }

    // Check if the file exists and delete it
    QFile::remove("debug_notes.txt");

    // Reinitialize the log file
    initializeLogFile();
}


void MainWindow::writeToNotes(const QString &data) {
    if (!logFile.isOpen()) {
        qCritical() << "Log file is not open.";
        return;
    }

    // Add a timestamp for each entry
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logStream << "[" << timestamp << "] " << data << Qt::endl;
    logStream.flush(); // Ensure immediate write to disk
}

void MainWindow::closeLogFile() {
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}

quint8 MainWindow::calculateChecksum(const QByteArray &data)
{
    quint8 checkSum = 0;
    for(quint8 byte : data)
    {
        checkSum ^= byte;
    }

    return checkSum;
}

void MainWindow::refreshPorts()
{
    QString currentPort = ui->comboBox_ports->currentText();

    qDebug()<<"Refreshing ports...";
    ui->comboBox_ports->clear();
    QStringList availablePorts;
    ui->comboBox_ports->addItems(serialObj->availablePorts());

    ui->comboBox_ports->setCurrentText(currentPort);
}

void MainWindow::onPortSelected(const QString &portName)
{
    serialObj->setPORTNAME(portName);
}

void MainWindow::handleTimeout()
{
    QMessageBox::warning(this, "Timeout", "Hardware Not Responding!");
}

void MainWindow::onDataReceived()
{
    // Stop the timer since data has been received
    qDebug()<<"Hello stop it";
    if (responseTimer->isActive()) {
        responseTimer->stop();
    }
}


//The below function is intended for providing space between hex bytes
QString MainWindow::hexBytes(QByteArray &cmd)
{
    //**************************Visuals*******************
    QString hexOutput = cmd.toHex().toUpper();
    QString formattedHexOutput;

    for (int i = 0; i < hexOutput.size(); i += 2) {
        if (i > 0) {
            formattedHexOutput += " ";
        }
        formattedHexOutput += hexOutput.mid(i, 2);
    }
    return formattedHexOutput;
    //**************************Visuals*******************
}

void MainWindow::initializePlot()
{
    // Clear existing data from the plot
    if (ui->customPlot_chLive1->graphCount() > 0) {
        ui->customPlot_chLive1->graph(0)->data()->clear();
    } else {
        // Create a single graph if it doesn't already exist
        ui->customPlot_chLive1->addGraph();
    }

    // Set axes labels (only needs to be done once)
    ui->customPlot_chLive1->xAxis->setLabel("Sample Number");
    ui->customPlot_chLive1->yAxis->setLabel("Scaled Value");

    // Customize graph appearance (optional)
    ui->customPlot_chLive1->graph(0)->setPen(QPen(Qt::blue)); // Set line color
    ui->customPlot_chLive1->graph(0)->setLineStyle(QCPGraph::lsLine); // Line style
    ui->customPlot_chLive1->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone)); // No scatter points

    // Enable zooming and panning
    ui->customPlot_chLive1->setInteraction(QCP::iRangeZoom, true);       // Enable zooming
    ui->customPlot_chLive1->setInteraction(QCP::iRangeDrag, true);       // Enable panning

    // Repaint the plot after clearing and setting up
    ui->customPlot_chLive1->replot(QCustomPlot::rpQueuedReplot);

    qInfo() << "Plot initialized and cleared.";
}


void MainWindow::portStatus(const QString &data)
{
    if(data.startsWith("Serial object is not initialized/port not selected"))
    {
        QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 460800"))
    {
        QMessageBox::information(this,"Success",data);
    }

    if(data.startsWith("Failed to open port"))
    {
        QMessageBox::critical(this,"Error",data);
    }

    ui->textEdit_rawBytes->append(data);
}


void MainWindow::on_pushButton_start_clicked()
{
    initializePlot();
    sampleNumber = 0;
    allXValues.clear();
    allYValues.clear();

    // Start the timeout timer
    responseTimer->start(4000); // 4 Sec timer

    QByteArray command;

    command.append(0xff); //1
    command.append(0x0a); //2
    command.append(0xff); //3


    qDebug() << "Start Command cmd sent : " + hexBytes(command);
    writeToNotes("Start Command cmd sent : " + hexBytes(command));


    emit sendMsgId(0x01);
    serialObj->writeData(command);
}

void MainWindow::recvLivePlotData(QByteArray &recvData)
{
    // Check if recvData has the correct size
    if (recvData.size() != 6) {
        qWarning() << "Invalid data size, expected 6 bytes, got: " << recvData.size();
        return;
    }

    QVector<double> xValues; // Temporary X-axis values (sample numbers)
    QVector<double> yValues; // Temporary Y-axis values (scaled values)

    // Process recvData to extract 6 uint16 values in MSB order
    for (int i = 0; i < recvData.size()-4; i += 2) {
        // Extract MSB and LSB from the QByteArray
        uint8_t msb = static_cast<uint8_t>(recvData.at(i));
        uint8_t lsb = static_cast<uint8_t>(recvData.at(i + 1));

        // Combine MSB and LSB into a uint16 value
        uint16_t value = (msb << 8) | lsb;

        // Scale the value using the formula
        double scaledValue = 2.5 - ((value * 1.5259) / 10000.0);

        // Populate x and y values
        xValues.append(sampleNumber++); // Increment sample number for each point
        yValues.append(scaledValue);
    }

    // Append the current chunk of data to the accumulated data
    allXValues.append(xValues);
    allYValues.append(yValues);

    // Update the existing graph with the accumulated data
    ui->customPlot_chLive1->graph(0)->setData(allXValues, allYValues);

    // Adjust the x and y axis ranges dynamically
    ui->customPlot_chLive1->xAxis->setRange(0, sampleNumber); // Use the last sample number
    ui->customPlot_chLive1->yAxis->setRange(*std::min_element(allYValues.begin(), allYValues.end()),
                                            *std::max_element(allYValues.begin(), allYValues.end()));

    // Enable zooming and panning
    ui->customPlot_chLive1->setInteraction(QCP::iRangeZoom, true);       // Enable zooming
    ui->customPlot_chLive1->setInteraction(QCP::iRangeDrag, true);       // Enable panning

    // Repaint the plot
    ui->customPlot_chLive1->replot();
    qInfo() << "Live plot updated with 3 points.";
}


void MainWindow::on_pushButton_getPower_clicked()
{
    // Start the timeout timer
    responseTimer->start(2500); // 2.5 Sec timer

    QByteArray command;

    command.append(0x47); //1
    command.append(0x01); //2

    quint8 checkSum = calculateChecksum(command); //3

    //checksum
    command.append(checkSum); //total 3 bytes


    qDebug() << "Power Card Data cmd sent : " + hexBytes(command);
    writeToNotes("Power Card Data cmd sent : " + hexBytes(command));


    emit sendMsgId(0x02);
    serialObj->writeData(command);
}

void MainWindow::receivePowerData(QVector<float> recvPowerData)
{
    ui->doubleSpinBox_28->setValue(recvPowerData[0]);
    ui->doubleSpinBox_15->setValue(recvPowerData[1]);
    ui->doubleSpinBox_neg15->setValue(recvPowerData[2]);
    ui->doubleSpinBox_ext10->setValue(recvPowerData[3]);
    ui->doubleSpinBox_5->setValue(recvPowerData[4]);
    ui->doubleSpinBox_neg5->setValue(recvPowerData[5]);
    ui->doubleSpinBox_3p3->setValue(recvPowerData[6]);

    // Default stylesheet
    QString defaultStyleSheet = R"(
                                QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
                                width: 0px;
                                }
                                QDoubleSpinBox {
                                background-color: rgb(255, 255, 0);
                                border-radius: 10px;
                                }
                                )";

    // Green blink stylesheet
    QString greenBlinkStyleSheet = R"(
                                   QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
                                   width: 0px;
                                   }
                                   QDoubleSpinBox {
                                   background-color: rgb(0, 255, 0);
                                   border-radius: 10px;
                                   }
                                   )";

    // Helper lambda to blink the spin box
    auto blinkSpinBox = [&](QDoubleSpinBox* spinBox) {
        // Set green background
        spinBox->setStyleSheet(greenBlinkStyleSheet);

        // Revert to default style after 300 ms
        QTimer::singleShot(300, spinBox, [spinBox, defaultStyleSheet]() {
            spinBox->setStyleSheet(defaultStyleSheet);
        });
    };

    // Blink all spin boxes
    blinkSpinBox(ui->doubleSpinBox_28);
    blinkSpinBox(ui->doubleSpinBox_15);
    blinkSpinBox(ui->doubleSpinBox_neg15);
    blinkSpinBox(ui->doubleSpinBox_ext10);
    blinkSpinBox(ui->doubleSpinBox_5);
    blinkSpinBox(ui->doubleSpinBox_neg5);
    blinkSpinBox(ui->doubleSpinBox_3p3);
}
