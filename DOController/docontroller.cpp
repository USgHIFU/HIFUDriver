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
//    if (errorCode == ErrorDeviceNotExist)
//    {
//        m_instantDoCtrl = NULL;
//    }
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
//    quint8 byteForLoad = (quint8)128;
//    quint8 byteForLock = (quint8)0;
//    writeData(PORT_LOAD,byteForLoad);
    writeData(PORT_LOAD,BYTE_LOAD);
//    writeData(PORT_LOAD,byteForLock);
    writeData(PORT_LOAD,BYTE_LOCK);
}

//void DOController::enable()
//{
//    quint8 byteForEnable = (quint8)64;
//    writeData(PORT_ENABLE,byteForEnable);
//    writeData(PORT_ENABLE,BYTE_ENABLE);
//}

//void DOController::disable()
//{
//    quint8 byteForDisable = (quint8)0;
//    writeData(PORT_DISABLE,byteForDisable);
//    writeData(PORT_DISABLE,BYTE_DISABLE);
//}
