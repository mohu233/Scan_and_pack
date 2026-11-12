#include "envmanager.h"
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QSettings> // 针对 Windows 注册表高级查询（可选）
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winver.h>
#endif

EnvManager::EnvManager(QObject *parent)
    : QObject{parent}
{}

EnvManager::~EnvManager() {}

// 🌟 核心辅助函数：精准读取 exe 文件的版本号
QString EnvManager::getExeVersion(const QString &exePath)
{
    if (!QFile::exists(exePath)) return "未知版本";

#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(exePath).toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &ignored);
    if (size > 0) {
        QVector<BYTE> data(static_cast<int>(size));
        if (GetFileVersionInfoW(nativePath.c_str(), 0, size, data.data())) {
            VS_FIXEDFILEINFO *info = nullptr;
            UINT infoSize = 0;
            if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &infoSize)
                && info && infoSize >= sizeof(VS_FIXEDFILEINFO)) {
                return QString::number(HIWORD(info->dwProductVersionMS));
            }
        }
    }
#endif
    return "未知";
}

// 🌟 自动扫描本地浏览器，并附带版本号
QList<BrowserInfo> EnvManager::detectLocalBrowsers()
{
    qDebug() << "核心类：开始自动扫描本地浏览器环境...";
    QList<BrowserInfo> browserList;

    QString progFiles = qEnvironmentVariable("ProgramFiles");
    QString progFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    QString localAppData = qEnvironmentVariable("LocalAppData");

    QMap<QString, QStringList> checkPaths;
    checkPaths["Chrome"] = QStringList()
                           << QDir::toNativeSeparators(progFiles + "/Google/Chrome/Application/chrome.exe")
                           << QDir::toNativeSeparators(progFilesX86 + "/Google/Chrome/Application/chrome.exe")
                           << QDir::toNativeSeparators(localAppData + "/Google/Chrome/Application/chrome.exe");

    checkPaths["Edge"] = QStringList()
                         << QDir::toNativeSeparators(progFiles + "/Microsoft/Edge/Application/msedge.exe")
                         << QDir::toNativeSeparators(progFilesX86 + "/Microsoft/Edge/Application/msedge.exe");

    QMap<QString, QStringList>::const_iterator i = checkPaths.constBegin();
    while (i != checkPaths.constEnd()) {
        QString browserName = i.key();
        QStringList paths = i.value();

        for (const QString &path : paths) {
            if (QFile::exists(path)) {
                BrowserInfo info;
                // 🌟 调用版本获取函数
                QString mainVersion = getExeVersion(path);

                // 完美拼接成 AdsPower 同款名字：如 "Chrome 120"
                info.name = QString("%1 %2").arg(browserName).arg(mainVersion);
                info.version = mainVersion;
                info.path = QDir::toNativeSeparators(path);

                browserList.append(info);
                qDebug() << "成功识别到详细浏览器：" << info.name << " -> 路径：" << path;
                break;
            }
        }
        ++i;
    }

    if (browserList.isEmpty()) {
        BrowserInfo info;
        info.name = "SunBrowser (内置默认内核)";
        info.version = "120";
        info.path = "mock_path/sunbrowser.exe";
        browserList.append(info);
    }

    return browserList;
}

// 🌟【精简 MainWindow 的核心功臣】：在此处直接操作 UI 控件
void EnvManager::initBrowserComboBox(QComboBox *comboBox)
{
    if (!comboBox) return;

    // 1. 清空下拉框
    comboBox->clear();

    // 2. 扫描数据
    QList<BrowserInfo> localBrowsers = this->detectLocalBrowsers();

    // 3. 循环填充
    for (const BrowserInfo &browser : localBrowsers) {
        // 展示的是详细名字（如 "Chrome 120"），隐藏绑定的是 exe 路径
        comboBox->addItem(browser.name, browser.path);
    }

    // 4. 默认选中第一项
    if (comboBox->count() > 0) {
        comboBox->setCurrentIndex(0);
    }
}

