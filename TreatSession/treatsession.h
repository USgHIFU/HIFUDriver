#ifndef TREATSESSION_H
#define TREATSESSION_H

class QTimer;

#include <QObject>
#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QVariant>
#include <QStringList>

#include "variable.h"
#include "constant.h"
#include "poweramp.h"
#include "docontroller.h"
#include "treatsession_global.h"

Q_DECLARE_LOGGING_CATEGORY(Session)

class TREATSESSIONSHARED_EXPORT TreatSession : public QObject
{
    Q_OBJECT

public:
    TreatSession(QObject* parent = 0);
    ~TreatSession();    

//  TODO: get spots of different planes
    void setSpots(QHash<float,QList<_3DCor> > spots);
    void setSonicationParam(_SoniParam params);
    inline _SesRec getRecorder() { return m_recorder; }
    inline QHash<QString, QVariant> getStatus() { return m_status; }

public slots:
    void start();
    void stop();
    void pause();
    void resume();

signals:
    readyStart();
    spotCompleted(int spotIndex);
    planeCompleted(float currPlane);
    sessionCompleted();
    statusUpdate();

private slots:
    void timeoutFcn();

private:
    enum SessionType
    {
        ON,
        OFF,
        COOLING
    };

    enum SessionError
    {
        NoError
    };

    QHash<float,QList<_3DCor> > m_spots;
    QHash<float,QList<int> > m_spotOrder;
    _SoniParam m_sonicationParam;
    float m_currPlane;

    //  the parameters used for the treat session
    _SesParam m_sessionParam;
    _SesRec m_recorder;
    SessionType m_currType;

    QTimer* m_timer;
    QStringList m_actionString;
    QStringList m_errorString;

    PowerAmp* m_pa;
    DOController* m_do;

    QHash<QString, QVariant> m_status;

    void onDuty();
    void offDuty();
    void cooling();

    void changeSpot();
    void resetSessionRecorder();
    void updateStatus();

    void setActionString();
    void setErrorString();
    QString printLastAction(cmdType);
    QString printLastError(SessionError);
};

#endif // TREATSESSION_H
