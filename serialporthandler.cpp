#include "serialporthandler.h"
int serialPortHandler::processedBytes = 0;

serialPortHandler::serialPortHandler(QObject *parent) : QObject(parent)
{
    serial = new QSerialPort;
    connect(serial, &QSerialPort::readyRead, this, &serialPortHandler::readData);

}

serialPortHandler::~serialPortHandler()
{
    delete serial;
}

QStringList serialPortHandler::availablePorts()
{
    QStringList ports;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ports<<info.portName();
    }
    return ports;
}

void serialPortHandler::setPORTNAME(const QString &portName)
{
    buffer.clear();

    if(serial->isOpen())
    {
        serial->close();
    }

    serial->setPortName(portName);
    serial->setBaudRate(921600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);


    if(!serial->open(QIODevice::ReadWrite))
    {
        qDebug()<<"Failed to open port"<<serial->portName();
        emit portOpening("Failed to open port "+serial->portName());
    }
    else
    {
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 921600";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 921600");
    }
}

float serialPortHandler::convertBytesToFloat(const QByteArray &data)
{
    if(data.size() != 4)
    {
        qDebug()<<"Insuffient data to convert into float";
    }

    // Assuming little-endian format
    QByteArray floatBytes = data;
    std::reverse(floatBytes.begin(), floatBytes.end()); // Convert to big-endian if needed

    float value;
    memcpy(&value, floatBytes.constData(), sizeof(float));
    return value;
}

quint8 serialPortHandler::chkSum(const QByteArray &data)
{
    // Ensure the QByteArray has at least two bytes (data + checksum)
    if (data.size() < 2) {
        throw std::invalid_argument("Data size must be at least 2 for checksum calculation.");
    }

    // Initialize checksum to 0
    quint8 checksum = 0;

    // Perform XOR for all bytes except the last one
    for (int i = 0; i < data.size() - 1; ++i) {
        checksum ^= static_cast<quint8>(data[i]);
    }

    qDebug()<<hex<<checksum<<"DEBUG_CHKSUM";
    return checksum;
}

void serialPortHandler::readData()
{
    qDebug()<<"------------------------------------------------------------------------------------";
    emit portOpening("------------------------------------------------------------------------------------");
    QByteArray ResponseData;

    // Read data from the serial port
    if (serial->bytesAvailable() == 0) {
        qWarning() << "No bytes available from serial port";
        return;  // Early return if no data is available
    }

    // Create a QMutexLocker to manage the mutex
    QMutexLocker locker(&bufferMutex); // Lock the mutex


    if (serial->bytesAvailable() < std::numeric_limits<int>::max()) {
        buffer.append(serial->readAll()); // Append only if it won't exceed max size
        if (!buffer.isEmpty()) {
            emit dataReceived(); // Signal data has been received
        }
    } else {
        qWarning() << "Attempt to append too much data to QByteArray!";
        return;
    }


    //Direct taking msgId from mainWindow
    quint8 msgId = id;
    //powerId to avoid that warning QByteRef calling out of bond error
    quint8 powerId = 0x00;

    if(msgId == 0x01)
    {
        qDebug()<<buffer.size()<<" :size";

    }
    else
    {
        qDebug()<<buffer.toHex()<<" Raw buffer data";
        qDebug()<<buffer.size()<<" :size";
    }


    if (msgId == 0x01) {
        qDebug() << "msgId:" << hex << msgId;

        // Loop through unprocessed data in the buffer
        while (buffer.size() - processedBytes >= 3)
        {
            QByteArray chunk = buffer.mid(processedBytes); // Extract unprocessed portion

            if (chunk.size() >= 3 && chunk.right(3) == QByteArray::fromHex("ffddff"))
            {
                powerId = 0x01;
                ResponseData = chunk.right(3);
                executeWriteToNotes("Start Command received bytes check: " + ResponseData.toHex());
                processedBytes = 0;
                break;
            }
            else if (chunk.size() >= 6)
            {
                powerId = 0x01;
                ResponseData = chunk.left(6); // Extract the first 6 bytes
                processedBytes += 6; // Increment processedBytes offset
                executeWriteToNotes("Start Command 6 bytes received: " + QString::number(ResponseData.size()));

                // Emit data for live plotting
                emit plotLiveData(ResponseData);
            }
            else {
                // Not enough data, wait for more bytes
                executeWriteToNotes("Start Command received Chunk Size " + QString::number(chunk.size()));
                break;
            }
        }

    }

    else if(msgId == 0x02)
    {
        qDebug() << "msgId:" <<hex<<msgId;

        if(buffer.size() == 17
                && static_cast<unsigned char>(buffer[0]) == 0x54
                && static_cast<unsigned char>(buffer[1]) == 0x01
                && static_cast<unsigned char>(buffer[16]) == chkSum(buffer))
        {
            powerId = 0x02;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Power Card Data received bytes check: "+ResponseData.toHex());
        }
        else
        {
            executeWriteToNotes("Required 17 bytes Received bytes: "+QString::number(buffer.size())
                                +" "+buffer.toHex());
        }

    }
    else
    {
        //do nothing
        qDebug()<<"do nothing not a specified size/unknown msgId";
        executeWriteToNotes("Fatal Error 404");
    }


    switch(powerId)
    {
    case 0x01:
    {
        if(ResponseData.size() == 3)
        {
            QMessageBox *msgBox = new QMessageBox(nullptr);
            msgBox->setWindowTitle("Completed");
            msgBox->setText("Plot Completed Successfully");
            msgBox->setStandardButtons(QMessageBox::Ok);
            msgBox->setAttribute(Qt::WA_DeleteOnClose); // Automatically delete when closed
            msgBox->setModal(false); // Set to non-modal
            msgBox->show();
        }
        else if(ResponseData.size() == 6)
        {
            emit plotLiveData(ResponseData);
        }
    }
        break;

    case 0x02:
    {

        // Assuming ResponseData and realData are defined and contain valid data.
        QByteArray realData = ResponseData.mid(2, 14);

        // Ensure that realData has exactly 14 bytes
        if (realData.size() != 14) {
            qWarning() << "Invalid data size. Expected 14 bytes.";
            return;
        }

        // Define variables to store the calculated values
        float pos28V = 0.0f, pos15V = 0.0f, neg15V = 0.0f, ext10V = 0.0f, pos5V = 0.0f, neg5V = 0.0f, pos3p3V = 0.0f;

        // Helper lambda to calculate the scaled value
        auto calculateScaledValue = [](quint16 rawValue) -> float {
            return ((rawValue * 20.48f) / 4095.0f - 10.24f) * 3;
        };

        // Helper lambda to calculate special scaled value
        auto calculateScaledSpecialValue = [](quint16 rawValue) -> float {
            return (rawValue * 20.48f) / 4095.0f - 10.24f;
        };

        // Extract and process each pair of bytes
        pos28V = calculateScaledValue(static_cast<quint16>((static_cast<unsigned char>(realData[0]) << 8) |
                                      static_cast<unsigned char>(realData[1])));
        pos15V = calculateScaledValue(static_cast<quint16>((static_cast<unsigned char>(realData[2]) << 8) |
                                      static_cast<unsigned char>(realData[3])));
        neg15V = calculateScaledValue(static_cast<quint16>((static_cast<unsigned char>(realData[4]) << 8) |
                                      static_cast<unsigned char>(realData[5])));
        ext10V = calculateScaledValue(static_cast<quint16>((static_cast<unsigned char>(realData[6]) << 8) |
                                      static_cast<unsigned char>(realData[7])));
        pos5V = calculateScaledSpecialValue(static_cast<quint16>((static_cast<unsigned char>(realData[8]) << 8) |
                                            static_cast<unsigned char>(realData[9])));
        neg5V = calculateScaledSpecialValue(static_cast<quint16>((static_cast<unsigned char>(realData[10]) << 8) |
                                            static_cast<unsigned char>(realData[11])));
        pos3p3V = calculateScaledSpecialValue(static_cast<quint16>((static_cast<unsigned char>(realData[12]) << 8) |
                                              static_cast<unsigned char>(realData[13])));

        // Output the results
        qDebug() << "pos28V:" << pos28V;
        qDebug() << "pos15V:" << pos15V;
        qDebug() << "neg15V:" << neg15V;
        qDebug() << "ext10V:" << ext10V;
        qDebug() << "pos5V:" << pos5V;
        qDebug() << "neg5V:" << neg5V;
        qDebug() << "pos3p3V:" << pos3p3V;

        QVector<float> powerData;
        powerData.append(pos28V);
        powerData.append(pos15V);
        powerData.append(neg15V);
        powerData.append(ext10V);
        powerData.append(pos5V);
        powerData.append(neg5V);
        powerData.append(pos3p3V);

        emit sendPowerData(powerData);


    }
        break;

    default:
    {
        qDebug() << "Unknown powerId: " <<hex << powerId << " with data: " << ResponseData.size();
    }
    }

}

void serialPortHandler::recvMsgId(quint8 id)
{
    qDebug() << "Received id:" <<hex<< id;
    this->id = id;
    buffer.clear();
    processedBytes = 0; //To ensure before clicking start processedBytes are re-setting.
}
