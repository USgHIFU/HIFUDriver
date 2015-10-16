#include <QtSerialPort/QSerialPortInfo>
#include <QTime>
#include <QSettings>

#include "math.h"
#include "poweramp.h"

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
    for (int i=1;i<=DEV_TEST_COUNT;i++)
    {
        //  Generate a random id of PA channel
        int ranId = genRanId();        
        readSettings();
        m_serialPort = new QSerialPort(m_portName);

        if (resetSingle(ranId))
        {
            qCDebug(PA()) << PA().categoryName() << "Successfully initialized.";
            return;
        }
    }

    qCDebug(PA()) << PA().categoryName() << "The name of the serial port was changed.";

    for (int i=1;i<DEV_TEST_COUNT;i++)
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
        echo(baId,baVolt,baCheck);

        if (!m_baRead.isEmpty())
        {
            QByteArray baRead = m_baRead;
            m_baRead.clear();
            if (checkReceivedBytes(baRead,baSend))
            {
                m_portName = serialPortInfo.portName();
                updateSettings();
                qCDebug(PA()) << PA().categoryName() << "Successfully initialized.";
                return;
            }
        }
        delete m_serialPort;        
    }
    m_serialPort = NULL;
    qCDebug(PA()) << PA().categoryName() << "Failed to initialize.";
}

bool PowerAmp::open()
{
    bool success = false;
    if (exist())
    {
        success = m_serialPort->isOpen() ? true : m_serialPort->open(QIODevice::ReadWrite);
//        if (m_serialPort->isOpen())
//        {
//            success = true;
//        }else
//        {
//            success = m_serialPort->open(QIODevice::ReadWrite);
//        }
    }

    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Opened the serial port successfully.";
    }else
    {
        qCWarning(PA()) << PA().categoryName()
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
    qsrand(time.msec() + time.second() * MS_UNIT);
    return qrand();
}

void PowerAmp::echo(QByteArray baId, QByteArray baVolt, QByteArray baCheck)
{
    if (!baCheck.isEmpty())
    {
        if (open())
        {
            m_serialPort->write(baId+baVolt+baCheck);

            if (m_serialPort->waitForReadyRead(ECHO_PERIOD))
            {
                //  test
//                qDebug() << "readyRead signal emitted.";
//                qDebug() << "bytesAvailable: " << m_serialPort->bytesAvailable();
                while(m_serialPort->bytesAvailable() != 5)
                {
                    if (m_serialPort->waitForReadyRead(ECHO_PERIOD))
                    {
                        //  Do nothing, wait
                        //  test
//                        qDebug() << "readyRead signal emitted.";
//                        qDebug() << "bytesAvailable: " << m_serialPort->bytesAvailable();
                    }else
                    {
//                        qDebug() << "cannot emit readyRead signal.";
                        break;
                    }
                }
            }else
            {
//                qDebug() << "cannot emit readyRead signal.";
            }

            if (m_serialPort->bytesAvailable())
            {
                m_baRead.append(m_serialPort->readAll());
            }
        }
    }
}

void PowerAmp::readSettings()
{
    QSettings* settings = new QSettings(SETTINGS_PATH,QSettings::IniFormat);
    m_portName = settings->value("PowerAmp/port").toString();
    delete settings;
}

void PowerAmp::updateSettings()
{
    QSettings* settings = new QSettings(SETTINGS_PATH,QSettings::IniFormat);
    settings->setValue("PowerAmp/port",m_portName);
    delete settings;
}

//int PowerAmp::validateId(int id)
//{
//    return (( 0 <= id && id <= DEV_COUNT_MAX ) ? id : -1);
//    if ( 0 <= id && id <= DEV_COUNT_MAX )
//        return id;
//    else
//        return -1;
//}

//VOLT PowerAmp::validateVolt(VOLT volt)
//{
//    return (( volt < 0 || volt > VOLT_MAX ) ? -1 : volt);
//    if ( volt < 0 || volt > VOLT_MAX )
//        volt = -1;
//    return volt;
//}

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
        checked = ((baReceive[0] == (baSend[0] - 0x80)) ||
                   (baReceive[0] == baSend[0])) &&
                  (baReceive[1] == baSend[1]);
        switch (baSend[2])
        {
        case 0x00: case 0x40: case 0x41: case 0x42:
        case 0x43: case 0x44: case 0x45: case 0x46:
        case 0x47:
            checked = (checked &&
                      (baReceive[2] == baSend[2]) &&
                      (baReceive[3] == baSend[3]) &&
                      (baReceive[4] == baSend[4]));
            break;
        case 0x10: case 0x20:            
            break;
        default:
            checked = false;
            break;
        }
    }

    return checked;
}

//  TODO
//  template function for ba2volt and ba2temp

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

bool PowerAmp::startSingle(int id, VOLT volt)
{
    //  optimize: startAll, the same BaVolt, BaCheck with BaId
    bool success = false;
    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(START,volt);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    echo(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        success = checkReceivedBytes(m_baRead,baId+baVolt+baCheck) ? true : success;
//        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
//        {
//            success = true;
//        }
        m_baRead.clear();
    }
    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Started #" << id
                      << " with " << volt << "v successfully.";
    }else
    {
        qCWarning(PA()) << PA().categoryName()
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

            if (safeCounter == SAFE_COUNTER)
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
        qCWarning(PA()) << PA().categoryName()
                        << "Failed to start all the power amplifiers.";
        qCWarning(PA()) << PA().categoryName()
                        << m_errorId.size() << "power amplifiers have error.";
        qCWarning(PA()) << PA().categoryName()
                        << "They are: " << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

bool PowerAmp::startAll2(VOLT volt)
{
    bool success = false;

    QByteArray baId;
    baId[0] = 0x80;
    baId[1] = 0x00;

    QByteArray baVolt = computeBaVolt(START,volt);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    if (open())
    {
        m_serialPort->write(baId+baVolt+baCheck);
        m_serialPort->waitForReadyRead(ECHO_PERIOD);
    }

//    VOLT tmpVolt;
    for (int i=1;i<=DEV_COUNT_MAX;i++)
    {
//        tmpVolt = echoVolt(i);
        if (echoVolt(i) == -1)
        {
            m_errorId.append(i);
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
        qCWarning(PA()) << PA().categoryName()
                        << "Failed to start all the power amplifiers.";
        qCWarning(PA()) << PA().categoryName()
                        << m_errorId.size() << "power amplifiers have error.";
        qCWarning(PA()) << PA().categoryName()
                        << "They are: " << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

bool PowerAmp::resetSingle(int id)
{
    bool success = false;
    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(RESET,1);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    echo(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        success = checkReceivedBytes(m_baRead,baId+baVolt+baCheck) ? true : success;
//        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
//        {
//            success = true;
//        }
        m_baRead.clear();
    }
    if (success)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "Reset #" << id << " successfully.";
    }else
    {
        qCWarning(PA()) << PA().categoryName()
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

            if (safeCounter == SAFE_COUNTER)
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
        qCWarning(PA()) << PA().categoryName()
                        << "Failed to reset all the power amplifiers.";
        qCWarning(PA()) << PA().categoryName()
                        << m_errorId.size() << "power amplifiers have error.";
        qCWarning(PA()) << PA().categoryName()
                        << "They are:" << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

bool PowerAmp::resetAll2()
{
    bool success = false;

    QByteArray baSend;
    baSend[0] = 0x80;
    baSend[1] = 0x00;
    baSend[2] = 0x00;
    baSend[3] = 0x00;
    baSend[4] = 0x00;

    if (open())
    {
        m_serialPort->write(baSend);
        m_serialPort->waitForReadyRead(ECHO_PERIOD);
    }

//    VOLT tmpVolt;
    for (int i=1;i<=DEV_COUNT_MAX;i++)
    {
//        tmpVolt = echoVolt(i);
        if (echoVolt(i) == -1)
        {
            m_errorId.append(i);
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
        qCWarning(PA()) << PA().categoryName()
                        << "Failed to reset all the power amplifiers.";
        qCWarning(PA()) << PA().categoryName()
                        << m_errorId.size() << "power amplifiers have error.";
        qCWarning(PA()) << PA().categoryName()
                        << "They are:" << "#" << m_errorId;
        m_errorId.clear();
    }

    return success;
}

VOLT PowerAmp::echoVolt(int id)
{
    VOLT volt = -1;

    QByteArray baId = computeBaId(id);
    QByteArray baVolt = computeBaVolt(ECHO_VOLT,1);
    QByteArray baCheck = computeBaCheck(baId,baVolt);

    echo(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        volt = checkReceivedBytes(m_baRead,baId+baVolt+baCheck) ? ba2volt(m_baRead) : volt;
//        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
//        {
//            volt = ba2volt(m_baRead);
//        }
        m_baRead.clear();
    }
    if (volt != -1)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "The voltage of #" << id
                      << " is " << volt << "v.";
    }else
    {
        qCWarning(PA()) << PA().categoryName()
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

    echo(baId,baVolt,baCheck);

    if (!m_baRead.isEmpty())
    {
        temp = checkReceivedBytes(m_baRead,baId+baVolt+baCheck) ? ba2temp(m_baRead) : temp;
//        if (checkReceivedBytes(m_baRead,baId+baVolt+baCheck))
//        {
//            temp = ba2temp(m_baRead);
//        }
        m_baRead.clear();
    }
    if (temp != -1)
    {
        qCDebug(PA()) << PA().categoryName()
                      << "The temperature of #" << id
                      << " is " << temp << "degrees.";
    }else
    {
        qCWarning(PA()) << PA().categoryName()
                        << "Cannot echo the temperature of #" << id << ".";
    }

    return temp;
}
