#include "TestClient.h"

#include <QCoreApplication>
#include <QtEndian>

static constexpr char
    END_SLIP_OCTET  = 0xC0,
    END_SUBS_OCTET  = 0xDC,
    ESC_SLIP_OCTET  = 0xDB,
    ESC_SUBS_OCTET  = 0xDD;

static constexpr int MIN_FRAME_SIZE = sizeof(quint8) + sizeof(quint16) + 1;

static const quint16 _crc_ccitt_lut[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108,
    0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
    0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee,
    0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
    0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d,
    0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5,
    0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
    0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
    0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
    0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13,
    0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
    0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e,
    0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1,
    0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
    0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
    0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
    0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882,
    0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e,
    0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
    0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d,
    0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
    0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static inline void _encodeSymbol(QByteArray &buffer, char ch) Q_DECL_NOTHROW
{
    switch (ch)
    {
        case END_SLIP_OCTET:
            buffer.append(ESC_SLIP_OCTET);
            buffer.append(END_SUBS_OCTET);
            break;

        case ESC_SLIP_OCTET:
            buffer.append(ESC_SLIP_OCTET);
            buffer.append(ESC_SUBS_OCTET);
            break;

        default:
            buffer.append(ch);
    }
}

TestClient::TestClient(QSettings *settings, SessionManager *session, QObject *parent)
    : QObject(parent),
      _settings(settings),
      _session(session),
      _serial(this)
{
    connect(&_serial, &QSerialPort::readyRead, this, &TestClient::onSerialPortReadyRead);
    connect(&_serial, &QSerialPort::errorOccurred, this, &TestClient::onSerialPortErrorOccurred);

    //Slip commands

    connect(this, &TestClient::checkBoardCurrent, [this](){_mode = slipMode;});
    connect(this, &TestClient::checkBoardCurrent, this, &TestClient::on_checkBoardCurrent);

    connect(this, &TestClient::sendRailtestCommand, this, &TestClient::on_sendRailtestCommand);
    connect(this, &TestClient::reset, this, &TestClient::on_reset);

    connect(this, &TestClient::switchSWD, [this](){_mode = slipMode;});
    connect(this, &TestClient::switchSWD, this, &TestClient::on_switchSWD);

    connect(this, &TestClient::powerOn, [this](){_mode = slipMode;});
    connect(this, &TestClient::powerOn, this, &TestClient::on_powerOn);

    connect(this, &TestClient::powerOff, [this](){_mode = slipMode;});
    connect(this, &TestClient::powerOff, this, &TestClient::on_powerOff);

    connect(this, &TestClient::readDIN, this, &TestClient::on_readDIN);
    connect(this, &TestClient::setDOUT, this, &TestClient::on_setDOUT);
    connect(this, &TestClient::clearDOUT, this, &TestClient::on_clearDOUT);

    connect(this, &TestClient::readCSA, [this](){_mode = slipMode;});
    connect(this, &TestClient::readCSA, this, &TestClient::on_readCSA);

    connect(this, &TestClient::readAIN, this, &TestClient::on_readAIN);
    connect(this, &TestClient::configDebugSerial, this, &TestClient::on_configDebugSerial);
    connect(this, &TestClient::DaliOn, this, &TestClient::on_DaliOn);
    connect(this, &TestClient::DaliOff, this, &TestClient::on_DaliOff);
    connect(this, &TestClient::readDaliADC, this, &TestClient::on_readDaliADC);
    connect(this, &TestClient::readDinADC, this, &TestClient::on_readDinADC);
    connect(this, &TestClient::read24V, this, &TestClient::on_read24V);
    connect(this, &TestClient::read3V, this, &TestClient::on_read3V);
    connect(this, &TestClient::readTemperature, this, &TestClient::on_readTemperature);

    //Rail test commands

//    connect(this, &TestClient::waitCommandPrompt, [this](){_mode = railMode;});
//    connect(this, &TestClient::waitCommandPrompt, &_rail, &RailtestClient::on_waitCommandPrompt);

//    connect(this, &TestClient::syncCommand, &_rail, &RailtestClient::on_syncCommand);

    connect(this, &TestClient::readChipId, [this](){_mode = railMode;});
    connect(this, &TestClient::readChipId, this, &TestClient::on_readChipId);

//    connect(this, &TestClient::testRadio, &_rail, &RailtestClient::on_testRadio);
//    connect(this, &TestClient::testAccelerometer, &_rail, &RailtestClient::on_testAccelerometer);
//    connect(this, &TestClient::testLightSensor, &_rail, &RailtestClient::on_testLightSensor);
//    connect(this, &TestClient::testDALI, &_rail, &RailtestClient::on_testDALI);
//    connect(this, &TestClient::testGNSS, &_rail, &RailtestClient::on_testGNSS);
}

TestClient::~TestClient()
{

}

void TestClient::setLogger(Logger *logger)
{
    _logger = logger;
}

void TestClient::setDutsNumbers(QList<int> numbers)
{
    _dutsNumbers = numbers;
}

void TestClient::setPort(const QString &name, qint32 baudRate, QSerialPort::DataBits dataBits,
    QSerialPort::Parity parity, QSerialPort::StopBits stopBits,
    QSerialPort::FlowControl flowControl)
{
    if (_serial.isOpen())
        _serial.close();
    _serial.setPortName(name);

    bool res = _serial.setBaudRate(baudRate)
        && _serial.setDataBits(dataBits)
        && _serial.setParity(parity)
        && _serial.setStopBits(stopBits)
        && _serial.setFlowControl(flowControl);

    if (!res)
        _logger->logError(_serial.errorString());
    open();
}

void TestClient::open()
{
    if (_serial.isOpen())
    {
        _serial.close();
    }

    if (!_serial.open(QSerialPort::ReadWrite))
        _logger->logError(_serial.errorString());
    else
    {
        readCSA(0);
    }
}

void TestClient::close()
{
    if (_serial.isOpen())
    {
        _serial.flush();
        _serial.close();
    }
}

void TestClient::onSerialPortReadyRead()
{
    switch (_mode)
    {
    case idleMode:
        _serial.readAll();
        break;

    case slipMode:
    case railMode:
        if(_serial.portName() != "COM6")
            processResponsePacket();
        break;
    }
}

void TestClient::onSerialPortErrorOccurred(QSerialPort::SerialPortError errorCode)
{
    if (errorCode != QSerialPort::NoError)
        _logger->logError("Serial port error occurred " + QString().setNum(errorCode) + " on port " + _serial.portName());
}

void TestClient::on_sendRailtestCommand(int channel, const QByteArray &cmd, const QByteArray &args)
{
    _mode = railMode;
    _syncCommand = cmd;
    _syncReplies.clear();
    sendFrame(channel, cmd + " " + args + "\r\n\r\n");
}

void TestClient::on_reset()
{
    MB_Packet_t pkt;

    pkt.type = qToBigEndian<uint16_t>(MB_SYSTEM_RESET);
    pkt.sequence = MB_SYSTEM_RESET;
    pkt.dataLen = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_switchSWD(int DUT)
{
   _logger->logDebug(QString("switchSWD called with arg %1").arg(DUT));
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t dut;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_SWITCH_SWD);
    pkt.h.sequence = 1;
    pkt.h.dataLen = 1;
    pkt.dut = DUT;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_powerOn(int DUT)
{
    _logger->logDebug(QString("on_powerOn called for %1").arg(DUT));
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_SWITCH_POWER);
    pkt.h.sequence = 2;
    pkt.h.dataLen = 2;
    pkt.dut = DUT;
    pkt.state = 1;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_powerOff(int DUT)
{
    _logger->logDebug(QString("on_powerOff called for %1").arg(DUT));
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_SWITCH_POWER);
    pkt.h.sequence = 3;
    pkt.h.dataLen = 2;
    pkt.dut = DUT;
    pkt.state = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readDIN(int DUT, int DIN)
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                din;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_DIN);
    pkt.h.sequence = 4;
    pkt.h.dataLen = 2;
    pkt.dut = DUT;
    pkt.din = DIN;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_setDOUT(int DUT, int DOUT)
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                dout,
                state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_WRITE_DOUT);
    pkt.h.sequence = 5;
    pkt.h.dataLen = 3;
    pkt.dut = DUT;
    pkt.dout = DOUT;
    pkt.state = 1;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_clearDOUT(int DUT, int DOUT)
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                dout,
                state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_WRITE_DOUT);
    pkt.h.sequence = 6;
    pkt.h.dataLen = 3;
    pkt.dut = DUT;
    pkt.dout = DOUT;
    pkt.state = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readCSA(int gain)
{
    _logger->logDebug(QString("on_readCSA called for board on %1").arg(_serial.portName()));
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t gain;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_CSA);
    pkt.h.sequence = 7;
    pkt.h.dataLen = 1;
    pkt.gain = gain;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readAIN(int DUT, int AIN, int gain)
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                ain,
                gain;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_ANALOG);
    pkt.h.sequence = 8;
    pkt.h.dataLen = 3;
    pkt.dut = DUT;
    pkt.ain = AIN;
    pkt.gain = gain;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_configDebugSerial(int DUT, int baudRate, unsigned char bits, unsigned char parity, unsigned char stopBits)
{
    MB_ConfigDutDebug_t pkt;

    pkt.header.type = qToBigEndian<uint16_t>(MB_CONFIG_DUT_DEBUG);
    pkt.header.sequence = 9;
    pkt.header.dataLen = sizeof(MB_ConfigDutDebug_t) - sizeof(MB_Packet_t);
    pkt.dutIndex = DUT;
    pkt.baudRate = qToBigEndian(baudRate);
    pkt.bits = bits;
    pkt.parity = parity;
    pkt.stopBits = stopBits;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_DaliOn()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_SWITCH_DALI);
    pkt.h.sequence = 10;
    pkt.h.dataLen = 1;
    pkt.state = 1;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_DaliOff()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t state;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_SWITCH_DALI);
    pkt.h.sequence = 11;
    pkt.h.dataLen = 1;
    pkt.state = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readDaliADC()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_DALI_ADC);
    pkt.h.sequence = 12;
    pkt.h.dataLen = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readDinADC(int DUT, int DIN)
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
        uint8_t
                dut,
                din;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_DIN_ADC);
    pkt.h.sequence = 13;
    pkt.h.dataLen = 2;
    pkt.dut = DUT;
    pkt.din = DIN;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_read24V()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_ADC_24V);
    pkt.h.sequence = 14;
    pkt.h.dataLen = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_read3V()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_ADC_3V);
    pkt.h.sequence = 15;
    pkt.h.dataLen = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readTemperature()
{
#pragma pack (push, 1)
    struct Pkt
    {
        MB_Packet_t h;
    };
#pragma pack (pop)

    Pkt pkt;

    pkt.h.type = qToBigEndian<uint16_t>(MB_READ_ADC_TEMP);
    pkt.h.sequence = 16;
    pkt.h.dataLen = 0;
    sendFrame(0, QByteArray((char*)&pkt, sizeof(pkt)));
}

void TestClient::on_readChipId(int dut)
{
    _currentCommand = readChipIdCommand;
    sendRailtestCommand(dut, "getmemw", "0x0FE081F0 2");
}

void TestClient::sendFrame(int channel, const QByteArray &frame) Q_DECL_NOTHROW
{
    QByteArray encodedBuffer;
    quint16 frameCrc = 0xFFFF;

    // Write UART wake up symbols and SLIP frame start.
    encodedBuffer.reserve((frame.size() + 3) * 2 + 2);
    encodedBuffer.append(END_SLIP_OCTET);

    // Write escaped channel number.
    quint8 index = channel ^ (frameCrc >> 8);

    _encodeSymbol(encodedBuffer, channel);
    frameCrc = _crc_ccitt_lut[index] ^ (frameCrc << 8);

    // Write escaped frame and calculate CRC.
    foreach (char ch, frame)
    {
        quint8 index = (quint8)ch ^ (frameCrc >> 8);

        _encodeSymbol(encodedBuffer, ch);
        frameCrc = _crc_ccitt_lut[index] ^ (frameCrc << 8);
    }

    // Write escaped CRC.
    _encodeSymbol(encodedBuffer, frameCrc >> 8);
    _encodeSymbol(encodedBuffer, frameCrc & 0xFF);

    // Write SLIP frame end.
    encodedBuffer.append(END_SLIP_OCTET);

    // Write encoded frame to serial port.
    _serial.write(encodedBuffer);
}

void TestClient::processResponsePacket()
{
    while (_serial.bytesAvailable())
    {
        QByteArray buffer = _serial.readAll();

        foreach (char ch, buffer)
        {
            if (ch == END_SLIP_OCTET)
                if (_frameStarted)
                {
                    if (_recvBuffer.size() >= MIN_FRAME_SIZE)
                    {
                        decodeFrame();
                        _frameStarted = false;
                    }
                    _recvBuffer.clear();
                }
                else
                {
                    _frameStarted = true;
                    _recvBuffer.clear();
                }
            else
                if (_frameStarted)
                    _recvBuffer.append(ch);
        }
    }
}

void TestClient::decodeFrame() Q_DECL_NOTHROW
{
    QByteArray decodedBuffer;
    static QByteArray railReply;

    // Write unescaped buffer.
    decodedBuffer.reserve(_recvBuffer.size());
    for (int i = 0; i < _recvBuffer.size(); ++i)
    {
        char ch = _recvBuffer.at(i);

        if (ch == ESC_SLIP_OCTET)
        {
            ++i;
            if (i >= _recvBuffer.size())
            {
                _logger->logError("SLIP. Decode frame. Unfinished escape sequence.");
                return;
            }
            switch (_recvBuffer.at(i))
            {
                case END_SUBS_OCTET:
                    decodedBuffer.append(END_SLIP_OCTET);
                    break;

                case ESC_SUBS_OCTET:
                    decodedBuffer.append(ESC_SLIP_OCTET);
                    break;

                default:
                    _logger->logError("SLIP. Decode frame. Invalid escape sequence.");
                    return;
            }
        }
        else
            decodedBuffer.append(ch);
    }
    if (decodedBuffer.size() < MIN_FRAME_SIZE)
    {
        _logger->logError("SLIP. Decode frame. Frame too short.");
        return;
    }

    // Calculate CRC.
    int frameSize = decodedBuffer.size() - 2;
    quint16
        frameCrc = 0xFFFF,
        bufferCrc = (quint8)decodedBuffer.at(frameSize + 1) | ((quint16)decodedBuffer.at(frameSize) << 8);

    for (int i = 0; i < frameSize; ++i)
    {
        quint8 index = (quint8)decodedBuffer.at(i) ^ (frameCrc >> 8);

        frameCrc = _crc_ccitt_lut[index] ^ (frameCrc << 8);
    }

    // Check CRC.
    if (frameCrc != bufferCrc)
    {
        _logger->logError("SLIP. Decode frame. Invalid Frame CRC.");
        return;
    }

    // Frame received successfully.

    QByteArray frame = decodedBuffer.mid(1, frameSize - 1);

//    if(decodedBuffer.at(0) == 2 && _serial.portName() == "COM6")
//        return;

    switch(_mode)
    {
    case slipMode:
        onSlipPacketReceived(decodedBuffer.at(0), frame);
        break;

    case railMode:
        railReply += frame;
        if(frame.contains("> "))
        {
            onSlipPacketReceived(decodedBuffer.at(0), railReply);
            railReply.clear();
        }
        break;
    }

    //onSlipPacketReceived(decodedBuffer.at(0), decodedBuffer.mid(1, frameSize - 1));
}

void TestClient::onSlipPacketReceived(quint8 channel, QByteArray frame) noexcept
{
    switch (channel)
    {
        case 0:
            if (frame.size() >= (int)sizeof(MB_Packet_t))
            {
                MB_Packet_t *pkt = (MB_Packet_t*)frame.data();

                pkt->type = qFromBigEndian(pkt->type);
                switch (pkt->type)
                {
                    case MB_STARTUP:
                        _logger->logInfo("Startup event.");
                        break;

                    case MB_GENERAL_RESULT:
                        {
                            MB_GeneralResult_t *gr = (MB_GeneralResult_t*)pkt;

                            gr->errorCode = qFromBigEndian(gr->errorCode);
                            switch (gr->header.sequence)
                            {
                            case 1:
                                _logger->logDebug(QString("Reply to switchSWD command to board on %1: %2").arg(_serial.portName()).arg(gr->errorCode));
                                break;

                            case 2:
                                _logger->logDebug(QString("Reply to powerOn command to %1: %2").arg(_serial.portName()).arg(gr->errorCode));
                                break;

                            case 3:
                                _logger->logDebug(QString("Reply to powerOff command to %1: %2").arg(_serial.portName()).arg(gr->errorCode));
                                break;

                            case 7:
                                _CSA = gr->errorCode;
                                _logger->logDebug(QString("Reply to readCSA command to %1: %2").arg(_serial.portName()).arg(gr->errorCode));
                                break;
                            }
                        }
                        break;

                    case MB_ASYNC_EVENT:
                        {
                            MB_Event_t *evt = (MB_Event_t*)pkt;

                            evt->eventCode = qFromBigEndian(evt->eventCode);
                            _logger->logInfo(QString("EVENT: code=%2.").arg(evt->eventCode));
                        }
                        break;
                }
            }
            break;

        case 1:
        case 2:
        case 3:
            processFrameFromRail(frame);
            break;
    }

    _mode = idleMode;
}

void TestClient::decodeRailtestReply(const QByteArray &reply)
{
    if (reply.startsWith("{{(") && reply.endsWith("}}"))
    {
        int idx = reply.indexOf(")}");

        if (idx == -1)
            return;

        auto name = reply.mid(3, idx - 3);
        auto params = reply.mid(idx + 2, reply.length() - idx - 3).split('}');
        QVariantMap decodedParams;

        foreach (auto param, params)
            if (param.startsWith('{'))
            {
                idx = param.indexOf(':');
                if (idx != -1)
                    decodedParams.insert(param.mid(1, idx - 1), param.mid(idx + 1));
            }

        if (_syncCommand == name)
            _syncReplies.append(decodedParams);
        else
            //emit replyReceived(name, decodedParams);

        return;
    }

    if (reply.startsWith("{{") && reply.endsWith("}}"))
    {
        auto params = reply.mid(1, reply.length() - 2).split('}');
        QVariantList decodedParams;

        foreach (auto param, params)
            if (param.startsWith('{'))
                decodedParams.append(param.mid(1));

        if (!_syncCommand.isEmpty())
            _syncReplies.push_back(decodedParams);

        return;
    }
}

void TestClient::processFrameFromRail(QByteArray frame)
{
    int idx = frame.indexOf("\r\n");
    frame = frame.mid(idx + 2); // Delete first line
    frame = frame.mid(1); // Delete '#' symbol

    QString frameString(frame);

    auto replyStringList = frameString.split("\r\n");
    QList<QByteArray> replyList;
    for (int i = 1; i < replyStringList.size() - 2; i++)
    {
        replyList.push_back(replyStringList.at(i).toLocal8Bit());
    }

    for(auto & reply : replyList)
    {
        decodeRailtestReply(reply);
    }

    switch (_currentCommand)
    {
    case readChipIdCommand:
        auto
            llo = _syncReplies.at(0).toList(),
            lhi = _syncReplies.at(1).toList();

        auto
           lo = llo.at(1).toByteArray().trimmed(),
           hi = lhi.at(1).toByteArray().trimmed();

        qDebug() << (hi.mid(2) + lo.mid(2)).toUpper();

        break;
    }
}

void TestClient::on_checkBoardCurrent()
{
    readCSA(0);
    //delay(30);

    if((_CSA < 140) && (_CSA > 110))
        _logger->logSuccess("Test board on " + _serial.portName() + " works normally. Current: " + QString().setNum(_CSA));
    else
        _logger->logError("Test board on " + _serial.portName() + " do not works normally!. Current: " + QString().setNum(_CSA));
}

void TestClient::delay(int msec)
{
    QTime expire = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() <= expire)
    {
        QCoreApplication::processEvents();
    }
}
