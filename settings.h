#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QDebug>
#include <QList>


class Settings : public QSettings
{
public:
    static Settings* GetInstance(){
        if(m_iInstance == nullptr){
            m_iInstance = new Settings();
        }
        return m_iInstance;
    }


private:
    Settings();
    ~Settings();

public:
    static QString APP_NAME;

private:
    static Settings* m_iInstance;
};

#endif // SETTINGS_H


