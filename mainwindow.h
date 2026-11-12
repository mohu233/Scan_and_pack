#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QVBoxLayout>
#include <QMainWindow>
#include <QTableWidget>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <QDateTime>
#include "envmanager.h"
#include <QLabel>
#include <QQueue>
#include <QUrl>
class QTabWidget;
class QNetworkAccessManager;
class ProxyExitIpLookup;
QT_BEGIN_NAMESPACE


#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

// struct EnvironmentInfo
// {
//     QString name;
//     QString status;
//     QString group;
//     QStringList modules;
// };




namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnOpen_clicked();
    //下拉菜单中“修改”和“删除”的槽函数声��?
    void on_btnEdit_triggered();
    void on_btnDelete_triggered();


    void on_pushButton_7_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_clicked();

    void on_pushButton_10_clicked();

    void on_pushButton_3_clicked();

private:
    friend class ProxyWorkflowTests;
    Ui::MainWindow *ui;
    QTableWidget *rightTable;
    QLineEdit *lineEditProxyHost;
    QLineEdit *lineEditProxyPort;
    QLineEdit *lineEditProxyUser;
    QLineEdit *lineEditProxyPass;
    QLabel *labelProxyHost;
    QLabel *labelProxyPort;
    QLabel *labelProxyUser;
    QLabel *labelProxyPass;
    QLineEdit *lineEditGroup;
    QLineEdit *lineEditTag;
    QLineEdit *lineEditAcceptLanguage;
    QComboBox *comboBoxUiLanguage;
    QLineEdit *lineEditResolution;
    QLineEdit *lineEditLatitude;
    QComboBox *comboBoxResolutionPreset;
    QLineEdit *lineEditLongitude;
    QLineEdit *lineEditCustomFonts;
    QComboBox *comboBoxSavedProxy;
    QLineEdit *lineEditDynamicProxyUrl;
    QLabel *labelDynamicProxyStatus;
    QComboBox *comboBoxIntegratedProvider;
    QLineEdit *lineEditIntegratedUser;
    QLineEdit *lineEditIntegratedPass;
    QLineEdit *lineEditIntegratedHost;
    QLineEdit *lineEditIntegratedPort;
    QVBoxLayout *m_proxyCardsLayout = nullptr;
    QLabel *m_proxyEmptyLabel = nullptr;
    QLabel *labelSavedProxyStatus = nullptr;
    quint64 m_dynamicProxyRequestSerial = 0;
    QTabWidget *proxyInputTabs = nullptr;
    QJsonObject m_resolvedDynamicProxy;
    QString m_requestedDynamicUrl;

    void initProxyModeUi();
    void applyInterfaceStyle();
    QJsonObject currentProxyProfile() const;
    QNetworkAccessManager *createProxyNetworkManager(const QJsonObject &profile, QString *error);
    bool importProxyFiles(const QStringList &paths, QString *message);
    void chooseProxyFiles(bool folder = false);
    void openWireGuardEditor(const QString &profileId);
    QJsonObject environmentProxyProfile(const QJsonObject &config) const;
    QString environmentProxyFingerprint(const QJsonObject &config) const;
    QString environmentIpText(const QJsonObject &config) const;
    void queueEnvironmentIpChecks();
    void startNextEnvironmentIpCheck();
    void updateEnvironmentExitIp(const QString &envId, const QString &fingerprint, const QString &ip);
    QQueue<QJsonObject> m_environmentIpQueue;
    QHash<QString, QPointer<ProxyExitIpLookup>> m_environmentIpChecks;
    QUrl m_environmentIpLookupUrl = QUrl("https://api.ipify.org?format=json");

    void resetCreateBrowserForm();
    void updateProxyInputsFromCurrentMode();
    bool validateCreateBrowserForm(QString *errorMessage) const;
    void initProxyManager();
    void refreshProxyManagerCards();
    void refreshSavedProxyCombo(const QString &selectedId = QString());
    void openProxyEditor(const QString &mode, const QString &profileId = QString());
    QJsonArray loadProxyProfiles() const;
    bool saveProxyProfiles(const QJsonArray &profiles) const;
    void fetchDynamicProxy(const QString &apiUrl, QLabel *statusLabel = nullptr);
    void testProxyProfile(const QJsonObject &profile, QLabel *statusLabel);

    struct BrowserLaunchConfig
    {
        QString envId;
        QString proxyFingerprint;
        QString envName;
        QString chromeUserDataDir;
        QString browserPath;
        QString userAgent;
        QString proxyServer;
        QString proxyType;
        QString proxyUsername;
        QString proxyPassword;
        QString proxyHost;
        int proxyPort = 0;
        QJsonObject wireguardProfile;
        QString acceptLanguage;
        QString uiLanguage;
        QString timezone;
        QString resolution;
        bool disableWebRtc = false;
        bool disableGeolocation = false;
        bool disableWebGl = false;
        QStringList startupUrls;
        QString extensionPath;
        QString webrtcMode;
    };

    bool openBrowserEnvironment(const BrowserLaunchConfig &config, int *debugPort);
    BrowserLaunchConfig buildLaunchConfig(const QJsonObject &configJson) const;
    void launchBrowserEnvironmentFromJson(const QJsonObject &launchJson,
                                          const QString &envId,
                                          const QString &envName,
                                          const QString &proxyType);

    // ��?config.json 中的 chrome_user_data_dir 解析为绝对路径（兼容旧版绝对路径 + 新版相对路径��?
    QString resolveChromeDataPath(const QString &configJsonPath, const QString &storedPath) const;

    //补上扫描并加载本地配置的私有函数声明
    void loadEnvironmentsFromJson();

    // 初始化时或导入后，用来加载本地模块数据的函数
    void loadModulesFromJson();

    void refreshEnvCards();

    void updateEnvCardStatus(const QString &envName, bool running);



    //
    QVBoxLayout *m_envLayout;

    void initDemoData();
    void createEnvCard(const EnvironmentInfo &info);

    void checkPythonAndDependencies();

    // ========== 日志系统 ==========
    void writeLog(const QString &category, const QString &message);
    static QString dataDir();
    QFile *m_logFile;




};
#endif // MAINWINDOW_H
