#include "settings.h"
#include <QFile>
#include <QJsonParseError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>

static bool readJsonFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QJsonParseError jsonError;
    QJsonDocument jsonDoc(QJsonDocument::fromJson(device.readAll(), &jsonError));
    if(jsonError.error != QJsonParseError::NoError)
    {
        qDebug() << "json error:" << jsonError.errorString();
        return false;
    }
    auto map1 = jsonDoc.toVariant().toMap();
    for(QMap<QString, QVariant>::const_iterator iter = map1.begin(); iter != map1.end(); ++iter){
        map[iter.key()] = iter.value();
    }
    return true;
}

static bool writeJsonFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    bool ret = false;

    QJsonObject rootObj;
    /*QJsonDocument jsonDocument; = QJsonDocument::fromVariant(QVariant::fromValue(map));
    if ( device.write(jsonDocument.toJson()) != -1 )
        ret = true;*/
    for(QMap<QString, QVariant>::const_iterator iter = map.begin(); iter != map.end(); ++iter){
        rootObj[iter.key()] = QJsonValue::fromVariant(iter.value());
    }
    QJsonDocument jsonDoc;
    jsonDoc.setObject(rootObj);
    if ( device.write(jsonDoc.toJson()) != -1 )
        ret = true;
    return ret;
}

Settings* Settings::m_iInstance = nullptr;
QString Settings::APP_NAME = "mlog_cli";





Settings::Settings():QSettings(Settings::APP_NAME+".json" ,QSettings::registerFormat("json", readJsonFile, writeJsonFile))
{
    sync();
//    m_iContainer->setPath(".");
}

Settings::~Settings()
{

}



