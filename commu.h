#ifndef COMMU_H
#define COMMU_H

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "api.pb.h"
#include <QObject>
#include <QUdpSocket>

#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QMap>
#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <QReadWriteLock>

const QString severityChar = "DIWEF";

class MlogInfo {
public:
    explicit MlogInfo(){
        addr.clear();
        port = 0;
        facility = "";
        needLogInfoRspTimes = 0;
        isSubscribe = false;
        loglevel = 0;
    }
    ~MlogInfo(){}
    QHostAddress addr;
    quint16 port;
    QString facility;
    int needLogInfoRspTimes;
    QDateTime lastRefreshTime;
    bool isSubscribe;
    int loglevel;
    QString filename;
    QString funcname;
    QString filter;
};

class Commu : public QObject
{
    Q_OBJECT
public:
    static Commu* Instance();
    static bool CheckIP(QString ip);
    void SetMlogAddress(const QHostAddress& addr);
    void GetFacilities(QMap<QString, QList<QPair<QHostAddress, quint16>>> &facilities);
    void Subscribe(QString &facility, const QHostAddress& addr, quint16 & port);
    void SetLogLevel(int level);
    void SetLogParam(QString filename,QString funcname,QString filter);

private:
    explicit Commu(QObject *parent = nullptr);
    ~Commu();

private slots:
    void __OnReadyRead();
    void __Refresh();

signals:
    void sigFacilityUpdate();
    void sigLogMsg(int level, QString& msg);

private:
    void __ParsePkg(QHostAddress& address, quint16& port, uint32_t cmd, char *pData, int len);
    int __SendPkg(QHostAddress& address, quint16& port, uint32_t cmd, google::protobuf::Message &msg);
    void __Broadcast();
    void __LogInfoRspHandler(QHostAddress& address, quint16& port, google::protobuf::Message *msg);
    void __LogSubscribeRspHandler(QHostAddress& address, quint16& port, google::protobuf::Message *msg);
    void __LogPublishNoticeHandler(QHostAddress& address, quint16& port, google::protobuf::Message *msg);
    void __UpdateMlogInfo(QHostAddress& address, quint16& port, QString &facility);

private:
    static Commu* m_iInstance;
    QUdpSocket* m_iUdpSocket;
    QByteArray m_RecvBuff;
    QHostAddress m_MlogAddr;
    QString m_MlogFacility;
    QReadWriteLock m_MlogInfosLock;
    QList<MlogInfo> m_MlogInfos;
    QTimer m_RefreshTimer;
};



#endif // COMMU_H
