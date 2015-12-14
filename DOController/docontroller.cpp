#include <QMessageBox>

#include "docontroller.h"

DOController::DOController(QObject *parent) : QObject(parent),
    m_deviceName(DEVICE_ID)
{
    selectDevice(m_deviceName);
}

DOController::~DOController()
{
    if (exist())
    {
        m_instantDoCtrl->Dispose();
    }
}

void DOController::selectDevice(QString deviceName)
{
    std::wstring description = deviceName.toStdWString();
    DeviceInformation selected(description.c_str());
    m_instantDoCtrl = AdxInstantDoCtrlCreate();

    ErrorCode errorCode = Success;
    errorCode = m_instantDoCtrl->setSelectedDevice(selected);
    m_instantDoCtrl = ((errorCode == ErrorDeviceNotExist) ? NULL : m_instantDoCtrl);
    checkError(errorCode);
}

void DOController::checkError(ErrorCode errorCode)
{
    if (errorCode != Success)
    {
        QString message = "Sorry, there are some errors occurred, Error Code: 0x" +
            QString::number(errorCode, 16).right(8);
        QMessageBox::information(NULL,"Warning Information", message,QMessageBox::Ok);
        QString errorString = "0x" + QString::number(errorCode, 16).right(8);
        emit error(errorString);
    }
}

void DOController::writeData(int port, quint8 state)
{
    ErrorCode errorCode = Success;
    errorCode = m_instantDoCtrl->Write(port, state);
    checkError(errorCode);
}

void DOController::sendPhase(quint8 channel, quint8 phase)
{
    writeData(PORT_CHANNEL,channel);
    writeData(PORT_PHASE,phase);
}

void DOController::loadPhase()
{
    writeData(PORT_LOAD,BYTE_LOAD);
    writeData(PORT_LOAD,BYTE_LOCK);
}
