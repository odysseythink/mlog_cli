#include "commu.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QtEndian>
#include <QHostAddress>
#include <QRegExp>

#define MAX_PKG_LEN 1024*20
#define MAX_RECV_BUFF_LEN 1024*40
#define defaultLogStartPort  29999

Commu* Commu::m_iInstance = nullptr;

Commu *Commu::Instance()
{
    if(m_iInstance == nullptr){
        m_iInstance = new Commu();
    }
    return m_iInstance;
}

Commu::Commu(QObject *parent)
    : QObject{parent}
{
    m_MlogAddr.clear();
    m_MlogFacility = "";
    for(int iLoop = 0; iLoop < 100; iLoop++){
        int port = QRandomGenerator::global()->bounded(defaultLogStartPort, defaultLogStartPort+100);
        m_iUdpSocket = new QUdpSocket(this);
        if (!m_iUdpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)){
            qDebug("[%s %s:%d]udp socket bind(%d) failed:%s", __FILE__, __FUNCTION__, __LINE__, defaultLogStartPort+iLoop, m_iUdpSocket->errorString().toStdString().c_str());
            m_iUdpSocket->close();
            delete m_iUdpSocket;
            m_iUdpSocket = nullptr;
            continue;
        }
        break;
        //连接槽函数，参数1:信号所在的对象，参数2:信号，参数3:槽函数所在的对象，参数4:槽函数
        //信号与槽连接后，触发信号会执行槽函数
//        connect(receiver,SIGNAL(readyRead()),this,SLOT(Receive()));
    }
    if (m_iUdpSocket != nullptr){
        connect(m_iUdpSocket,SIGNAL(readyRead()),this,SLOT(__OnReadyRead()));
//        __Broadcast();
    }
    m_RefreshTimer.setInterval(1000);
    connect(&m_RefreshTimer, SIGNAL(timeout()), this, SLOT(__Refresh()));
    m_RefreshTimer.start();
}

Commu::~Commu()
{
    if (m_iUdpSocket != nullptr){
        m_iUdpSocket->close();
        m_iUdpSocket = nullptr;
    }
}

void Commu::__OnReadyRead()
{
    if (m_RecvBuff.size() > MAX_RECV_BUFF_LEN){
        return;
    } else { //没有缓存的包
        QHostAddress address;
        address.clear();
        quint16 port = 0;
        QByteArray data;
        data.resize(m_iUdpSocket->pendingDatagramSize());
        m_iUdpSocket->readDatagram(data.data(), data.size(), &address, &port);
        if (data.size() == 0){
            return;
        }
        QString strIpPort = address.toString()+":"+QString::number(port);
        qDebug("[%s %s:%d]read %dbytes from %s ", __FILE__, __FUNCTION__, __LINE__,data.size(), strIpPort.toStdString().c_str());
//        qDebug() << "read " << data.size() << " bytes";
        m_RecvBuff.append(data);

        while (m_RecvBuff.size() > 0){
            quint32 cmd = qFromBigEndian<quint32>(m_RecvBuff.data());
            quint32 pkglen = qFromBigEndian<quint32>(m_RecvBuff.data()+4);
            if(pkglen > MAX_PKG_LEN){
                qDebug("cmd=0x%08x pkg is too long", cmd);
                m_RecvBuff.clear();
                return;
            }
            if (pkglen > quint32(m_RecvBuff.size())){ // 没有收完,缓存,留待下一次处理
                return;
            }
            __ParsePkg(address, port, cmd, m_RecvBuff.data()+8, pkglen-8);
            m_RecvBuff.remove(0, pkglen);
        }
    }
}

void Commu::__Refresh()
{
    if (m_iUdpSocket != nullptr && m_MlogAddr.toString() != ""){
        __Broadcast();
    }
}

void Commu::__ParsePkg(QHostAddress& address, quint16& port, uint32_t cmd, char *pData, int len)
{
    switch (cmd) {
    case pbapi::PK_LOG_INFO_RSP::CMD:
    {
        pbapi::PK_LOG_INFO_RSP msg;
        if(!msg.ParseFromArray(pData, len)){
            qWarning("[%s %s:%d]parse from array failed", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        __LogInfoRspHandler(address, port, &msg);
        break;
    }
    case pbapi::PK_LOG_SUBSCRIBE_RSP::CMD:
    {
        pbapi::PK_LOG_SUBSCRIBE_RSP msg;
        if(!msg.ParseFromArray(pData, len)){
            qWarning("[%s %s:%d]parse from array failed", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        __LogSubscribeRspHandler(address, port, &msg);
        break;
    }
    case pbapi::PK_LOG_PUBLISH_NOTICE::CMD:
    {
        pbapi::PK_LOG_PUBLISH_NOTICE msg;
        if(!msg.ParseFromArray(pData, len)){
            qWarning("[%s %s:%d]parse from array failed", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        __LogPublishNoticeHandler(address, port, &msg);
        break;
    }
    }
//    QDateTime::currentDateTime()
}

int Commu::__SendPkg(QHostAddress& address, quint16& port, uint32_t cmd, google::protobuf::Message &msg)
{
    std::string data = msg.SerializeAsString();
    QByteArray pkg;
    pkg.resize(data.size()+8);
    qToBigEndian<quint32>(cmd, pkg.data());
    qToBigEndian<quint32>(data.size()+8, pkg.data()+4);
    memcpy(pkg.data()+8, data.c_str(), data.size());
    return m_iUdpSocket->writeDatagram(pkg, address, port);
}

void Commu::__Broadcast()
{
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    if(!m_MlogInfosLock.tryLockForWrite())return;
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    bool infosUpdate = false;
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); ){
        if (m_MlogInfos.at(iLoop).needLogInfoRspTimes >= 3){
            qDebug("[%s %s:%d]remove %s:%d facility(%s)", __FILE__, __FUNCTION__, __LINE__, m_MlogInfos.at(iLoop).addr.toString().toStdString().c_str(), m_MlogInfos.at(iLoop).port, m_MlogInfos.at(iLoop).facility.toStdString().c_str());
            m_MlogInfos.removeAt(iLoop);
            infosUpdate = true;
        } else {
            iLoop++;
        }
    }
    m_MlogInfosLock.unlock();
    pbapi::PK_LOG_INFO_REQ msg;
    msg.set_name("mlog");
    msg.set_pwd("mlog123456");
    for(int iLoop = 0; iLoop < 100; iLoop++){
        quint16 port = 19999+iLoop;
        qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
        if(m_MlogInfosLock.tryLockForWrite()){
            qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
            bool needRefresh = true;
            for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
                if (m_MlogInfos[iLoop].addr.toString() == m_MlogAddr.toString() && m_MlogInfos[iLoop].port == port){
                    m_MlogInfos[iLoop].needLogInfoRspTimes++;
                    if (m_MlogInfos[iLoop].lastRefreshTime.secsTo(QDateTime::currentDateTime()) > 1){
                        needRefresh = false;
                    }
                    break;
                }
            }
            m_MlogInfosLock.unlock();
            if (needRefresh){
                __SendPkg(m_MlogAddr, port, pbapi::PK_LOG_INFO_REQ::CMD, msg);
    //            qDebug("[%s %s:%d]send PK_LOG_INFO_REQ to(%s:%d)", __FILE__, __FUNCTION__, __LINE__, m_MlogAddr.toString().toStdString().c_str(), port);
            }
        }
        qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    }
    if (infosUpdate){
        emit sigFacilityUpdate();
    }
}

void Commu::__LogInfoRspHandler(QHostAddress &address, quint16 &port, google::protobuf::Message *msg)
{
    pbapi::PK_LOG_INFO_RSP* infoRsp = dynamic_cast<pbapi::PK_LOG_INFO_RSP *>(msg);
    if (infoRsp == nullptr){
        qWarning("[%s %s:%d]msg is not a pbapi::PK_LOG_INFO_RSP pointer", __FILE__, __FUNCTION__, __LINE__);
        return;
    }
    if (infoRsp->errmsg() == ""){
        QString facility = QString::fromStdString(infoRsp->facility());
        __UpdateMlogInfo(address, port, facility);
    }
}

void Commu::__LogSubscribeRspHandler(QHostAddress &address, quint16 &port, google::protobuf::Message *msg)
{
    pbapi::PK_LOG_SUBSCRIBE_RSP* infoRsp = dynamic_cast<pbapi::PK_LOG_SUBSCRIBE_RSP *>(msg);
    if (infoRsp == nullptr){
        qWarning("[%s %s:%d]msg is not a pbapi::PK_LOG_INFO_RSP pointer", __FILE__, __FUNCTION__, __LINE__);
        return;
    }
    if (infoRsp->errmsg() == ""){
        QString facility = "";
        __UpdateMlogInfo(address, port, facility);
    }
}

void Commu::__LogPublishNoticeHandler(QHostAddress &address, quint16 &port, google::protobuf::Message *msg)
{
    pbapi::PK_LOG_PUBLISH_NOTICE* infoRsp = dynamic_cast<pbapi::PK_LOG_PUBLISH_NOTICE *>(msg);
    if (infoRsp == nullptr){
        qWarning("[%s %s:%d]msg is not a pbapi::PK_LOG_INFO_RSP pointer", __FILE__, __FUNCTION__, __LINE__);
        return;
    }

    QString facility = QString::fromStdString(infoRsp->facility());
    __UpdateMlogInfo(address, port, facility);
    int idx = -1;
    m_MlogInfosLock.lockForRead();
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        if (m_MlogInfos[iLoop].addr.toString() == address.toString() && m_MlogInfos[iLoop].port == port && m_MlogInfos[iLoop].facility.toStdString() == infoRsp->facility()){
            idx = iLoop;
            break;
        }
    }
//    qDebug("[%s %s:%d]idx=%d", __FILE__, __FUNCTION__, __LINE__, idx);
    if (idx == -1) {
        qWarning("[%s %s:%d]can't found %s:%d facility=%s in the mlog list", __FILE__, __FUNCTION__, __LINE__, address.toString().toStdString().c_str(), port, facility.toStdString().c_str());
        m_MlogInfosLock.unlock();
        return;
    }
    bool showlog = false;
    if (infoRsp->level() >= m_MlogInfos[idx].loglevel){
        showlog = true;
    }

    if (m_MlogInfos[idx].filename != "" && m_MlogInfos[idx].filename.toStdString() != infoRsp->file()){
        showlog = false;
    }
    if (m_MlogInfos[idx].funcname != ""){
        QString funcname = QString::fromStdString(infoRsp->funcname());
        QStringList tmps = funcname.split('.');
        funcname = tmps[tmps.size()-1];
        if (m_MlogInfos[idx].funcname != funcname){
            showlog = false;
        }
    }

    if (m_MlogInfos[idx].filter != "" && !QString::fromStdString(infoRsp->msg()).contains(m_MlogInfos[idx].filter)){
        showlog = false;
    }
    if (showlog){
        QString logmsg = QString::asprintf("[%s][%c][%d][%s %s:%d]%s", infoRsp->timestamp().c_str(),severityChar[infoRsp->level()], infoRsp->pid(), infoRsp->file().c_str(), infoRsp->funcname().c_str(), infoRsp->line(), infoRsp->msg().c_str());
        emit sigLogMsg(infoRsp->level(), logmsg);
    }
//        qDebug("[%s %s:%d]msg = %s", __FILE__, __FUNCTION__, __LINE__, logmsg.toStdString().c_str());

    m_MlogInfosLock.unlock();
}

void Commu::__UpdateMlogInfo(QHostAddress &address, quint16 &port, QString &facility)
{
    m_MlogInfosLock.lockForWrite();
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        if (m_MlogInfos[iLoop].addr.toString() == address.toString() && m_MlogInfos[iLoop].port == port){
            m_MlogInfos[iLoop].needLogInfoRspTimes =0;
            m_MlogInfos[iLoop].lastRefreshTime = QDateTime::currentDateTime();
            if (facility != ""){
                if (facility != m_MlogInfos[iLoop].facility){
                    qDebug("[%s %s:%d]%s:%d update facility(%s)", __FILE__, __FUNCTION__, __LINE__, address.toString().toStdString().c_str(), port, facility.toStdString().c_str());
                    emit sigFacilityUpdate();
                }
                m_MlogInfos[iLoop].facility = facility;
            }
            m_MlogInfosLock.unlock();
            return;
        }
    }
    MlogInfo info;
    info.addr = address;
    info.port = port;
    info.facility = facility;
    info.lastRefreshTime = QDateTime::currentDateTime();
    info.needLogInfoRspTimes = 0;
    m_MlogInfos.append(info);
    qDebug("[%s %s:%d]add new %s:%d facility(%s)", __FILE__, __FUNCTION__, __LINE__, address.toString().toStdString().c_str(), port, facility.toStdString().c_str());
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    m_MlogInfosLock.unlock();
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    emit sigFacilityUpdate();
}


bool Commu::CheckIP(QString ip)
{
    QRegExp rx2("^(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$");
    if( !rx2.exactMatch(ip) )
    {
        return false;
    }
    return true;
}

void Commu::SetMlogAddress(const QHostAddress &addr)
{
    if (addr.isNull()){
        qWarning("[%s %s:%d]invalid arg", __FILE__, __FUNCTION__, __LINE__);
        return;
    }
    m_MlogAddr = addr;
    __Broadcast();
}

void Commu::GetFacilities(QMap<QString, QList<QPair<QHostAddress, quint16> > > &facilities)
{
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    m_MlogInfosLock.lockForRead();
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        if (facilities.contains(m_MlogInfos.at(iLoop).facility)){
            facilities[m_MlogInfos.at(iLoop).facility].append(QPair<QHostAddress, quint16>(m_MlogInfos.at(iLoop).addr, m_MlogInfos.at(iLoop).port));
        } else {
            QList<QPair<QHostAddress, quint16> > list;
            list.append(QPair<QHostAddress, quint16>(m_MlogInfos.at(iLoop).addr, m_MlogInfos.at(iLoop).port));
            facilities[m_MlogInfos.at(iLoop).facility] = list;
        }
    }
    m_MlogInfosLock.unlock();
}

void Commu::Subscribe(QString &facility, const QHostAddress &addr, quint16 &port)
{
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    m_MlogInfosLock.lockForWrite();
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    int idx = -1;
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        m_MlogInfos[iLoop].isSubscribe = false;
        if (m_MlogInfos[iLoop].addr.toString() == addr.toString() && m_MlogInfos[iLoop].port == port && m_MlogInfos[iLoop].facility == facility){
            idx = iLoop;
        }
    }
    if (idx == -1) {
        qWarning("[%s %s:%d]can't found %s:%d facility=%s in the mlog list", __FILE__, __FUNCTION__, __LINE__, addr.toString().toStdString().c_str(), port, facility.toStdString().c_str());
        m_MlogInfosLock.unlock();
        return;
    }
    pbapi::PK_LOG_SUBSCRIBE_REQ msg;
    msg.set_name("mlog");
    msg.set_pwd("mlog123456");
    msg.set_facility(facility.toStdString());
    __SendPkg(m_MlogAddr, port, pbapi::PK_LOG_SUBSCRIBE_REQ::CMD, msg);
    m_MlogInfos[idx].isSubscribe = true;
    m_MlogInfosLock.unlock();
}

void Commu::SetLogLevel(int level)
{
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    m_MlogInfosLock.lockForWrite();
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        m_MlogInfos[iLoop].loglevel = level;
    }
    m_MlogInfosLock.unlock();
}

void Commu::SetLogParam(QString filename, QString funcname, QString filter)
{
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    m_MlogInfosLock.lockForWrite();
    qDebug("[%s %s:%d]", __FILE__, __FUNCTION__, __LINE__);
    for(int iLoop = 0; iLoop < m_MlogInfos.size(); iLoop++){
        m_MlogInfos[iLoop].filename = filename;
        m_MlogInfos[iLoop].funcname = funcname;
        m_MlogInfos[iLoop].filter = filter;
    }
    m_MlogInfosLock.unlock();
}
