#ifndef DOCONTROLLER_H
#define DOCONTROLLER_H

#include <QObject>

#include "docontroller_global.h"
#include "inc/bdaqctrl.h"
#include "variable.h"
#include "constant.h"

using namespace Automation::BDaq;

class DOCONTROLLERSHARED_EXPORT DOController : public QObject
{
    Q_OBJECT

public:
    DOController(QObject *parent = 0);
    ~DOController();

    inline bool exist() {return m_instantDoCtrl != NULL ? true : false;}
    void writeData(int port, quint8 state);
    void sendPhase(quint8 channel, quint8 phase);
    void loadPhase();
    inline void enable() { writeData(PORT_ENABLE,BYTE_ENABLE); }
    inline void disable(){ writeData(PORT_DISABLE,BYTE_DISABLE); }

signals:
    void error(QString errorString);

private:    
    InstantDoCtrl *m_instantDoCtrl;
    QString m_deviceName;
    void selectDevice(QString deviceName);
    void checkError(ErrorCode errorCode);
};

#endif // DOCONTROLLER_H
