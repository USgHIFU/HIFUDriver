#include <QTimer>
#include <QDebug>

#include "treatsession.h"
#include "header.h"
#include "phaseinfo.c"

Q_LOGGING_CATEGORY(Session,"TREAT SESSION")

TreatSession::TreatSession(QObject *parent) : QObject(parent)
{
    m_pa = new PowerAmp(this);
    m_do = new DOController(this);

    if (!exist())
    {
        emit error("Cannot find the HIFU driver!");
    }

    resetSessionRecorder();
    setActionString();
    setErrorString();

    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setSingleShot(true);
    connect(m_sessionTimer,SIGNAL(timeout()),this,SLOT(onSessionCompleted()));
}

TreatSession::~TreatSession()
{
    m_pa->~PowerAmp();
    m_do->~DOController();
}

void TreatSession::setSpots(QHash<float,QList<_3DCor> > spots)
{
    if (spots.isEmpty())
    {
        return;
        //  TODO: emit a signal
    }
    //  Clear the previous spot settings
    m_spots.clear();
    m_spots = spots;
    m_angleCurrentPlane = spots.begin().key();
    m_sessionParam.spotCount = spots.begin().value().size();
}

void TreatSession::setSonicationParam(_SoniParam params)
{
    m_sonicationParam.volt = params.volt;
    m_sonicationParam.totalTime = params.totalTime;
    m_sonicationParam.period = params.period;
    m_sonicationParam.dutyCycle = params.dutyCycle;
    m_sonicationParam.coolingTime = params.coolingTime;

    //  converted into session parameters
    m_sessionParam.dutyOn = params.period * params.dutyCycle / PERCENT_UNIT;
    m_sessionParam.dutyOff = params.period - m_sessionParam.dutyOn;
    m_sessionParam.periodCount = params.totalTime * MS_UNIT / m_sessionParam.dutyOn;
    m_sessionParam.coolingTime = params.coolingTime * MS_UNIT;
}

void TreatSession::start()
{
    if (m_spots.isEmpty())
        return;

    if (m_pa->exist())
    {
        if (m_pa->startAll(m_sonicationParam.volt))
        {
            emit readyStart();
        }
    }
//  Start the treat session with the first spot in the plan
    changeSpot();
//  Deliver the acoustic energy at a given spot
    m_currentType = ON;
    if (m_do->exist())
    {
        m_do->enable();
    }    

    m_sessionTimer->start(m_sessionParam.dutyOn);

    qCDebug(Session()) << Session().categoryName()
                       << printLastAction(START)+printLastError(NoError);
    qCDebug(Session()) << SEPERATOR;
}

void TreatSession::updateStatus()
{
    int remainingSpotCount = 0;
    QHash<float, QList<_3DCor> >::iterator i = m_spots.find(m_angleCurrentPlane);
    for(;i != m_spots.end(); i++)
    {
        remainingSpotCount += i.value().size();
    }

    float sonicationTime = float(m_sonicationParam.totalTime) / float(m_sonicationParam.dutyCycle) * PERCENT_UNIT;
    qWarning() << "sonicationTime:" << sonicationTime;
    float operationTime = sonicationTime + m_sessionParam.coolingTime / MS_UNIT;
    qWarning() << "operationTime:" << operationTime;

    //  Remaining time(s)
    float remainingTime = remainingSpotCount * operationTime;

//    m_status["SonicatedSpot"] = QVariant(m_recorder.spotIndex);
    m_status["RemainingSpot"] = QVariant(remainingSpotCount);
    m_status["SonicatedPlane"] = QVariant(m_angleCurrentPlane);
//    m_status["SpotCount"] = QVariant(m_sessionParam.spotCount);
    m_status["RemainingTime"] = QVariant(remainingTime);

    if (m_pa->exist())
    {
        //  Echo voltage values of 144 channels after finishing sonication of a spot
        QList<QVariant> errorId;
        for (int i = 0; i < DEV_COUNT_MAX; i++)
        {
             VOLT volt = m_pa->echoVolt(i);
             if(volt < 0)
                 errorId.append(i);
        }
        m_status["EchoVoltErrorID"] = QVariant(errorId);

        for(int i = 0; i < m_status["EchoVoltErrorID"].toList().size(); i++)
        {
            qCWarning(Session()) << Session().categoryName()
                                 << "Echo voltage after sonication failed : #"
                                 << m_status["EchoVoltErrorID"].toList().value(i).toInt();
        }
    }

    emit statusUpdate();
}

void TreatSession::changeSpot()
{
    //  Determine whether the count of spots is 0
    if (m_spots.value(m_angleCurrentPlane).isEmpty())
    {
        m_spots.remove(m_angleCurrentPlane);
        if (!m_spots.isEmpty())
        {
            m_recorder.spotIndex = 0;
            m_angleCurrentPlane = m_spots.begin().key();
            m_sessionParam.spotCount = m_spots.begin().value().size();
        }else
        {
            m_sessionParam.spotCount = 0;
            return;
        }
    }

    QList<_3DCor> spots = m_spots.value(m_angleCurrentPlane);
    qCDebug(Session()) << Session().categoryName()
                       << m_spots.size() << "planes left.";
    qCDebug(Session()) << Session().categoryName()
                       << spots.size() << " spots left in the current plane.";
    Coordinate x = spots.at(0).x;
    Coordinate y = spots.at(0).y;
    Coordinate z = spots.at(0).z;
    spots.removeAt(0);
    m_spots.insert(m_angleCurrentPlane,spots);

    //  Compute the phases for the spot
    real_T volt[DEV_COUNT_MAX];
    real_T angle[DEV_COUNT_MAX];
    PhaseInfo(1,x,y,z,volt,angle);
    //  The phases of #22 and #23, #35 and #36, and #55 and #56 are inverted
    double value = angle[21];
    angle[21] = angle[22];
    angle[22] = value;
    value = angle[34];
    angle[34] = angle[35];
    angle[35] = value;
    value = angle[54];
    angle[54] = angle[55];
    angle[55] = value;

    if (m_do->exist())
    {
        for (int i=0;i<DEV_COUNT_MAX;i++)
        {
            //  Test
            qDebug() << "#" << i << ": " << angle[i];
            m_do->sendPhase(i,angle[i]);
            m_do->loadPhase();
        }
    }
}

void TreatSession::onDuty()
{
    //  Stop delivering the acoustic energy here
    m_currentType = OFF;
    if (m_do->exist())
    {
        m_do->disable();
    }

    m_sessionTimer->start(m_sessionParam.dutyOff);
}

void TreatSession::offDuty()
{
    m_recorder.periodIndex++;

    qCDebug(Session()) << Session().categoryName()
                       << "The #" << m_recorder.periodIndex
                       << " period is finished.";

    if (m_recorder.periodIndex < m_sessionParam.periodCount)
    {
        m_currentType = ON;
        //  Deliver the acoustic energy here at the same spot
        if (m_do->exist())
        {
            m_do->enable();
        }

        m_sessionTimer->start(m_sessionParam.dutyOn);
    }else
    {
        m_recorder.spotIndex++;
        m_recorder.periodIndex = 0;

        m_currentType = COOLING;
        //  Deliver a cooling period here
        if (m_do->exist())
        {
            m_do->disable();
        }
        m_sessionTimer->start(m_sessionParam.coolingTime);
        updateStatus();
        qCDebug(Session()) << Session().categoryName()
                           << "The #" << m_recorder.spotIndex
                           << " spot is finished.";
        changeSpot();
    }
}

void TreatSession::cooling()
{
    qCDebug(Session()) << Session().categoryName()
                       << "Finished the stage of cooling.";
    qCDebug(Session()) << SEPERATOR;

    if (m_recorder.spotIndex < m_sessionParam.spotCount)
    {
        // Start delivering the acoustic energy at the next spot
        m_currentType = ON;
        if (m_do->exist())
        {
            m_do->enable();
        }

        m_sessionTimer->start(m_sessionParam.dutyOn);
    }else
    {
        // Finish the current session
        qCDebug(Session()) << Session().categoryName()
                           << "All the spots have been sonicated.";
        qCDebug(Session()) << SEPERATOR;

        resetSessionRecorder();
        if (m_pa->exist())
        {
            if (m_pa->resetAll())
            {
                //  TODO: display the information
            }
        }
        emit sessionCompleted();
    }
}

void TreatSession::onSessionCompleted()
{
    switch (m_currentType)
    {
    case ON:
        onDuty();
        break;
    case OFF:
        offDuty();        
        break;
    case COOLING:
        cooling();
        break;
    }
}

void TreatSession::stop()
{
    pause();

    if (m_pa->exist())
    {
        if (m_pa->resetAll())
        {
//            qDebug() << "PowerAmp is reset";
        }
    }

    qCWarning(Session()) << Session().categoryName()
                         << printLastAction(STOP)+printLastError(NoError);
//    qCWarning(Session()) << Session().categoryName()
//                         << "The spot #" << m_recorder.spotIndex
//                         << ", period #" << m_recorder.periodIndex
//                         << " is finished.";
//    qCWarning(Session()) << SEPERATOR;

    //  Reset the session param
    resetSessionRecorder();
    m_spots.clear();
}

void TreatSession::pause()
{
    // Stop delivering the acoustic energy
    if (m_do->exist())
    {
        m_do->disable();
    }

    if (m_sessionTimer->isActive())
    {
        m_sessionTimer->stop();
    }
    qCWarning(Session()) << Session().categoryName()
                         << printLastAction(PAUSE)+printLastError(NoError);
    qCWarning(Session()) << Session().categoryName()
                         << "The spot #" << m_recorder.spotIndex
                         << ", period #" << m_recorder.periodIndex
                         << " is finished.";
    qCWarning(Session()) << SEPERATOR;
}

void TreatSession::resume()
{
    qCWarning(Session()) << Session().categoryName()
                         << printLastAction(RESUME)+printLastError(NoError);
    qCWarning(Session()) << Session().categoryName()
                         << "The spot #" << m_recorder.spotIndex
                         << ", period #" << m_recorder.periodIndex
                         << " is started, again.";
    qCWarning(Session()) << SEPERATOR;

    onSessionCompleted();
}

void TreatSession::resetSessionRecorder()
{
    m_recorder.periodIndex = 0;
    m_recorder.spotIndex = 0;
}

void TreatSession::setActionString()
{
    m_actionString << "Start a treat session"
                   << "Stop the running treat session"
                   << "Pause the running treat session"
                   << "Resume the previous treat session";
}

void TreatSession::setErrorString()
{
    m_errorString << "Done successfully.";
}

QString TreatSession::printLastAction(cmdType iType)
{
    QString str = "ACTION: ";
    str += m_actionString[iType - 1];
    str += " ";
    return str;
}

QString TreatSession::printLastError(SessionError i)
{
    QString str = "RESULT: ";
    str += m_errorString[i];
    return str;
}
