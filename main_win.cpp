#include "main_win.h"
#include "ui_main_win.h"
#include "commu.h"
#include <QMessageBox>
#include <QLineEdit>
#include "settings.h"

CMainWin::CMainWin(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CMainWin)
{
    ui->setupUi(this);
    connect(Commu::Instance(), SIGNAL(sigFacilityUpdate()), this, SLOT(__OnFacilityUpdate()));
    connect(Commu::Instance(), SIGNAL(sigLogMsg(int, QString&)), this, SLOT(__OnLogMsg(int, QString&)));
    ui->m_iFacilityView->setModel(&m_FacilityModel);

    ui->m_iMlogAddrEdit->setText(Settings::GetInstance()->value("mlog_ip").toString());
    ui->m_iAdvancedSettingGBox->setVisible(false);
    __OnMlogAddrEdited();
}

CMainWin::~CMainWin()
{
    delete ui;
}

void CMainWin::__OnFacilityUpdate()
{
    QMap<QString, QList<QPair<QHostAddress, quint16> > > facilities;
    Commu::Instance()->GetFacilities(facilities);
    m_FacilityModel.clear();
    QMap<QString, QList<QPair<QHostAddress, quint16> >>::const_iterator iter = facilities.cbegin();
    while (iter != facilities.cend()) {
//        cout << i.key() << ": " << i.value() << Qt::endl;
        QStandardItem *item = new QStandardItem(iter.key());
        for(int iLoop = 0; iLoop < iter.value().size(); iLoop++){
            QString ipport = iter.value().at(iLoop).first.toString() +":" +QString::number(iter.value().at(iLoop).second);
            QStandardItem *subitem= new QStandardItem(ipport);
            item->appendRow(subitem);
        }
        m_FacilityModel.appendRow(item);
        ++iter;
    }

}

void CMainWin::__OnMlogAddrEdited()
{
    QString addr = ui->m_iMlogAddrEdit->text();
    if (addr == ""){
        return ;
    }
    if (!Commu::CheckIP(addr)){
        qDebug("[%s %s:%d]addr(%s) is invalid", __FILE__, __FUNCTION__, __LINE__, addr.toStdString().c_str());
        QMessageBox::warning(this, "日志应用运行地址错误", "日志应用运行地址错误");
        ui->m_iMlogAddrEdit->clear();
        return;
    }
    qDebug("[%s %s:%d]addr(%s) ", __FILE__, __FUNCTION__, __LINE__, addr.toStdString().c_str());
    QHostAddress hostaddr(addr);
    Commu::Instance()->SetMlogAddress(hostaddr);
    Settings::GetInstance()->setValue("mlog_ip", addr);
}

void CMainWin::__OnMlogDoubleClicked(const QModelIndex &idx)
{
    QStandardItem* item = m_FacilityModel.itemFromIndex(idx);
    if (item != nullptr){
        qDebug("[%s %s:%d]item(%s) double clicked", __FILE__, __FUNCTION__, __LINE__, item->text().toStdString().c_str());
        QString ipport = item->text();
        QStringList tmps = ipport.split(':');
        QStandardItem* parent = item->parent();
        if (tmps.size() == 2 && parent != nullptr){
            if(Commu::CheckIP(tmps[0])){
                QHostAddress addr(tmps[0]);
                bool ok = false;
                quint16 port = tmps[1].toInt(&ok);
                if (ok && port > 0 && port <= 65535){
                    QString facility = parent->text();
                    Commu::Instance()->Subscribe(facility, addr, port);
                }
            }
        }
    }
}

void CMainWin::__OnLogMsg(int level, QString &msg)
{
    qDebug("[%s %s:%d]msg = %s", __FILE__, __FUNCTION__, __LINE__, msg.toStdString().c_str());
    QTextCharFormat fmt;
    //字体色
    switch (level) {
    case 0:
        fmt.setForeground(QBrush("gray"));
        break;
    case 1:
        fmt.setForeground(QBrush("blue"));
        break;
    case 2:
        fmt.setForeground(QBrush("orange"));
        break;
    case 3:
        fmt.setForeground(QBrush("red"));
        break;
    default:
        fmt.setForeground(QBrush("gray"));
        break;
    }

    //fmt.setUnderlineColor("red");

    //文本框使用以上设定
    ui->m_iLogMsgEdit->mergeCurrentCharFormat(fmt);
    ui->m_iLogMsgEdit->appendPlainText(msg);
}

void CMainWin::__OnShowAdvancedSetting(bool stat)
{
    ui->m_iAdvancedSettingGBox->setVisible(stat);
}

void CMainWin::__OnLogLevelChanged(const QString &text)
{
    Commu::Instance()->SetLogLevel(ui->m_iLogLevelEdit->currentIndex());
}

void CMainWin::__OnAdvancedSet()
{
    if(ui->m_iLogMonitorFilenameEdit->text() == "" && ui->m_iLogMonitorFuncnameEdit->text() == "" && ui->m_iLogMonitorFilterEdit->text() == ""){
        QMessageBox::warning(this, "设置高级日志参数错误", "请提供设置信息");
        return;
    }
    Commu::Instance()->SetLogParam(ui->m_iLogMonitorFilenameEdit->text(), ui->m_iLogMonitorFuncnameEdit->text(), ui->m_iLogMonitorFilterEdit->text());
}

