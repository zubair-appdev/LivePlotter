#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <serialporthandler.h>
#include <QMessageBox>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include "qcustomplot.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Forward declaration of serialPortHandler
class serialPortHandler;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void refreshPorts();

    //For Saving log Data
    void resetLogFile();
    static void writeToNotes(const QString &data);
    void initializeLogFile();
    void closeLogFile();

    quint8 calculateChecksum(const QByteArray &data);
    QString hexBytes(QByteArray &cmd);

    void initializePlot();


private slots:
    void onPortSelected(const QString &portName);

    void portStatus(const QString&);

    //response time handling

    void handleTimeout();

    void onDataReceived();

    void on_pushButton_start_clicked();

    void recvLivePlotData(QByteArray &recvData);

    void on_pushButton_getPower_clicked();

    void receivePowerData(QVector<float> recvPowerData);

signals:
    void sendMsgId(quint8 id);

private:
    Ui::MainWindow *ui;
    serialPortHandler *serialObj;

    //Log handling
    static QFile logFile;
    static QTextStream logStream;

    //Response Time waiting timer
    QTimer *responseTimer = nullptr; // Timer to track response timeout

    // Initialize sample number
    int sampleNumber = 0;
    QVector<double> allXValues;
    QVector<double> allYValues;

};
#endif // MAINWINDOW_H
