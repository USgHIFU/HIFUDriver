#ifndef POWERAMP_H
#define POWERAMP_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QList>
#include <QLoggingCategory>

#include "poweramp_global.h"
#include "constant.h"
#include "macro.h"

Q_DECLARE_LOGGING_CATEGORY(PA)

class POWERAMPSHARED_EXPORT PowerAmp : public QObject
{
    Q_OBJECT
public:
    PowerAmp(QObject* parent = 0);
    ~PowerAmp();

    enum ACTION
    {
        RESET,
        START,
        ECHO_VOLT,
        ECHO_TEMP
    };

    //  initialize the serial port for power amplifiers
    //  used in the constructor or other places
    void initialize();
    inline bool exist() {return m_serialPort != NULL ? true : false;}

    bool resetSingle(int id);
    //  send only 5 bytes to reset all the power amplifiers
    bool resetAll2();
    bool startSingle(int id, VOLT volt);
    //  send only 5 bytes to start all the power amplifiers at the set voltage
    bool startAll2(VOLT volt);
    //  get the current voltage of the set power amplifier
    VOLT echoVolt(int id);
    //  get the current temperature of the set power amplifier
    DEGREE echoTemp(int id);

public slots:
    bool resetAll();
    bool startAll(VOLT volt);    

signals:
    void error(QString errorString);
    void actionCompleted();

private:
    //  set the serial port for the communication of power amplifiers
    void setPort();
    QSerialPort* m_serialPort;
    QString m_portName;

    //  the procedure of sending the set bytes and reading the echoed bytes
    void echo(QByteArray baId,QByteArray baVolt,QByteArray baCheck);
    QByteArray m_baRead;

    QList<int> m_errorId;

    bool open();
    void close();

    inline int validateId(int id) { return (( 0 <= id && id <= DEV_COUNT_MAX ) ? id : -1); }
    inline VOLT validateVolt(VOLT volt) { return (( volt < 0 || volt > VOLT_MAX ) ? -1 : volt); }

    QByteArray computeBaId(int id);
    QByteArray computeBaVolt(ACTION action, VOLT volt);
    QByteArray computeBaCheck(QByteArray baId, QByteArray baVolt);
    VOLT ba2volt(QByteArray baEcho);
    DEGREE ba2temp(QByteArray baEcho);

    bool checkReceivedBytes(QByteArray baReceive, QByteArray baSend);

    int genRanId();
    VOLT genRanVolt();
    int genRandomNum();
    QByteArray genSendBytes();

    void readSettings();
    void updateSettings();

private slots:
    void handleError(QSerialPort::SerialPortError serialError);
};

#endif // POWERAMP_H
