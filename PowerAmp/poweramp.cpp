#include "math.h"
#include "poweramp.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QTime>
#include <QSettings>

Q_LOGGING_CATEGORY(PA,"POWER AMPLIFIER")

PowerAmp::PowerAmp(QObject *parent) : QObject(parent)
{
    initialize();

    if (exist())
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Connected to the port of "
                      << m_serialPort->portName() << ".";
        connect(m_serialPort,SIGNAL(error(QSerialPort::SerialPortError)),
                this,SLOT(handleError(QSerialPort::SerialPortError)));
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Cannot find a serial port to connect.";
        //  TODO
        //  Handle this situation
    }
}

PowerAmp::~PowerAmp()
{
    if (exist())
    {
        close();
        delete m_serialPort;
    }
    qCDebug(PA()) << PA().categoryName()
                  << "Power amplifiers are closed.";
}

void PowerAmp::initialize()
{
    qCDebug(PA()) << PA().categoryName() << "Initialization...";
    for (int i=0;i<2;i++)
    {
        //  Generate a random id of PA channel
        int ranId = genRanId();        
        readConfig();
        m_serialPort = new QSerialPort(m_portName);

        if (resetSingle(ranId))
        {
            qCDebug(PA()) << PA().categoryName() << "Successfully initialized.";
            return;
        }
    }

    qCDebug(PA()) << PA().categoryName() << "The port name was changed.";

    for (int i=0;i<2;i++)
    {
        setPort();
    }
}

void PowerAmp::setPort()
{
    //  Generate a random id of PA channel
    int ranId = genRanId();
    QByteArray baId,baVolt,baCheck,baSend;
    //  Generate the bytes to send
    baId = computeBaId(ranId);
    baVolt = computeBaVolt(RESET,1);
    baCheck = computeBaCheck(baId,baVolt);

    baSend += baId;
    baSend += baVolt;
    baSend += baCheck;

    QList<QSerialPortInfo> serialPortInfoList = QSerialPortInfo::availablePorts();
    foreach (const QSerialPortInfo &serialPortInfo,serialPortInfoList)
    {
        m_serialPort = new QSerialPort(serialPortInfo);
        readBack(baId,baVolt,baCheck);

        if (!m_baRead.isEmpty())
        {
            QByteArray baRead = m_baRead;
            m_baRead.clear();
            if (checkReceivedBytes(baRead,baSend))
            {
                m_portName = serialPortInfo.portName();
                writeConfig();
                qCDebug(PA()) << PA().categoryName() << "Successfully initialized.";
                return;
            }
        }
        delete m_serialPort;        
    }
    m_serialPort = NULL;
    qCDebug(PA()) << PA().categoryName() << "Failed to initialize.";
}

void PowerAmp::readConfig()
{
    QSettings* settings = new QSettings(CONFIG_PATH,QSettings::IniFormat);
    m_portName = settings->value("PowerAmp/port").toString();
    delete settings;
}

void PowerAmp::writeConfig()
{
    QSettings* settings = new QSettings(CONFIG_PATH,QSettings::IniFormat);
    settings->setValue("PowerAmp/port",m_portName);
    delete settings;
}

int PowerAmp::validateId(int id)
{
    if ( 0 <= id && id <= DEV_COUNT_MAX )
        return id;
    else
        return -1;
}

VOLT PowerAmp::validateVolt(VOLT volt)
{
    if ( volt < 0 || volt > VOLT_MAX )
        volt = -1;
    return volt;
}

QByteArray PowerAmp::computeBaId(int id)
{
    QByteArray baId;
    if (validateId(id) >= 0)
    {
        baId.resize(2);
        baId[0] = 0x80 + id / 128;
        baId[1] = id % 128;
    }

    return baId;
}

QByteArray PowerAmp::computeBaVolt(ACTION action, VOLT volt)
{
    QByteArray baVolt;

    switch (action) {
    case RESET:
        baVolt.resize(2);
        baVolt[0] = 0x00;
        baVolt[1] = 0x00;
        break;
    case START:
        if (validateVolt(volt) >= 0)
        {
            int intVolt = (int)ceil(validateVolt(volt) * 10);
            baVolt.resize(2);
            baVolt[0] = 0x40 + intVolt / 128;
            baVolt[1] = intVolt % 128;
        }
        break;
    case ECHO_VOLT:
        baVolt.resize(2);
        baVolt[0] = 0x20;
        baVolt[1] = 0x00;
        break;
    case ECHO_TEMP:
        baVolt.resize(2);
        baVolt[0] = 0x10;
        baVolt[1] = 0x00;
    default:
        break;
    }

    return baVolt;
}

QByteArray PowerAmp::computeBaCheck(QByteArray baId, QByteArray baVolt)
{
    QByteArray baCheck;
    if (baId.isEmpty() || baVolt.isEmpty())
    {        
    }else
    {
        char sum = baId[1] + baId[0] + baVolt[1] + baVolt[0];
        baCheck.resize(1);
        baCheck[0] = (0x7F & sum);
    }

    return baCheck;
}

bool PowerAmp::checkReceivedBytes(QByteArray baReceive, QByteArray baSend)
{
    bool checked = false;
    bool validLength = (baReceive.size() == baSend.size());
    // test
//    qDebug() << "baSend: " << baSend.toHex();
//    qDebug() << "baReceive: " << baReceive.toHex();

    if (validLength)
    {
        checked = (baReceive[0] == (baSend[0] - 0x80));
        switch (baSend[2])
        {
        case 0x00: case 0x40:
            checked = ((checked || (baReceive[0] == baSend[0])) &&
                      (baReceive[1] == baSend[1]) &&
                      (baReceive[2] == baSend[2]) &&
                      (baReceive[3] == baSend[3]) &&
                      (baReceive[4] == baSend[4]));
            break;
        case 0x10: case 0x20:
            break;
        }
    }

    return checked;
}

VOLT PowerAmp::ba2volt(QByteArray baEcho)
{    
    int intVolt = (int)baEcho[2] * 128 + (int)baEcho[3];
    VOLT volt = double(intVolt) / double(10);
    return volt;
}

DEGREE PowerAmp::ba2temp(QByteArray baEcho)
{
    int intTemp = (int)baEcho[2] * 128 + (int)baEcho[3];
    DEGREE temp = double(intTemp) / double(10);
    return temp;
}

void PowerAmp::readBack(QByteArray baId, QByteArray baVolt, QByteArray baCheck)
{
    if (!baCheck.isEmpty())
    {
        if (open())
        {
            m_serialPort->write(baId+baVolt+baCheck);

            if (m_serialPort->waitForReadyRead(ECHO_PERIOD))
            {
                qDebug() << "readyRead signal emitted.";
                qDebug() << "bytesAvailable: " << m_serialPort->bytesAvailable();

                while(m_serialPort->bytesAvailable() != 5)
                {
                    if (m_serialPort->waitForReadyRead(ECHO_PERIOD))
                    {
                        //  Do nothing, wait
                        qDebug() << "readyRead signal emitted.";
                        qDebug() << "bytesAvailable: " << m_serialPort->bytesAvailable();
                    }else
                    {
                        qDebug() << "cannot emit readyRead signal.";
                        break;
                    }
                }
            }else
            {
                qDebug() << "cannot emit readyRead signal.";
            }

            if (m_serialPort->bytesAvailable())
            {
                m_baRead.append(m_serialPort->readAll());
            }
        }
    }
}

bool PowerAmp::startSingle(int id, VOLT volt)
{
    //  optimize: startAll, the same BaVolt, BaCheck with BaId
    bool success = false;
    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(START,volt);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    readBack(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
        {
            success = true;
        }
        m_baRead.clear();
    }
    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Started #" << id
                      << " with " << volt << "v successfully.";
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Failed to start #" << id
                      << " with " << volt << "v.";
    }

    return success;
}

bool PowerAmp::startAll(VOLT volt)
{
    bool success = false;
    QList<int> errorId;

    for (int id=1;id<=DEV_COUNT_MAX;id++)
    {
        int safeCounter = 0;
        while (true)
        {
            if (startSingle(id,volt))
                break;
            else
                safeCounter++;

            //  TODO
            if (safeCounter == 5)
            {
                errorId.append(id);
                break;
            }
        }
    }

    if (!errorId.isEmpty())
    {
        for(int i=0;i<errorId.size();i++)
        {
            if (echoVolt(errorId.at(i)) == -1)
            {
                m_errorId.append(errorId.at(i));
            }
        }
    }

    if (m_errorId.isEmpty())
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Started all the power amplifiers successfully.";
        success = true;
        emit actionCompleted();
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Failed to start all the power amplifiers.";
        qCDebug(PA()) << PA().categoryName()
                      << m_errorId.size() << "power amplifiers have error.";
        qCDebug(PA()) << PA().categoryName()
                      << "They are: " << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

void PowerAmp::startAll2(VOLT volt)
{
    QByteArray baId;
    baId[0] = 0x80;
    baId[1] = 0x00;

    QByteArray baVolt = computeBaVolt(START,volt);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    if (open())
    {
        m_serialPort->write(baId);
        m_serialPort->write(baVolt);
        m_serialPort->write(baCheck);
    }
}

bool PowerAmp::resetSingle(int id)
{
    bool success = false;
    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(RESET,1);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    readBack(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
        {
            success = true;
        }
        m_baRead.clear();
    }
    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Reset #" << id << " successfully.";
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Failed to reset #" << id << ".";
    }

    return success;
}

bool PowerAmp::resetAll()
{
    bool success = false;

    QList<int> errorId;

    for (int id=1;id<=DEV_COUNT_MAX;id++)
    {
        int safeCounter = 0;
        while(true)
        {
            if (resetSingle(id))
                break;
            else
                safeCounter++;

            if (safeCounter == 5)
            {
                errorId.append(id);
                break;
            }
        }
    }

    if (!errorId.isEmpty())
    {
        for(int i=0;i<errorId.size();i++)
        {            
            if (echoVolt(errorId.at(i)) == -1)
            {
                qDebug() << "#" << errorId.at(i) << "Failed to reset.";
                m_errorId.append(errorId.at(i));
            }
        }
    }

    if (m_errorId.isEmpty())
    {
        success = true;
        qCDebug(PA()) << PA().categoryName()
                      << "Reset all the power amplifiers successfully.";
        emit actionCompleted();

    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Failed to reset all the power amplifiers.";
        qCDebug(PA()) << PA().categoryName()
                      << m_errorId.size() << "power amplifiers have error.";
        qCDebug(PA()) << PA().categoryName()
                      << "They are:" << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

void PowerAmp::resetAll2()
{
    QByteArray baSend;
    baSend[0] = 0x80;
    baSend[1] = 0x00;
    baSend[2] = 0x00;
    baSend[3] = 0x00;
    baSend[4] = 0x00;

    if (open())
    {
        m_serialPort->write(baSend);
    }
}

VOLT PowerAmp::echoVolt(int id)
{
    VOLT volt = -1;

    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(ECHO_VOLT,1);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    readBack(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
        {
            volt = ba2volt(m_baRead);
        }
        m_baRead.clear();
    }
    if (volt != -1)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "The voltage of #" << id
                      << " is " << volt << "v.";
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Cannot echo the voltage of #" << id << ".";
    }
    return volt;
}

DEGREE PowerAmp::echoTemp(int id)
{
    DEGREE temp = -1;
    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(ECHO_TEMP,1);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    readBack(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
        {
            temp = ba2temp(m_baRead);
        }
        m_baRead.clear();
    }
    if (temp != -1)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "The temperature of #" << id
                      << " is " << temp << "degrees.";
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Cannot echo the temperature of #" << id << ".";
    }

    return temp;
}

bool PowerAmp::open()
{
    bool success = false;
    if (exist())
    {
        if (m_serialPort->isOpen())
        {
            success = true;
        }else
        {
            success = m_serialPort->open(QIODevice::ReadWrite);
        }
    }

    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Opened the serial port successfully.";
    }else
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Failed to open the serial port.";
    }

    return success;
}

void PowerAmp::close()
{
    if (open())
    {
        m_serialPort->close();
    }
}

void PowerAmp::handleError(QSerialPort::SerialPortError serialError)
{
    emit error(m_serialPort->errorString());
}

int PowerAmp::genRanId()
{
    //  Generate a random number of PA channel
    int ranId = genRandomNum() % DEV_COUNT_MAX;
    while (!ranId)
    {
        ranId = genRandomNum() % DEV_COUNT_MAX;
    }
    return ranId;
}

VOLT PowerAmp::genRanVolt()
{
    VOLT volt = (VOLT)(genRandomNum() % (VOLT_MAX * 10)) / 10;
    while (volt == 0)
    {
        volt = (VOLT)(genRandomNum() % (VOLT_MAX * 10)) / 10;
    }
    return volt;
}

int PowerAmp::genRandomNum()
{
    QTime time = QTime::currentTime();
    qsrand(time.msec() + time.second()*1000);
    return qrand();
}