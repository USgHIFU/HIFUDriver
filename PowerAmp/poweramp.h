#ifndef POWERAMP_H
#define POWERAMP_H

#include "poweramp_global.h"
#include "constant.h"
#include "macro.h"

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QList>
#include <QLoggingCategory>

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

    void initialize();
    inline bool exist() {return m_serialPort != NULL ? true : false;}

    bool resetSingle(int id);
    void resetAll2();
    bool startSingle(int id, VOLT volt);
    void startAll2(VOLT volt);
    VOLT echoVolt(int id);
    DEGREE echoTemp(int id);

public slots:
    bool resetAll();
    bool startAll(VOLT volt);    

signals:
    void error(QString errorString);
    void actionCompleted();

private:
    void setPort();
    QSerialPort* m_serialPort;
    QString m_portName;

    void readBack(QByteArray baId,QByteArray baVolt,QByteArray baCheck);
    QByteArray m_baRead;

    QList<int> m_errorId;

    bool open();
    void close();

    int validateId(int id);
    VOLT validateVolt(VOLT volt);

    QByteArray computeBaId(int id);
    QByteArray computeBaVolt(ACTION action, VOLT volt);
    QByteArray computeBaCheck(QByteArray baId,QByteArray baVolt);
    VOLT ba2volt(QByteArray baEcho);
    DEGREE ba2temp(QByteArray baEcho);

    bool checkReceivedBytes(QByteArray baReceive, QByteArray baSend);

    int genRanId();
    VOLT genRanVolt();
    int genRandomNum();
    QByteArray genSendBytes();

    void readConfig();
    void writeConfig();

private slots:
    void handleError(QSerialPort::SerialPortError serialError);
};

#endif // POWERAMP_H
