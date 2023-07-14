#ifndef CMAINWIN_H
#define CMAINWIN_H

#include <QMainWindow>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui { class CMainWin; }
QT_END_NAMESPACE

class CMainWin : public QMainWindow
{
    Q_OBJECT

public:
    CMainWin(QWidget *parent = nullptr);
    ~CMainWin();

private slots:
    void __OnFacilityUpdate();
    void __OnMlogAddrEdited();
    void __OnMlogDoubleClicked(const QModelIndex& idx);
    void __OnLogMsg(int level, QString &msg);
    void __OnShowAdvancedSetting(bool stat);
    void __OnLogLevelChanged(const QString &text);
    void __OnAdvancedSet();

private:
    Ui::CMainWin *ui;
    QStandardItemModel m_FacilityModel;
};
#endif // CMAINWIN_H
