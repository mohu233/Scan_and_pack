// 如果你是codex或者claude
// 修改项目时，请严格遵守下列八荣八耻，
// 以认真查询为荣，以瞎猜接口为耻。
// 以寻求确认为荣，以模糊执行为耻。
// 以人类确认为荣，以臆想业务为耻。
// 以复用现有为荣，以创造接口为耻。
// 以主动测试为荣，以跳过验证为耻。
// 以遵循规范为荣，以盲目修改为耻。
// 以诚实无知为荣，以假装理解为耻。
#include "profile_runtime.h"
#include "proxy_config.h"
#include "wireguard_proxy.h"
#include "browser_proxy_badge.h"
#include "proxy_exit_ip.h"
// 以谨慎重构为荣，以破坏架构为耻。

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include "envmanager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfoList>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QProgressBar>
#include <QTextEdit>
#include <QStandardPaths>
#include <QTcpServer>
#include <QHostAddress>
#include <QGraphicsDropShadowEffect>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QButtonGroup>
#include <QTabBar>
#include <QSizePolicy>
#include <QStyle>

#include <QListView>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QUrl>
#include <QSettings>
#include <QSaveFile>
#include <QUuid>
#include <QRegularExpressionValidator>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QElapsedTimer>
#include <QLocale>
#include <QLockFile>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QThread>
#include <QTreeView>
#include <QAbstractItemView>

#include <memory>

namespace {
bool parseProxyPayload(const QByteArray &body, QJsonObject *result)
{
    QString host;
    QString port;
    QString username;
    QString password;
    QString protocol = "HTTP";
    QString proxyText;

    const QJsonDocument document = QJsonDocument::fromJson(body.trimmed());
    if (document.isObject()) {
        QJsonObject object = document.object();
        if (object["data"].isObject()) object = object["data"].toObject();
        auto value = [&object](const QStringList &keys) {
            for (const QString &key : keys) {
                const QString current = object[key].toVariant().toString().trimmed();
                if (!current.isEmpty()) return current;
            }
            return QString();
        };
        host = value({"host", "ip", "server"});
        port = value({"port"});
        username = value({"username", "user"});
        password = value({"password", "pass"});
        const QString returnedProtocol = value({"type", "protocol"}).toUpper();
        if (!returnedProtocol.isEmpty()) protocol = returnedProtocol;
        proxyText = value({"proxy", "url"});
    } else {
        proxyText = QString::fromUtf8(body).trimmed();
    }

    if (host.isEmpty() && !proxyText.isEmpty()) {
        const QUrl proxyUrl(proxyText.contains("://") ? proxyText : "http://" + proxyText);
        host = proxyUrl.host();
        port = QString::number(proxyUrl.port());
        username = proxyUrl.userName();
        password = proxyUrl.password();
        if (!proxyUrl.scheme().isEmpty()) protocol = proxyUrl.scheme().toUpper();
    }
    if (host.isEmpty()) {
        const QStringList parts = proxyText.split(':');
        if (parts.size() >= 2) {
            host = parts[0].trimmed();
            port = parts[1].trimmed();
            if (parts.size() >= 3) username = parts[2];
            if (parts.size() >= 4) password = parts.mid(3).join(':');
        }
    }
    if (host.isEmpty() || port.toInt() <= 0 || port.toInt() > 65535) return false;

    if (result) {
        (*result)["protocol"] = protocol;
        (*result)["host"] = host;
        (*result)["port"] = port;
        (*result)["username"] = username;
        (*result)["password"] = password;
    }
    return true;
}
}

// 做成安装包一件安装

// python 补丁

//多张图片

// 放链接

// 按钮选择图片 视频

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_logFile(nullptr)
{
    ui->setupUi(this);
    if (!QCoreApplication::applicationVersion().isEmpty())
        ui->label_34->setText("v" + QCoreApplication::applicationVersion());
    setMinimumSize(1024, 680);
    ui->menubar->hide();
    ui->statusbar->hide();

    // ========== 日志系统初始化 ==========
    QString logPath = dataDir() + "/ProfilePower.log";
    m_logFile = new QFile(logPath, this);
    if (m_logFile->open(QIODevice::Append | QIODevice::Text)) {
        writeLog("启动", "══════════════════════════════════════");
        writeLog("启动", QString("ProfilePower 启动 — %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
        writeLog("启动", QString("日志路径: %1").arg(logPath));
        writeLog("启动", QString("程序目录: %1").arg(QCoreApplication::applicationDirPath()));
    }

    // ========== 全局美化 ==========
    this->setWindowTitle("ProfilePower");
    this->setMinimumSize(1100, 720);

    // 全局字体
    QFont appFont = this->font();
    appFont.setFamily("Microsoft YaHei UI");
    appFont.setPointSize(10);
    this->setFont(appFont);

    // ========== 全局 QSS 样式表（仅影响背景和滚动条，避免覆盖具体控件） ==========
    const QString globalBgStyle = R"(
        QMainWindow, QDialog {
            background-color: #f5f6fa;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #c8ced6;
            border-radius: 4px;
            min-height: 40px;
        }
        QScrollBar::handle:vertical:hover { background: #a0a8b4; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )";
    this->setStyleSheet(globalBgStyle);

    // =========================================================================
    // 1. 创建滚动区域，配置滚动策略并去掉边框
    // =========================================================================
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true); // 必须设置！这样里面的卡片宽度才会自动拉伸压满右侧空间
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 屏蔽横向滚动
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);    // 纵向按需显示滚动条
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");

    // =========================================================================
    // 2. 让滚动区域顶替原 ui->widget 的位置，把它完全压满
    // =========================================================================
    if (ui->widget->parentWidget() && ui->widget->parentWidget()->layout()) {
        QGridLayout *gridLayout = qobject_cast<QGridLayout*>(ui->widget->parentWidget()->layout());
        if (gridLayout) {
            // 找到 ui->widget 在 grid 中的位置（行、列）
            int idx = gridLayout->indexOf(ui->widget);
            int row, col, rowSpan, colSpan;
            gridLayout->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            // 从 grid 中移除 widget，但不删除它
            gridLayout->removeWidget(ui->widget);
            // 把 scrollArea 放到 widget 原来的位置，占据同样的行列跨度和拉伸系数
            gridLayout->addWidget(scrollArea, row, col, rowSpan, colSpan);
        }
    } else {
        if (ui->centralwidget->layout()) {
            ui->centralwidget->layout()->addWidget(scrollArea);
        } else {
            QVBoxLayout *mainLayout = new QVBoxLayout(ui->centralwidget);
            mainLayout->setContentsMargins(0, 0, 0, 0);
            mainLayout->addWidget(scrollArea);
        }
    }

    // =========================================================================
    // 3. 把主容器 ui->widget 塞进滚动区域中
    // =========================================================================
    scrollArea->setWidget(ui->widget);

    // =========================================================================
    // 4. 【唯一一次】为卡片容器初始化垂直布局，并设置紧凑靠顶对齐
    // =========================================================================
    m_envLayout = new QVBoxLayout(ui->widget);
    m_envLayout->setContentsMargins(20, 20, 20, 20); // 统一内边距
    m_envLayout->setSpacing(15);                     // 卡片之间的上下间距
    m_envLayout->setAlignment(Qt::AlignTop);
    ui->widget->setStyleSheet("background-color: #f7f9fa;"); // 舒服的浅灰色滚动底色

    ui->widget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    ui->widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);



    ui->widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // =========================================================================
    // 5. 依据本地 profiles/ 文件夹加载真实环境，无 config 则自动修复创建
    // =========================================================================
    QDir profilesDir(dataDir() + "/profiles");
    if (!profilesDir.exists()) {
        profilesDir.mkpath(".");
    }

    // 获取 profiles/ 目录下所有真实的指纹环境文件夹
    QStringList envSubDirs = profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // 如果当前本地没有任何真实指纹，我们默认创建一个“默认环境”防止界面空白
    if (envSubDirs.isEmpty()) {
        QString defaultEnvId = "env_default";
        profilesDir.mkpath(defaultEnvId + "/chrome-data");
        envSubDirs << defaultEnvId;
    }

    for (const QString &subDirName : envSubDirs) {
        QString currentEnvPath = profilesDir.absoluteFilePath(subDirName);
        QString jsonFilePath = currentEnvPath + "/config.json";
        QFile file(jsonFilePath);

        QJsonObject configJson;

        // 如果文件不存在，则直接在线自动生成并修复一个基础的默认环境文件结构
        if (!file.exists()) {
            configJson["env_id"] = subDirName;
            configJson["env_name"] = subDirName == "env_default" ? "默认指纹环境" : subDirName;
            configJson["created_time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            configJson["chrome_user_data_dir"] = "./chrome-data";
            configJson["remark"] = "系统自动初始化的环境文件";
            configJson["proxy_type"] = "无代理";
            configJson["modules"] = QJsonArray(); // 默认没有挂载

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(QJsonDocument(configJson).toJson(QJsonDocument::Indented));
                file.close();
            }
        } else {
            // 如果存在，读取真实数据
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                configJson = QJsonDocument::fromJson(file.readAll()).object();
                file.close();
            }
        }

        // 把真实的本地指纹数据转化为实体对象，并传递给卡片生成组件
        EnvironmentInfo realEnvInfo;
        realEnvInfo.name = configJson["env_name"].toString();
        realEnvInfo.configPath = jsonFilePath;
        realEnvInfo.status = "未运行";
        realEnvInfo.group = "默认组";


        qDebug() << "\n===== [构造函数] 加载环境 =====";
        qDebug() << "subDirName (文件夹名/env_id):" << subDirName;
        qDebug() << "jsonFilePath:" << jsonFilePath;
        qDebug() << "env_name:" << configJson["env_name"].toString();
        qDebug() << "modules数组:" << configJson["modules"].toArray();



        // 从 config.json 抽取并还原真实加载已经挂载的模块数组
        QJsonArray modsArray = configJson["modules"].toArray();
        for (int m = 0; m < modsArray.size(); ++m) {
            realEnvInfo.modules.append(modsArray[m].toString());
        }

        // 渲染到前端界面 widget 内！
        createEnvCard(realEnvInfo);
    }


    // =========================================================================
    // 6. 其他附属控件初始化
    // =========================================================================
    // 1. 实例化核心业务类
    EnvManager *envMgr = new EnvManager(this);

    // 只需要把界面上的 comboBox_2 指针传给它，它内部自己扫描、自己加载、自己对齐！
    envMgr->initBrowserComboBox(ui->comboBox_2);

    ui->comboBox_3->addItems(QStringList() << "Stable" << "Beta" << "Dev" << "Canary");
    ui->comboBox_6->addItems(QStringList() << "Windows" << "macOS" << "Linux");
    ui->comboBox_8->addItems(QStringList() << "Windows 10" << "Windows 11" << "macOS 14" << "Ubuntu 22.04");
    ui->comboBox_7->addItems(QStringList() << "x64" << "arm64");
    ui->comboBox_5->addItems(QStringList() << "Desktop" << "Laptop");
    ui->comboBox_4->addItems(QStringList() << "Auto" << "Chrome" << "Edge");
    ui->comboBox_9->addItems(QStringList() << "Custom" << "Chrome Windows" << "Chrome macOS" << "Edge Windows");
    ui->comboBox_11->addItems(QStringList() << "No Proxy" << "HTTP" << "HTTPS" << "SOCKS5");
    ui->comboBox_11->setCurrentText("HTTP");

    // ---- Proxy host/port/user/pass input fields ----
    lineEditProxyHost = new QLineEdit();
    lineEditProxyHost->setPlaceholderText("proxy.example.com");
    labelProxyHost = new QLabel("Proxy Host:");
    ui->gridLayout_4->addWidget(labelProxyHost, 2, 0);
    ui->gridLayout_4->addWidget(lineEditProxyHost, 2, 1);

    lineEditProxyPort = new QLineEdit();
    lineEditProxyPort->setPlaceholderText("8080");
    lineEditProxyPort->setValidator(new QIntValidator(1, 65535, lineEditProxyPort));
    labelProxyPort = new QLabel("Proxy Port:");
    ui->gridLayout_4->addWidget(labelProxyPort, 3, 0);
    ui->gridLayout_4->addWidget(lineEditProxyPort, 3, 1);

    lineEditProxyUser = new QLineEdit();
    lineEditProxyUser->setPlaceholderText("Optional username");
    labelProxyUser = new QLabel("Proxy User:");
    ui->gridLayout_4->addWidget(labelProxyUser, 4, 0);
    ui->gridLayout_4->addWidget(lineEditProxyUser, 4, 1);

    lineEditProxyPass = new QLineEdit();
    lineEditProxyPass->setEchoMode(QLineEdit::Password);
    lineEditProxyPass->setPlaceholderText("Optional password");
    labelProxyPass = new QLabel("Proxy Pass:");
    ui->gridLayout_4->addWidget(labelProxyPass, 5, 0);
    ui->gridLayout_4->addWidget(lineEditProxyPass, 5, 1);

    // Toggle proxy fields: setVisible collapses hidden rows; setEnabled leaves blank space
    connect(ui->comboBox_11, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        bool hasProxy = (text != "No Proxy" && text != QString::fromUtf8("\u65e0\u4ee3\u7406"));
        lineEditProxyHost->setVisible(hasProxy);
        lineEditProxyPort->setVisible(hasProxy);
        lineEditProxyUser->setVisible(hasProxy);
        lineEditProxyPass->setVisible(hasProxy);
        labelProxyHost->setVisible(hasProxy);
        labelProxyPort->setVisible(hasProxy);
        labelProxyUser->setVisible(hasProxy);
        labelProxyPass->setVisible(hasProxy);
        ui->gridLayout_4->invalidate();
        ui->gridLayout_4->activate();
    });
    ui->comboBox_11->setCurrentText("No Proxy");
    ui->comboBox_12->addItems(QStringList() << "ip-api" << "ipinfo" << "ip2location");
    ui->comboBox_12->setCurrentText("ipinfo");
    ui->comboBox_10->addItems(QStringList() << "General" << "Facebook" << "Google" << "TikTok" << "Amazon" << "Shopee");
    ui->comboBox_14->addItems(QStringList() << "Asia/Shanghai" << "UTC" << "America/New_York" << "Europe/London" << "Asia/Tokyo");
    ui->comboBox_14->setEditable(true);
    ui->comboBox_13->addItems(QStringList() << "Google Inc. (Intel)" << "Google Inc. (NVIDIA)" << "Intel Inc." << "NVIDIA Corporation");

    // Replace the accidental font selectors with editable group/tag fields.
    ui->fontComboBox_2->hide();
    ui->fontComboBox->hide();
    lineEditGroup = new QLineEdit(ui->page_2);
    lineEditTag = new QLineEdit(ui->page_2);
    lineEditGroup->setPlaceholderText("Default group");
    lineEditTag->setPlaceholderText("Optional tag");
    ui->horizontalLayout_8->addWidget(lineEditGroup);
    ui->horizontalLayout_8->addWidget(lineEditTag);

    lineEditAcceptLanguage = new QLineEdit("zh-CN,zh;q=0.9,en;q=0.8", ui->tab_18);
    auto languageLayout = new QHBoxLayout(ui->tab_18);
    languageLayout->setContentsMargins(8, 8, 8, 8);
    languageLayout->addWidget(lineEditAcceptLanguage);

    comboBoxUiLanguage = new QComboBox(ui->tab_26);
    comboBoxUiLanguage->setEditable(true);
    comboBoxUiLanguage->addItems({"zh-CN", "en-US", "ja-JP", "de-DE", "fr-FR"});
    auto uiLanguageLayout = new QHBoxLayout(ui->tab_26);
    uiLanguageLayout->setContentsMargins(8, 8, 8, 8);
    uiLanguageLayout->addWidget(comboBoxUiLanguage);

    comboBoxResolutionPreset = new QComboBox(ui->tab_22);
    comboBoxResolutionPreset->addItems({"1920x1080", "1680x1050", "1536x864", "1440x900", "1366x768", "1280x720"});
    auto presetResolutionLayout = new QHBoxLayout(ui->tab_22);
    presetResolutionLayout->setContentsMargins(8, 8, 8, 8);
    presetResolutionLayout->addWidget(comboBoxResolutionPreset);
    lineEditResolution = new QLineEdit("1920x1080", ui->tab_27);
    lineEditResolution->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]{3,5}[xX,][0-9]{3,5}$"), lineEditResolution));
    auto customResolutionLayout = new QHBoxLayout(ui->tab_27);
    customResolutionLayout->setContentsMargins(8, 8, 8, 8);
    customResolutionLayout->addWidget(lineEditResolution);

    lineEditLatitude = new QLineEdit(ui->tab_25);
    lineEditLongitude = new QLineEdit(ui->tab_25);
    lineEditLatitude->setPlaceholderText("Latitude (-90..90)");
    lineEditLongitude->setPlaceholderText("Longitude (-180..180)");
    lineEditLatitude->setValidator(new QDoubleValidator(-90.0, 90.0, 6, lineEditLatitude));
    lineEditLongitude->setValidator(new QDoubleValidator(-180.0, 180.0, 6, lineEditLongitude));
    auto geoLayout = new QHBoxLayout(ui->tab_25);
    geoLayout->setContentsMargins(8, 8, 8, 8);
    geoLayout->addWidget(lineEditLatitude);
    geoLayout->addWidget(lineEditLongitude);

    lineEditCustomFonts = new QLineEdit(ui->tab_24);
    lineEditCustomFonts->setPlaceholderText("Arial, Segoe UI, Microsoft YaHei");
    auto fontsLayout = new QHBoxLayout(ui->tab_24);
    fontsLayout->setContentsMargins(8, 8, 8, 8);
    fontsLayout->addWidget(lineEditCustomFonts);

    // Saved proxy library.
    auto savedProxyPageLayout = new QVBoxLayout(ui->tab_2);
    savedProxyPageLayout->setContentsMargins(12, 12, 12, 12);
    savedProxyPageLayout->setSpacing(0);
    QWidget *savedProxyContainer = ui->horizontalLayout_9->parentWidget();
    ui->horizontalLayout_9->setContentsMargins(0, 0, 0, 0);
    savedProxyContainer->setMinimumHeight(42);
    savedProxyContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    savedProxyPageLayout->addWidget(savedProxyContainer);
    comboBoxSavedProxy = new QComboBox(ui->tab_2);
    comboBoxSavedProxy->setObjectName("comboBoxSavedProxy");
    auto btnManageProxy = new QPushButton(QString::fromUtf8("管理代理"), ui->tab_2);
    auto btnRefreshProxy = new QPushButton(QString::fromUtf8("刷新"), ui->tab_2);
    labelSavedProxyStatus = new QLabel(ui->tab_2);
    ui->horizontalLayout_9->addWidget(comboBoxSavedProxy, 1);
    ui->horizontalLayout_9->addWidget(btnManageProxy);
    ui->horizontalLayout_9->addWidget(btnRefreshProxy);
    ui->horizontalLayout_9->addWidget(labelSavedProxyStatus);
    const QString savedProxyActionStyle =
        "QPushButton { background: #ffffff; color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; padding: 7px 13px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { border-color: #2563eb; color: #1d4ed8; background: #f8fbff; }";
    btnManageProxy->setStyleSheet(savedProxyActionStyle);
    btnRefreshProxy->setStyleSheet(savedProxyActionStyle);
    btnManageProxy->setCursor(Qt::PointingHandCursor);
    btnRefreshProxy->setCursor(Qt::PointingHandCursor);
    labelSavedProxyStatus->setStyleSheet("color: #64748b; font-size: 12px;");
    refreshSavedProxyCombo();
    connect(comboBoxSavedProxy, &QComboBox::currentIndexChanged, this, [this]() { updateProxyInputsFromCurrentMode(); });
    connect(btnManageProxy, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->page_3);
        refreshProxyManagerCards();
    });
    connect(btnRefreshProxy, &QPushButton::clicked, this, [this]() {
        refreshSavedProxyCombo(comboBoxSavedProxy->currentData().toMap().value("id").toString());
    });

    // Dynamic proxy API: accepts JSON, a proxy URL, or host:port:user:password.
    auto dynamicLayout = new QVBoxLayout(ui->tab_3);
    auto dynamicInputLayout = new QHBoxLayout();
    lineEditDynamicProxyUrl = new QLineEdit(ui->tab_3);
    lineEditDynamicProxyUrl->setPlaceholderText("https://provider.example/api/get-proxy");
    auto btnFetchDynamicProxy = new QPushButton("Fetch", ui->tab_3);
    labelDynamicProxyStatus = new QLabel(ui->tab_3);
    dynamicInputLayout->addWidget(lineEditDynamicProxyUrl, 1);
    dynamicInputLayout->addWidget(btnFetchDynamicProxy);
    dynamicLayout->addLayout(dynamicInputLayout);
    dynamicLayout->addWidget(labelDynamicProxyStatus);
    connect(btnFetchDynamicProxy, &QPushButton::clicked, this, [this]() {
        fetchDynamicProxy(lineEditDynamicProxyUrl->text(), labelDynamicProxyStatus);
    });

    // Integrated providers use editable gateway defaults and the existing relay.
    auto integratedLayout = new QFormLayout(ui->tab_4);
    comboBoxIntegratedProvider = new QComboBox(ui->tab_4);
    comboBoxIntegratedProvider->addItems({"Bright Data", "Oxylabs", "IPRoyal", "Custom gateway"});
    lineEditIntegratedHost = new QLineEdit(ui->tab_4);
    lineEditIntegratedPort = new QLineEdit(ui->tab_4);
    lineEditIntegratedPort->setValidator(new QIntValidator(1, 65535, lineEditIntegratedPort));
    lineEditIntegratedUser = new QLineEdit(ui->tab_4);
    lineEditIntegratedPass = new QLineEdit(ui->tab_4);
    lineEditIntegratedPass->setEchoMode(QLineEdit::Password);
    integratedLayout->addRow("Provider", comboBoxIntegratedProvider);
    integratedLayout->addRow("Gateway", lineEditIntegratedHost);
    integratedLayout->addRow("Port", lineEditIntegratedPort);
    integratedLayout->addRow("Username", lineEditIntegratedUser);
    integratedLayout->addRow("Password", lineEditIntegratedPass);
    auto updateProvider = [this]() {
        const QString provider = comboBoxIntegratedProvider->currentText();
        if (provider == "Bright Data") { lineEditIntegratedHost->setText("brd.superproxy.io"); lineEditIntegratedPort->setText("33335"); }
        else if (provider == "Oxylabs") { lineEditIntegratedHost->setText("pr.oxylabs.io"); lineEditIntegratedPort->setText("7777"); }
        else if (provider == "IPRoyal") { lineEditIntegratedHost->setText("geo.iproyal.com"); lineEditIntegratedPort->setText("12321"); }
    };
    connect(comboBoxIntegratedProvider, &QComboBox::currentTextChanged, this, [updateProvider]() { updateProvider(); });
    updateProvider();
    initProxyModeUi();
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this]() { updateProxyInputsFromCurrentMode(); });
    initProxyManager();

    ui->comboBox_9->clear();
    ui->comboBox_9->addItems({"Auto generated", "Custom", "Chrome Windows", "Chrome macOS", "Chrome Linux", "Edge Windows"});

    auto updateOsVersions = [this]() {
        const QString current = ui->comboBox_8->currentText();
        const QString family = ui->comboBox_6->currentText();
        QStringList versions;
        if (family == "macOS") versions = {"macOS 13", "macOS 14", "macOS 15"};
        else if (family == "Linux") versions = {"Ubuntu 20.04", "Ubuntu 22.04", "Ubuntu 24.04"};
        else versions = {"Windows 10", "Windows 11"};
        ui->comboBox_8->clear();
        ui->comboBox_8->addItems(versions);
        ui->comboBox_8->setCurrentText(versions.contains(current) ? current : versions.constFirst());
    };

    auto updateUserAgent = [this]() {
        const QString profile = ui->comboBox_9->currentText();
        if (profile == "Custom") return;

        QString family = ui->comboBox_6->currentText();
        QString engine = ui->comboBox_4->currentText();
        if (profile.contains("Windows")) family = "Windows";
        else if (profile.contains("macOS")) family = "macOS";
        else if (profile.contains("Linux")) family = "Linux";
        if (profile.startsWith("Edge")) engine = "Edge";
        else if (profile.startsWith("Chrome")) engine = "Chrome";
        if (engine == "Auto") {
            const QString browser = ui->comboBox_2->currentText() + " " + ui->comboBox_2->currentData().toString();
            engine = browser.contains("edge", Qt::CaseInsensitive) ? "Edge" : "Chrome";
        }

        const QRegularExpressionMatch match = QRegularExpression("(\\d+)").match(ui->comboBox_2->currentText());
        const QString version = match.hasMatch() ? match.captured(1) : "146";
        QString osToken = "Windows NT 10.0; Win64; x64";
        if (family == "macOS") osToken = "Macintosh; Intel Mac OS X 10_15_7";
        else if (family == "Linux") osToken = ui->comboBox_7->currentText() == "arm64"
                                                   ? "X11; Linux aarch64" : "X11; Linux x86_64";
        QString ua = QString("Mozilla/5.0 (%1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%2.0.0.0 Safari/537.36")
                         .arg(osToken, version);
        if (engine == "Edge") ua += QString(" Edg/%1.0.0.0").arg(version);
        ui->lineEdit_3->setText(ua);
    };

    connect(ui->comboBox_6, &QComboBox::currentTextChanged, this, [updateOsVersions, updateUserAgent]() {
        updateOsVersions();
        updateUserAgent();
    });
    for (QComboBox *combo : {ui->comboBox_2, ui->comboBox_4, ui->comboBox_7, ui->comboBox_8, ui->comboBox_9}) {
        connect(combo, &QComboBox::currentTextChanged, this, [updateUserAgent]() { updateUserAgent(); });
    }
    updateOsVersions();
    updateUserAgent();

    connect(ui->comboBox_13, &QComboBox::currentTextChanged, this, [this](const QString &vendor) {
        if (vendor.contains("NVIDIA")) ui->lineEdit_6->setText("ANGLE (NVIDIA, NVIDIA GeForce GTX 1660 Direct3D11)");
        else ui->lineEdit_6->setText("ANGLE (Intel, Intel(R) UHD Graphics Direct3D11)");
    });
    ui->lineEdit_6->setText("ANGLE (Intel, Intel(R) UHD Graphics Direct3D11)");
    connect(ui->pushButton_9, &QPushButton::clicked, this, [this]() {
        updateProxyInputsFromCurrentMode();
        const QJsonObject profile = currentProxyProfile();
        QString proxyError;
        if (!ProxyConfig::validate(profile, &proxyError)) {
            QMessageBox::warning(this, QString::fromUtf8("检查网络"), proxyError);
            return;
        }
        auto manager = createProxyNetworkManager(profile, &proxyError);
        if (!manager) {
            QMessageBox::warning(this, QString::fromUtf8("检查网络"), proxyError);
            return;
        }
        QString endpoint = "https://ipinfo.io/json";
        if (ui->comboBox_12->currentText() == "ip-api") endpoint = "http://ip-api.com/json/?fields=status,message,query,countryCode,lat,lon,timezone";
        else if (ui->comboBox_12->currentText() == "ip2location") endpoint = "https://api.ipify.org?format=json";
        QNetworkRequest request{QUrl(endpoint)};
        request.setRawHeader("Accept", "application/json");
        request.setHeader(QNetworkRequest::UserAgentHeader, "ProfilePower/1.0");
        ui->pushButton_9->setEnabled(false);
        ui->pushButton_9->setText("Testing...");
        QNetworkReply *reply = manager->get(request);
        QTimer::singleShot(15000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
            ui->pushButton_9->setEnabled(true);
            ui->pushButton_9->setText(QString::fromUtf8("\u68c0\u67e5\u7f51\u7edc"));
            const QByteArray body = reply->readAll();
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::warning(this, "Proxy test", QString("Connection failed:\n%1").arg(reply->errorString()));
                reply->deleteLater(); manager->deleteLater();
                return;
            }
            const QJsonDocument document = QJsonDocument::fromJson(body);
            if (!document.isObject()) {
                QMessageBox::warning(this, "Proxy test", "The IP service returned invalid data.");
                reply->deleteLater(); manager->deleteLater();
                return;
            }
            const QJsonObject object = document.object();
            if (object["status"].toString() == "fail") {
                QMessageBox::warning(this, "Proxy test", object["message"].toString("IP service failed."));
                reply->deleteLater(); manager->deleteLater();
                return;
            }
            const QString ip = object["ip"].toString(object["query"].toString());
            const QString timezone = object["timezone"].toString();
            double latitude = object["lat"].toDouble(qQNaN());
            double longitude = object["lon"].toDouble(qQNaN());
            const QString location = object["loc"].toString();
            if (!location.isEmpty()) {
                const QStringList parts = location.split(',');
                if (parts.size() == 2) { latitude = parts[0].toDouble(); longitude = parts[1].toDouble(); }
            }
            if (!timezone.isEmpty()) ui->comboBox_14->setCurrentText(timezone);
            if (!qIsNaN(latitude) && !qIsNaN(longitude)) {
                lineEditLatitude->setText(QString::number(latitude, 'f', 6));
                lineEditLongitude->setText(QString::number(longitude, 'f', 6));
            }
            const QString country = object["country"].toString(object["countryCode"].toString());
            if (ui->tabWidget_6->currentIndex() == 0) {
                const QMap<QString, QString> localeByCountry{{"CN", "zh-CN,zh;q=0.9,en;q=0.8"}, {"US", "en-US,en;q=0.9"},
                    {"GB", "en-GB,en;q=0.9"}, {"JP", "ja-JP,ja;q=0.9,en;q=0.8"}, {"DE", "de-DE,de;q=0.9,en;q=0.8"},
                    {"FR", "fr-FR,fr;q=0.9,en;q=0.8"}, {"CA", "en-CA,en;q=0.9,fr;q=0.7"}, {"AU", "en-AU,en;q=0.9"}};
                if (localeByCountry.contains(country.toUpper())) lineEditAcceptLanguage->setText(localeByCountry.value(country.toUpper()));
            }
            QMessageBox::information(this, "Proxy test", QString("Connected\nIP: %1\nCountry: %2\nTimezone: %3").arg(ip, country, timezone));
            reply->deleteLater(); manager->deleteLater();
        });
    });

    ui->checkBox->setText("Canvas");
    ui->checkBox_2->setText("Audio");
    ui->checkBox_3->setText("ClientRects");
    ui->checkBox_4->setText("WebGL");
    ui->checkBox_5->setText("Fonts");
    ui->checkBox_6->setText("Media devices");
    ui->lineEdit_2->setPlaceholderText("请输入环境名称");
    ui->lineEdit_3->setPlaceholderText("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 ...");
    ui->lineEdit_4->setPlaceholderText("支持 JSON、Netscape、Name=Value");
    ui->lineEdit_5->setPlaceholderText("请输入备注");
    ui->textEdit->setPlaceholderText("每行一个启动标签页 URL");

    ui->page_2->setStyleSheet("QWidget#page_2 { background: #f3f5f8; }");
    ui->scrollArea->setFrameShape(QFrame::NoFrame);
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    ui->scrollAreaWidgetContents->setStyleSheet("QWidget#scrollAreaWidgetContents { background: #ffffff; }");
    ui->gridLayout_3->setContentsMargins(26, 20, 26, 18);
    ui->gridLayout_3->setHorizontalSpacing(18);
    ui->gridLayout_3->setVerticalSpacing(14);
    ui->gridLayout_6->setContentsMargins(34, 26, 22, 26);
    ui->gridLayout_6->setHorizontalSpacing(24);
    ui->formLayout_2->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->formLayout_2->setFormAlignment(Qt::AlignTop);
    ui->formLayout_2->setHorizontalSpacing(18);
    ui->formLayout_2->setVerticalSpacing(16);

    QWidget *sectionTabs = new QWidget(ui->page_2);
    QHBoxLayout *sectionTabsLayout = new QHBoxLayout(sectionTabs);
    sectionTabsLayout->setContentsMargins(0, 0, 0, 0);
    sectionTabsLayout->setSpacing(8);
    QString sectionButtonStyle =
        "QPushButton { background: transparent; color: #526071; border: none; border-bottom: 2px solid transparent; padding: 11px 15px; font-size: 13px; font-weight: 500; }"
        "QPushButton:hover { color: #1d4ed8; background: #f6f8fb; }"
        "QPushButton:checked { color: #1d4ed8; border-bottom-color: #2563eb; font-weight: 700; }";
    QButtonGroup *sectionGroup = new QButtonGroup(this);
    sectionGroup->setExclusive(true);
    const QStringList sectionNames = {QString::fromUtf8("基础设置"), QString::fromUtf8("代理信息"), QString::fromUtf8("账号平台"), QString::fromUtf8("指纹配置"), QString::fromUtf8("高级设置")};
    const QList<int> sectionRows = {0, 7, 8, 10, 18};
    for (int i = 0; i < sectionNames.size(); ++i) {
        QPushButton *btn = new QPushButton(sectionNames[i], sectionTabs);
        btn->setCheckable(true);
        btn->setStyleSheet(sectionButtonStyle);
        btn->setCursor(Qt::PointingHandCursor);
        sectionGroup->addButton(btn, i);
        sectionTabsLayout->addWidget(btn);
        if (i == 0) btn->setChecked(true);
    }
    sectionTabsLayout->addStretch();
    ui->gridLayout_3->addWidget(sectionTabs, 1, 0, Qt::AlignTop);
    ui->gridLayout_3->setRowStretch(0, 0);
    ui->gridLayout_3->setRowStretch(1, 0);
    ui->gridLayout_3->setRowStretch(2, 1);
    ui->gridLayout_3->setRowStretch(3, 0);
    ui->horizontalLayout_4->setContentsMargins(0, 8, 0, 0);
    ui->horizontalLayout_4->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(sectionGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this, sectionRows](int id) {
        if (id < 0 || id >= sectionRows.size()) return;
        QLayoutItem *item = ui->formLayout_2->itemAt(sectionRows[id], QFormLayout::LabelRole);
        if (!item || !item->widget()) return;
        ui->scrollArea->ensureWidgetVisible(item->widget(), 0, 24);
    });

    QFrame *summaryPanel = new QFrame(ui->scrollAreaWidgetContents);
    summaryPanel->setObjectName("fingerprintSummaryPanel");
    summaryPanel->setMinimumWidth(0);
    summaryPanel->setMaximumWidth(QWIDGETSIZE_MAX);
    summaryPanel->setStyleSheet(
        "QFrame#fingerprintSummaryPanel { background: #f8fafc; border: 1px solid #edf0f5; border-radius: 8px; }"
        "QLabel { color: #7b8497; font-size: 12px; }"
        "QLabel#summaryTitle { color: #17203a; font-size: 15px; font-weight: 700; }"
        "QLabel[role='value'] { color: #17203a; font-size: 12px; }");
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    summaryLayout->setSpacing(11);
    QLabel *summaryTitle = new QLabel(QString::fromUtf8("概要"), summaryPanel);
    summaryTitle->setObjectName("summaryTitle");
    summaryLayout->addWidget(summaryTitle);
    QFrame *summaryLine = new QFrame(summaryPanel);
    summaryLine->setFrameShape(QFrame::HLine);
    summaryLine->setStyleSheet("color: #e7ebf2;");
    summaryLayout->addWidget(summaryLine);

    auto addSummaryRow = [summaryPanel, summaryLayout](const QString &name, const QString &objectName) {
        QWidget *row = new QWidget(summaryPanel);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        QLabel *label = new QLabel(name, row);
        QLabel *value = new QLabel("-", row);
        value->setObjectName(objectName);
        value->setProperty("role", "value");
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setWordWrap(true);
        rowLayout->addWidget(label);
        rowLayout->addWidget(value, 1);
        summaryLayout->addWidget(row);
        return value;
    };
    QLabel *summaryBrowser = addSummaryRow(QString::fromUtf8("浏览器"), "summaryBrowser");
    QLabel *summaryUA = addSummaryRow("User-Agent", "summaryUA");
    QLabel *summaryWebRtc = addSummaryRow("WebRTC", "summaryWebRtc");
    QLabel *summaryTimezone = addSummaryRow(QString::fromUtf8("时区"), "summaryTimezone");
    QLabel *summaryLanguage = addSummaryRow(QString::fromUtf8("语言"), "summaryLanguage");
    QLabel *summaryResolution = addSummaryRow(QString::fromUtf8("分辨率"), "summaryResolution");
    QLabel *summaryCanvas = addSummaryRow("Canvas", "summaryCanvas");
    QLabel *summaryWebgl = addSummaryRow(QString::fromUtf8("WebGL图像"), "summaryWebgl");
    QLabel *summaryAudio = addSummaryRow("AudioContext", "summaryAudio");
    QLabel *summaryMedia = addSummaryRow(QString::fromUtf8("媒体设备"), "summaryMedia");
    summaryLayout->addStretch();
    ui->gridLayout_6->addWidget(summaryPanel, 1, 0);

    auto updateSummary = [=]() {
        const QString ua = ui->lineEdit_3->text().trimmed();
        summaryBrowser->setText(ui->comboBox_2->currentText().isEmpty() ? "-" : ui->comboBox_2->currentText());
        summaryUA->setText(ua.isEmpty() ? QString::fromUtf8("基于浏览器默认") : ua);
        summaryWebRtc->setText(ui->tabWidget_3->tabText(ui->tabWidget_3->currentIndex()));
        const QString timezoneValue = ui->tabWidget_4->currentIndex() == 1 ? QString::fromUtf8("系统真实") : ui->comboBox_14->currentText();
        summaryTimezone->setText(ui->tabWidget_4->tabText(ui->tabWidget_4->currentIndex()) + " / " + timezoneValue);
        summaryLanguage->setText(lineEditAcceptLanguage->text().section(',', 0, 0));
        QString resolutionValue = QString::fromUtf8("随机");
        if (ui->tabWidget_8->currentIndex() == 1) resolutionValue = comboBoxResolutionPreset->currentText();
        else if (ui->tabWidget_8->currentIndex() == 2) resolutionValue = lineEditResolution->text();
        summaryResolution->setText(resolutionValue);
        summaryCanvas->setText(ui->checkBox->isChecked() ? QString::fromUtf8("噪音") : QString::fromUtf8("真实"));
        summaryWebgl->setText(ui->checkBox_4->isChecked() ? QString::fromUtf8("噪音") : ui->tabWidget_2->tabText(ui->tabWidget_2->currentIndex()));
        summaryAudio->setText(ui->checkBox_2->isChecked() ? QString::fromUtf8("噪音") : QString::fromUtf8("真实"));
        summaryMedia->setText(ui->checkBox_6->isChecked() ? QString::fromUtf8("噪音 [Auto]") : QString::fromUtf8("真实"));
    };
    for (QComboBox *cb : {ui->comboBox_2, ui->comboBox_14, comboBoxResolutionPreset}) {
        connect(cb, &QComboBox::currentTextChanged, this, [updateSummary]() { updateSummary(); });
    }
    for (QTabWidget *tw : {ui->tabWidget_2, ui->tabWidget_3, ui->tabWidget_4, ui->tabWidget_6, ui->tabWidget_8}) {
        connect(tw, &QTabWidget::currentChanged, this, [updateSummary]() { updateSummary(); });
    }
    connect(lineEditAcceptLanguage, &QLineEdit::textChanged, this, [updateSummary]() { updateSummary(); });
    connect(lineEditResolution, &QLineEdit::textChanged, this, [updateSummary]() { updateSummary(); });
    for (QCheckBox *cb : {ui->checkBox, ui->checkBox_2, ui->checkBox_4, ui->checkBox_6}) {
        connect(cb, &QCheckBox::toggled, this, [updateSummary]() { updateSummary(); });
    }
    connect(ui->lineEdit_3, &QLineEdit::textChanged, this, [updateSummary]() { updateSummary(); });
    connect(ui->pushButton_5, &QPushButton::clicked, this, [this, updateSummary]() {
        resetCreateBrowserForm();
        updateSummary();
    });
    resetCreateBrowserForm();
    updateSummary();

    ui->stackedWidget->setCurrentWidget(ui->page);

    // ================== 【第一步：基础配置主数据表 (左侧)】 ==================
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setAlternatingRowColors(true);

    // ================== 【第二步：纯代码动态创建右侧固定表】 ==================
    rightTable = new QTableWidget(this);
    rightTable->setColumnCount(1);
    rightTable->setHorizontalHeaderLabels(QStringList() << "操作");
    rightTable->verticalHeader()->setVisible(false);
    rightTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    rightTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    rightTable->setAlternatingRowColors(true);

    rightTable->setColumnWidth(0, 130);
    rightTable->setFixedWidth(132);
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

    // ================== 【第三步：把固定表塞进布局中】 ==================
    if (ui->tableWidget->parentWidget()->layout()) {
        QLayout *parentLayout = ui->tableWidget->parentWidget()->layout();
        QWidget *tableContainer = new QWidget(this);
        QHBoxLayout *hLayout = new QHBoxLayout(tableContainer);
        hLayout->setSpacing(0);
        hLayout->setContentsMargins(0, 0, 0, 0);

        parentLayout->replaceWidget(ui->tableWidget, tableContainer);
        hLayout->addWidget(ui->tableWidget, 1);
        hLayout->addWidget(rightTable, 0);
    }

    // ================== 【第四步：绑定垂直滚动条联动】 ==================
    connect(ui->tableWidget->verticalScrollBar(), &QScrollBar::valueChanged,
            rightTable->verticalScrollBar(), &QScrollBar::setValue);
    connect(rightTable->verticalScrollBar(), &QScrollBar::valueChanged,
            ui->tableWidget->verticalScrollBar(), &QScrollBar::setValue);

    // 美化表格皮肤风格 —— 现代圆角卡片表
    QString tableStyle =
        "QTableWidget {"
        "  border: 1px solid #e8e8e8;"
        "  border-radius: 10px;"
        "  background-color: #ffffff;"
        "  gridline-color: #f5f5f5;"
        "  outline: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f8f9fd;"
        "  color: #636e72;"
        "  padding: 10px 8px;"
        "  border: none;"
        "  border-bottom: 2px solid #e8e8e8;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.5px;"
        "}"
        "QTableWidget::item {"
        "  padding: 8px 6px;"
        "  color: #2d3436;"
        "  border-bottom: 1px solid #f8f8f8;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #e8f4fd;"
        "  color: #0984e3;"
        "}"
        "QTableWidget::item:hover {"
        "  background-color: #f8f9fd;"
        "}";
    ui->tableWidget->setStyleSheet(tableStyle);
    rightTable->setStyleSheet(tableStyle);

    // 同样美化其他表格
    QString tableStyle2 =
        "QTableWidget {"
        "  border: 1px solid #e8e8e8;"
        "  border-radius: 10px;"
        "  background-color: #ffffff;"
        "  gridline-color: #f5f5f5;"
        "  outline: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f8f9fd;"
        "  color: #636e72;"
        "  padding: 10px 8px;"
        "  border: none;"
        "  border-bottom: 2px solid #e8e8e8;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "}"
        "QTableWidget::item {"
        "  padding: 8px 6px;"
        "  color: #2d3436;"
        "  border-bottom: 1px solid #f8f8f8;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #e8f4fd;"
        "  color: #0984e3;"
        "}"
        "QTableWidget::item:hover {"
        "  background-color: #f8f9fd;"
        "}";
    ui->tableWidget_2->setStyleSheet(tableStyle2);
    ui->tableWidget_2->verticalHeader()->setVisible(false);
    ui->tableWidget_2->setAlternatingRowColors(true);
    ui->tableWidget_2->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget_2->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableWidget_3->setStyleSheet(tableStyle2);
    ui->tableWidget_3->verticalHeader()->setVisible(false);
    ui->tableWidget_3->setAlternatingRowColors(true);
    ui->tableWidget_3->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget_3->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableWidget->horizontalHeader()->setFixedHeight(35);
    rightTable->horizontalHeader()->setFixedHeight(35);
    ui->tableWidget_2->horizontalHeader()->setFixedHeight(35);
    ui->tableWidget_3->horizontalHeader()->setFixedHeight(35);

    // 加载真实数据
    loadEnvironmentsFromJson();
    loadModulesFromJson();

    // ========== 左侧导航栏美化 ==========
    // 应用名称/Logo区域
    ui->label->setStyleSheet(
        "font-size: 22px; font-weight: 800; color: #1d4ed8;"
        " padding: 24px 22px 18px 22px;"
        " background: transparent;"
        " border-bottom: 1px solid #e5e9ef;");
    ui->label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 左侧容器背景（不设宽度限制，由 grid 布局自动处理）
    QFrame *navContainer = new QFrame(ui->centralwidget);
    navContainer->setObjectName("navigationPanel");
    ui->gridLayout->removeItem(ui->verticalLayout);
    QVBoxLayout *navigationLayout = new QVBoxLayout(navContainer);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    navigationLayout->setSpacing(0);
    navigationLayout->addLayout(ui->verticalLayout);
    ui->gridLayout->addWidget(navContainer, 0, 0);
    ui->gridLayout->setContentsMargins(0, 0, 0, 0);
    ui->gridLayout->setHorizontalSpacing(0);
    ui->gridLayout->setColumnStretch(0, 0);
    ui->gridLayout->setColumnStretch(1, 1);
    navContainer->setFixedWidth(218);
    navContainer->setStyleSheet(
        "QFrame#navigationPanel { background-color: #ffffff; border: none; border-right: 1px solid #dfe4ea; }");

    // 导航按钮图标映射（使用 Windows 兼容的 Unicode 字符）
    QMap<QPushButton*, QStyle::StandardPixmap> navIcons;
    navIcons[ui->pushButton_7]  = QStyle::SP_FileDialogNewFolder;
    navIcons[ui->pushButton_6]  = QStyle::SP_ComputerIcon;
    navIcons[ui->pushButton_4]  = QStyle::SP_DriveNetIcon;
    navIcons[ui->pushButton_2]  = QStyle::SP_CommandLink;
    navIcons[ui->pushButton_3]  = QStyle::SP_BrowserReload;
    navIcons[ui->pushButton]    = QStyle::SP_FileDialogDetailedView;
    navIcons[ui->pushButton_11] = QStyle::SP_MessageBoxInformation;

    // 所有导航按钮统一样式 —— 带左侧彩色指示条
    QString navBtnStyle =
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #445164;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 11px 16px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  text-align: left;"
        "  margin: 2px 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #f2f5f9;"
        "  color: #1d4ed8;"
        "}";

    QList<QPushButton*> navBtns = {
        ui->pushButton_7, ui->pushButton_6, ui->pushButton_4,
        ui->pushButton_2, ui->pushButton_3, ui->pushButton,
        ui->pushButton_11
    };
    for (QPushButton *btn : navBtns) {
        btn->setStyleSheet(navBtnStyle);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(42);
        btn->setIconSize(QSize(17, 17));

        // 添加图标前缀
        if (navIcons.contains(btn)) {
            btn->setIcon(style()->standardIcon(navIcons[btn]));
        }
    }

    // Selection is represented by a state, so every page uses the same style.
    ui->pushButton_6->setChecked(true);
    auto updateNavHighlight = [navBtns](QPushButton *activeBtn) {
        for (QPushButton *button : navBtns) button->setChecked(button == activeBtn);
    };

    // 给所有导航按钮连接高亮更新
    for (QPushButton* b : navBtns) {
        connect(b, &QPushButton::clicked, this, [updateNavHighlight, b]() {
            updateNavHighlight(b);
        });
    }




    // ??【完工弹簧】：在整个界面所有内容初始化完的最后一行加上弹簧，压满并支撑滚动区域
    m_envLayout->addStretch();

#ifndef PROFILEPOWER_PROXY_TESTS
    QTimer::singleShot(100, this, &MainWindow::checkPythonAndDependencies);
#endif

    writeLog("初始化", "UI 构建完成，已安排 Python 环境检测");


    // ========== 页面标题美化 ==========
    QString pageTitleStyle = "font-size: 21px; font-weight: 700; color: #172033; background: transparent; border: none; padding: 8px 0px 12px 2px;";
    ui->label_3->setStyleSheet(pageTitleStyle);   // 环境管理
    ui->label_2->setStyleSheet(pageTitleStyle);   // 新建浏览器
    ui->label_4->setStyleSheet(pageTitleStyle);   // 代理管理
    ui->label_5->setStyleSheet(pageTitleStyle);   // API调用
    ui->label_6->setStyleSheet(pageTitleStyle);   // 自动化管理
    ui->label_19->setStyleSheet(pageTitleStyle);  // 模块管理

    // ========== 确定/取消按钮美化 ==========
    QString confirmBtnStyle =
        "QPushButton {"
        "  background-color: #2563eb; color: white; border: 1px solid #2563eb;"
        "  border-radius: 6px; padding: 10px 34px; font-size: 13px; font-weight: 700;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; border-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }";
    QString cancelBtnStyle =
        "QPushButton {"
        "  background-color: #ffffff; color: #334155;"
        "  border: 1px solid #cbd5e1; border-radius: 6px;"
        "  padding: 10px 34px; font-size: 13px; font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #f8fafc; border-color: #94a3b8; }"
        "QPushButton:pressed { background-color: #eef2f6; }";

    ui->pushButton_8->setStyleSheet(confirmBtnStyle);  // 确定
    ui->pushButton_5->setStyleSheet(cancelBtnStyle);   // 取消

    // ---- ?? 选择文件夹按钮（pushButton_10）美化 ----
    QString selectFolderStyle =
        "QPushButton {"
        "  background-color: #0984e3;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 9px 24px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0770c4;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #065da6;"
        "}";
    ui->pushButton_10->setStyleSheet(selectFolderStyle);
    ui->pushButton_10->setCursor(Qt::PointingHandCursor);
    ui->pushButton_10->setFixedHeight(38);

    // ==================================================================
    // 【formLayout_2 控件美化】— 新建浏览器页面的输入框/下拉框/标签/选项卡
    // ==================================================================

    // ---- ① 所有 QLineEdit（名称/UA/Cookie/备注/自定义WebGL等）----
    QString lineEditStyle =
        "QLineEdit {"
        "  border: 1.5px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  background-color: #ffffff;"
        "  color: #2d3436;"
        "  font-size: 13px;"
        "  min-height: 20px;"
        "  selection-background-color: #74b9ff;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #0984e3;"
        "  background-color: #f8fbff;"
        "}"
        "QLineEdit:hover {"
        "  border-color: #b2bec3;"
        "}";

    for (QLineEdit* le : {
             ui->lineEdit_2, ui->lineEdit_3, ui->lineEdit_4, ui->lineEdit_5,
             ui->lineEdit_6
         }) {
        le->setStyleSheet(lineEditStyle);
    }

    // ---- ② 所有 QComboBox（浏览器/操作系统/UA/代理类型/账号平台等）----
    QString comboStyle =
        "QComboBox {"
        "  border: 1.5px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  padding: 7px 14px;"
        "  background-color: #ffffff;"
        "  color: #2d3436;"
        "  font-size: 13px;"
        "  min-height: 20px;"
        "}"
        "QComboBox:hover { border-color: #b2bec3; }"
        "QComboBox:focus { border-color: #0984e3; }"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 28px;"
        "  border: none;"
        "}"
        "QComboBox::down-arrow {"
        "  border: 4.5px solid transparent;"
        "  border-top-color: #636e72;"
        "  margin-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  background: #ffffff;"
        "  selection-background-color: #e8f4fd;"
        "  selection-color: #0984e3;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  padding: 8px 12px;"
        "  border-radius: 4px;"
        "  min-height: 22px;"
        "}";

    for (QComboBox* cb : {
             ui->comboBox_2, ui->comboBox_3, ui->comboBox_6, ui->comboBox_8,
             ui->comboBox_7, ui->comboBox_5, ui->comboBox_4, ui->comboBox_9,
             ui->comboBox_11, ui->comboBox_12, ui->comboBox_10
             // ui->comboBox_13, ui->comboBox_14, ui->comboBox
         }) {
        cb->setStyleSheet(comboStyle);
    }

    // ---- ③ QFontComboBox（分组选择）----
    QString fontComboStyle =
        "QFontComboBox {"
        "  border: 1.5px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  padding: 7px 14px;"
        "  background-color: #ffffff;"
        "  color: #2d3436;"
        "  font-size: 13px;"
        "  min-height: 20px;"
        "}"
        "QFontComboBox:hover { border-color: #b2bec3; }"
        "QFontComboBox:focus { border-color: #0984e3; }"
        "QFontComboBox::drop-down {"
        "  width: 28px; border: none;"
        "}"
        "QFontComboBox::down-arrow {"
        "  border: 4.5px solid transparent;"
        "  border-top-color: #636e72;"
        "  margin-right: 6px;"
        "}"
        "QFontComboBox QAbstractItemView {"
        "  border: 1px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  background: #ffffff;"
        "  selection-background-color: #e8f4fd;"
        "  selection-color: #0984e3;"
        "}";

    ui->fontComboBox->setStyleSheet(fontComboStyle);
    ui->fontComboBox_2->setStyleSheet(fontComboStyle);

    // ---- ④ QTextEdit（标签页）----
    QString textEditStyle =
        "QTextEdit {"
        "  border: 1.5px solid #e0e3e8;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  background-color: #ffffff;"
        "  color: #2d3436;"
        "  font-size: 13px;"
        "  selection-background-color: #74b9ff;"
        "}"
        "QTextEdit:focus {"
        "  border-color: #0984e3;"
        "  background-color: #f8fbff;"
        "}"
        "QTextEdit:hover {"
        "  border-color: #b2bec3;"
        "}";
    ui->textEdit->setStyleSheet(textEditStyle);

    // ---- ⑤ QTabWidget（代理方式/WebRTC/时区/地理位置/语言/分辨率/字体/WebGL）----
    QString tabWidgetStyle =
        "QTabWidget::pane {"
        "  border: none;"
        "  background: transparent;"
        "  top: -1px;"
        "}"
        "QTabBar { background: #f0f2f5; border: none; border-radius: 8px; }"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: #334155;"
        "  border: 1px solid transparent;"
        "  border-radius: 6px;"
        "  padding: 7px 16px;"
        "  font-size: 12px;"
        "  margin: 3px 2px 3px 3px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #ffffff;"
        "  color: #2563eb;"
        "  border: 1px solid #dbe3ef;"
        "  font-weight: 700;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  color: #1d4ed8;"
        "  background: #e7ebf1;"
        "}";
    const QString proxyTabWidgetStyle = tabWidgetStyle +
        "QTabWidget#tabWidget::pane { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 8px; top: 6px; padding: 10px; }";

    for (QTabWidget* tw : {
             ui->tabWidget, proxyInputTabs, ui->tabWidget_2, ui->tabWidget_3,
             ui->tabWidget_4, ui->tabWidget_5, ui->tabWidget_6,
             ui->tabWidget_7, ui->tabWidget_8, ui->tabWidget_9
         }) {
        tw->setStyleSheet(tabWidgetStyle);
        tw->tabBar()->setDrawBase(false);
        tw->tabBar()->setExpanding(false);
    }
    ui->tabWidget->setStyleSheet(proxyTabWidgetStyle);

    // ---- ⑥ QCheckBox（硬件噪音）----
    QString checkBoxStyle =
        "QCheckBox {"
        "  spacing: 8px;"
        "  font-size: 13px;"
        "  color: #2d3436;"
        "}"
        "QCheckBox::indicator {"
        "  width: 18px;"
        "  height: 18px;"
        "  border: 1.5px solid #b2bec3;"
        "  border-radius: 4px;"
        "  background: #ffffff;"
        "}"
        "QCheckBox::indicator:hover { border-color: #0984e3; }"
        "QCheckBox::indicator:checked {"
        "  background: #0984e3;"
        "  border-color: #0984e3;"
        "}";

    for (QCheckBox* cb : {
             ui->checkBox, ui->checkBox_2, ui->checkBox_3,
             ui->checkBox_4, ui->checkBox_5, ui->checkBox_6
         }) {
        cb->setStyleSheet(checkBoxStyle);
    }

    // ---- ⑦ form 标签文字优化（统一字体颜色）----
    QString formLabelStyle = "font-size: 13px; font-weight: 500; color: #2d3436; padding-right: 8px;";
    for (QLabel* lbl : ui->scrollArea->findChildren<QLabel*>()) {
        // 只美化和 form 相关的标签（避开 page title 等）
        if (lbl->parentWidget() == ui->scrollAreaWidgetContents ||
            (lbl->parentWidget() && lbl->parentWidget()->parentWidget() == ui->scrollAreaWidgetContents)) {
            lbl->setStyleSheet(formLabelStyle);
        }
    }

    // ---- ⑧ QLineEdit 占位符提示文字颜色优化 ----
    for (QLineEdit* le : ui->scrollArea->findChildren<QLineEdit*>()) {
        le->setPlaceholderText(le->placeholderText()); // 触发重绘
    }

    // Final pass also covers controls created dynamically at runtime.
    const QString refinedInputStyle =
        "QLineEdit { background: #ffffff; color: #172033; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px 12px; min-height: 21px; font-size: 13px; }"
        "QLineEdit:hover { border-color: #94a3b8; }"
        "QLineEdit:focus { border: 1px solid #2563eb; background: #ffffff; selection-background-color: #bfdbfe; }"
        "QLineEdit:disabled { background: #f1f5f9; color: #94a3b8; }";
    for (QLineEdit *edit : ui->page_2->findChildren<QLineEdit*>()) {
        if (!qobject_cast<QComboBox *>(edit->parentWidget())) edit->setStyleSheet(refinedInputStyle);
    }

    const QString refinedComboStyle =
        "QComboBox { background: #ffffff; color: #172033; border: 1px solid #cbd5e1; border-radius: 6px; padding: 7px 12px; min-height: 23px; font-size: 13px; }"
        "QComboBox:hover { border-color: #94a3b8; }"
        "QComboBox:focus { border-color: #2563eb; }"
        "QComboBox::drop-down { width: 28px; border: none; }"
        "QComboBox QAbstractItemView { background: #ffffff; color: #172033; border: 1px solid #cbd5e1; selection-background-color: #eaf2ff; selection-color: #1d4ed8; outline: none; padding: 4px; }";
    for (QComboBox *combo : ui->page_2->findChildren<QComboBox*>()) combo->setStyleSheet(refinedComboStyle);

    const QString utilityButtonStyle =
        "QPushButton { background: #ffffff; color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px 16px; min-height: 22px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: #f8fafc; border-color: #94a3b8; color: #1d4ed8; }"
        "QPushButton:pressed { background: #eef2f6; }";
    ui->pushButton_9->setStyleSheet(utilityButtonStyle);
    ui->pushButton_9->setCursor(Qt::PointingHandCursor);

    ui->horizontalLayout_4->setSpacing(10);
    ui->pushButton_8->setMinimumSize(110, 40);
    ui->pushButton_5->setMinimumSize(110, 40);

    applyInterfaceStyle();



}

//析构
MainWindow::~MainWindow()
{
    const QList<QProcess *> processes = findChildren<QProcess *>();
    for (QProcess *process : processes) {
        if (process->state() == QProcess::NotRunning) continue;
        process->disconnect();
        process->terminate();
    }

    QElapsedTimer shutdownTimer;
    shutdownTimer.start();
    for (QProcess *process : processes) {
        if (process->state() == QProcess::NotRunning) continue;
        const int remaining = qMax(0, 1500 - static_cast<int>(shutdownTimer.elapsed()));
        if (remaining > 0) process->waitForFinished(remaining);
    }

    for (QProcess *process : processes) {
        if (process->state() == QProcess::NotRunning) continue;
        process->kill();
    }
    for (QProcess *process : processes) {
        if (process->state() != QProcess::NotRunning) process->waitForFinished(1000);
    }

    writeLog("退出", "ProfilePower 程序退出");
    if (m_logFile && m_logFile->isOpen()) {
        m_logFile->flush();
        m_logFile->close();
    }
    delete ui;
}


// ========== 日志系统实现 ==========
void MainWindow::writeLog(const QString &category, const QString &message)
{
    if (!m_logFile || !m_logFile->isOpen()) return;

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString line = QString("[%1] [%2] %3\n").arg(timestamp, category, message);

    QTextStream stream(m_logFile);
    stream << line;
    stream.flush();  // 每条立即落盘，防止崩溃丢失
}





void MainWindow::checkPythonAndDependencies()
{
    const QString python = []() {
        QString executable = QStandardPaths::findExecutable("python");
        if (executable.isEmpty()) executable = QStandardPaths::findExecutable("python3");
        return executable;
    }();

    if (python.isEmpty()) {
        writeLog("Environment check", "Python was not found; automatic installer execution is disabled.");
        QMessageBox::warning(
            this,
            "Python required",
            "Python 3.10 or later was not found.\n\n"
            "Download it from the official site and enable Add Python to PATH:\n"
            "https://www.python.org/downloads/windows/");
        return;
    }

    QProcess *checkProcess = new QProcess(this);
    checkProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(checkProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, checkProcess, python](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(checkProcess->readAllStandardOutput()).trimmed();
        const QString errorOutput = QString::fromUtf8(checkProcess->readAllStandardError()).trimmed();
        checkProcess->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            writeLog("Environment check", QString("Dependency check failed: %1").arg(errorOutput));
            return;
        }

        const QStringList missingPackages = output.split(',', Qt::SkipEmptyParts);
        if (missingPackages.isEmpty()) {
            writeLog("Environment check", "Python dependencies are available.");
            return;
        }

        const auto answer = QMessageBox::question(
            this,
            "Install Python dependencies",
            QString("Missing dependencies: %1\n\nInstall them now?").arg(missingPackages.join(", ")),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer != QMessageBox::Yes) return;

        QProgressDialog *progress = new QProgressDialog("Installing Python dependencies...", QString(), 0, 0, this);
        progress->setWindowTitle("Python dependencies");
        progress->setCancelButton(nullptr);
        progress->setMinimumDuration(0);
        progress->setAttribute(Qt::WA_DeleteOnClose);
        progress->show();

        QProcess *pipProcess = new QProcess(progress);
        pipProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(pipProcess, &QProcess::errorOccurred, this, [this, pipProcess, progress](QProcess::ProcessError) {
            writeLog("Dependency install", QString("Could not start pip: %1").arg(pipProcess->errorString()));
            progress->close();
            pipProcess->deleteLater();
            QMessageBox::warning(this, "Dependency install", "The pip process could not be started.");
        });
        connect(pipProcess, &QProcess::readyReadStandardOutput, this, [this, pipProcess]() {
            const QString text = QString::fromLocal8Bit(pipProcess->readAllStandardOutput()).trimmed();
            if (!text.isEmpty()) writeLog("Dependency install", text);
        });
        connect(pipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, pipProcess, progress, python, missingPackages](int code, QProcess::ExitStatus processStatus) {
            pipProcess->deleteLater();
            if (processStatus != QProcess::NormalExit || code != 0) {
                progress->close();
                QMessageBox::warning(this, "Dependency install", "pip could not install the required packages. See the application log for details.");
                return;
            }
            if (!missingPackages.contains("playwright")) {
                progress->close();
                QMessageBox::information(this, "Dependency install", "Python dependencies were installed successfully.");
                return;
            }

            progress->setLabelText("Installing Playwright Chromium...");
            QProcess *playwrightProcess = new QProcess(progress);
            playwrightProcess->setProcessChannelMode(QProcess::MergedChannels);
            connect(playwrightProcess, &QProcess::errorOccurred, this,
                    [this, playwrightProcess, progress](QProcess::ProcessError) {
                writeLog("Playwright install", QString("Could not start Playwright: %1").arg(playwrightProcess->errorString()));
                progress->close();
                playwrightProcess->deleteLater();
                QMessageBox::warning(this, "Dependency install", "The Playwright installer could not be started.");
            });
            connect(playwrightProcess, &QProcess::readyReadStandardOutput, this, [this, playwrightProcess]() {
                const QString text = QString::fromLocal8Bit(playwrightProcess->readAllStandardOutput()).trimmed();
                if (!text.isEmpty()) writeLog("Playwright install", text);
            });
            connect(playwrightProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, playwrightProcess, progress](int browserCode, QProcess::ExitStatus browserStatus) {
                playwrightProcess->deleteLater();
                progress->close();
                if (browserStatus == QProcess::NormalExit && browserCode == 0) {
                    QMessageBox::information(this, "Dependency install", "Python dependencies and Playwright Chromium were installed successfully.");
                } else {
                    QMessageBox::warning(this, "Dependency install", "Dependencies were installed, but Playwright Chromium installation failed.");
                }
            });
            playwrightProcess->start(python, {"-m", "playwright", "install", "chromium"});
        });
        QStringList arguments{"-m", "pip", "install"};
        arguments.append(missingPackages);
        pipProcess->start(python, arguments);
    });
    connect(checkProcess, &QProcess::errorOccurred, this, [this, checkProcess](QProcess::ProcessError) {
        writeLog("Environment check", QString("Could not start dependency check: %1").arg(checkProcess->errorString()));
        checkProcess->deleteLater();
    });
    const QString checkScript =
        "import importlib.util;"
        "print(','.join(name for name in ('playwright','requests') if importlib.util.find_spec(name) is None))";
    checkProcess->start(python, {"-c", checkScript});
    return;

}




void MainWindow::createEnvCard(const EnvironmentInfo &info)
{
    // 1. 创建环境卡片容器 (QFrame)
    QFrame *cardFrame = new QFrame(this);

    // 【修复先前的拼写错误并支持横向拉伸】
    cardFrame->setMinimumWidth(0);
    cardFrame->setMaximumWidth(QWIDGETSIZE_MAX);
    cardFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    cardFrame->setObjectName("EnvCard");
    cardFrame->setStyleSheet(
        "QFrame#EnvCard {"
        "   background-color: #ffffff;"
        "   border: 1px solid #eef0f4;"
        "   border-radius: 12px;"
        "}"
        "QFrame#EnvCard:hover {"
        "   border-color: #b0d4f1;"
        "}"
        );

    // 添加卡片阴影
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(cardFrame);
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(2);
    shadow->setColor(QColor(0, 0, 0, 20));
    cardFrame->setGraphicsEffect(shadow);

    // 【关键】：把当前环境特有的路径和名字存进 cardFrame 属性里
    QString envConfigPath = info.configPath;
    cardFrame->setProperty("config_path", envConfigPath);
    cardFrame->setProperty("env_name", info.name);

    QVBoxLayout *cardMainLayout = new QVBoxLayout(cardFrame);
    cardMainLayout->setContentsMargins(20, 16, 20, 16);
    cardMainLayout->setSpacing(12);

    // ==========================================
    // 2. 环境头部区域 (QHBoxLayout)
    // ==========================================
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *lblTitle = new QLabel(info.name, cardFrame);
    lblTitle->setStyleSheet("font-size: 17px; font-weight: 700; color: #2d3436; background: transparent;");

    QLabel *lblStatus = new QLabel(cardFrame);
    if (info.status == "运行中") {
        lblStatus->setText("  ●  运行中");
        lblStatus->setStyleSheet("color: #00b894; font-size: 12px; font-weight: 600; margin-left: 10px; "
                                 "background: #e6f9f2; border-radius: 10px; padding: 2px 10px;");
    } else {
        lblStatus->setText("  ●  未运行");
        lblStatus->setStyleSheet("color: #748198; font-size: 12px; font-weight: 500; margin-left: 10px; "
                                 "background: #f5f6fa; border-radius: 10px; padding: 2px 10px;");
    }

    QHBoxLayout *titleLeftLayout = new QHBoxLayout();
    titleLeftLayout->addWidget(lblTitle);
    titleLeftLayout->addWidget(lblStatus);
    titleLeftLayout->addStretch();

    QPushButton *btnAttach = new QPushButton("+ 挂载模块", cardFrame);
    btnAttach->setCursor(Qt::PointingHandCursor);
    btnAttach->setStyleSheet(
        "QPushButton { background-color: white; color: #2563eb; border: 1px solid #cfddfa; border-radius: 6px; padding: 7px 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background-color: #edf4ff; }"
        "QPushButton:pressed { background-color: #dbe8ff; }"
        );

    headerLayout->addLayout(titleLeftLayout, 1);
    headerLayout->addSpacing(20);
    headerLayout->addWidget(btnAttach);
    cardMainLayout->addLayout(headerLayout);

    // 分割线
    QFrame *line = new QFrame(cardFrame);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("color: #f0f2f5; background-color: #f0f2f5; max-height: 1px; border: none;");
    cardMainLayout->addWidget(line);

    // ==========================================
    // 3. 已挂载模块区域 (QHBoxLayout)
    // ==========================================
    QHBoxLayout *modulesLayout = new QHBoxLayout();
    modulesLayout->setContentsMargins(0, 0, 0, 0);
    modulesLayout->setSpacing(10);

    QLabel *lblModuleTitle = new QLabel("已挂载模块：", cardFrame);
    lblModuleTitle->setStyleSheet("color: #636e72; font-weight: 600; font-size: 13px; background: transparent;");
    modulesLayout->addWidget(lblModuleTitle);

    // 动态包装渲染已有标签模块的辅助 Lambda 函数 (保持当前 cardFrame 独立作用域)
    auto createTagWidget = [this,cardFrame, modulesLayout, envConfigPath](const QString &modName) {
        if (modName.trimmed().isEmpty()) return;

        QWidget *tagWidget = new QWidget(cardFrame);
        QString bgColor = "#e8f4ff";
        QString textColor = "#1e90ff";
        // if (modName.contains("B站")) { bgColor = "#ffeaf2"; textColor = "#ff6b81"; }
        // else if (modName.contains("TK")) { bgColor = "#fff2e6"; textColor = "#ff9f43"; }

        tagWidget->setStyleSheet(QString("QWidget { background-color: %1; border-radius: 4px; }").arg(bgColor));

        QHBoxLayout *tagLayout = new QHBoxLayout(tagWidget);
        tagLayout->setContentsMargins(8, 4, 8, 4);
        tagLayout->setSpacing(6);

        QLabel *lblTagName = new QLabel(modName, tagWidget);
        lblTagName->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(textColor));

        QPushButton *btnCloseTag = new QPushButton("×", tagWidget);
        btnCloseTag->setFixedSize(14, 14);
        btnCloseTag->setCursor(Qt::PointingHandCursor);
        btnCloseTag->setStyleSheet(
            QString("QPushButton { color: %1; border: none; background: transparent; font-size: 14px; font-weight: bold; line-height: 14px; }"
                    "QPushButton:hover { color: #ff4757; }").arg(textColor)
            );

        tagLayout->addWidget(lblTagName);
        tagLayout->addWidget(btnCloseTag);

        int insertIdx = modulesLayout->count() - 2;
        modulesLayout->insertWidget(insertIdx >= 1 ? insertIdx : 1, tagWidget);

        connect(btnCloseTag, &QPushButton::clicked, tagWidget, [this, envConfigPath, modName]() {
            QFile file(envConfigPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
                file.close();

                QJsonArray modulesArr = json["modules"].toArray();
                QJsonArray updatedArr;
                for (int i = 0; i < modulesArr.size(); ++i) {
                    if (modulesArr[i].toString() != modName) {
                        updatedArr.append(modulesArr[i]);
                    }
                }
                json["modules"] = updatedArr;

                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
                    file.close();
                }
            }

            // 刷新界面（延迟执行，避免删除信号发送者期间出问题）
            QTimer::singleShot(0, this, &MainWindow::refreshEnvCards);
        });
    };



    // 循环遍历加载环境中已挂载的数据
    for (const QString &modName : info.modules) {
        createTagWidget(modName);
    }

    QPushButton *btnAddMod = new QPushButton("+ 添加模块", cardFrame);
    btnAddMod->setCursor(Qt::PointingHandCursor);
    btnAddMod->setStyleSheet(
        "QPushButton { background-color: transparent; color: #9c9c9c; border: 1px dashed #ced6e0; border-radius: 4px; padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { border-color: #1e90ff; color: #1e90ff; }"
        );
    modulesLayout->addWidget(btnAddMod);
    modulesLayout->addStretch();

    cardMainLayout->addLayout(modulesLayout);


    // ==========================================
    // 5. 运行模块按钮区域
    // ==========================================
    QPushButton *btnRunModule = new QPushButton("运行模块", cardFrame);
    btnRunModule->setObjectName("runEnvironmentModulesButton");
    btnRunModule->setIcon(QIcon(":/ui/play.svg"));
    btnRunModule->setCursor(Qt::PointingHandCursor);
    btnRunModule->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: white; border: none; border-radius: 6px; padding: 9px 16px; font-weight: 600; font-size: 13px; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
        );

    // 空信号槽，后续实现运行逻辑
    // connect(btnRunModule, &QPushButton::clicked, this, [this]() {
    //     qDebug() << "运行模块按钮被点击 - 待实现";
    //     // 检查当前环境为运行状态
    //     //环境为 运行中 获取当前挂载的 模块
    //     // 参考
    // });

    // 运行所有已挂载模块
    connect(btnRunModule, &QPushButton::clicked, this, [this, btnRunModule]() {
        // 1. 从父级卡片获取环境配置路径和名称
        QWidget *cardFrame = btnRunModule->parentWidget();
        if (!cardFrame) return;

        QString envConfigPath = cardFrame->property("config_path").toString();
        QString envName = cardFrame->property("env_name").toString();
        if (envConfigPath.isEmpty()) {
            QMessageBox::warning(this, "运行模块", "无法获取环境配置路径！");
            return;
        }

        // 2. 检查环境是否在运行中（查找 tableWidget_2 中该环境的端口）
        QString hostPort;
        bool envRunning = false;
        for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
            QTableWidgetItem *nameItem = ui->tableWidget_2->item(i, 2);
            QTableWidgetItem *portItem = ui->tableWidget_2->item(i, 5);
            if (nameItem && nameItem->text().trimmed() == envName && portItem) {
                hostPort = portItem->text().trimmed();
                envRunning = true;
                break;
            }
        }
        if (!envRunning) {
            QMessageBox::warning(this, "运行模块",
                                 QString("环境【%1】未运行，请先打开浏览器环境！").arg(envName));
            return;
        }

        // 3. 读取环境 config.json 获取已挂载的模块列表
        QFile envFile(envConfigPath);
        if (!envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "运行模块", "无法读取环境配置文件！");
            return;
        }
        QJsonArray mountedModNames = QJsonDocument::fromJson(envFile.readAll()).object()["modules"].toArray();
        envFile.close();

        if (mountedModNames.isEmpty()) {
            QMessageBox::information(this, "运行模块", "该环境未挂载任何模块！");
            return;
        }

        QDir modulesDir(QCoreApplication::applicationDirPath() + "/modules");
        QMap<QString, QPair<QString, QString>> nameToInfo;
        for (const QString &subDir : modulesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile moduleFile(modulesDir.absoluteFilePath(subDir + "/config.json"));
            if (!moduleFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QJsonObject moduleJson = QJsonDocument::fromJson(moduleFile.readAll()).object();
            const QString moduleName = moduleJson["name"].toString().trimmed();
            if (!moduleName.isEmpty()) {
                nameToInfo[moduleName] = qMakePair(modulesDir.absoluteFilePath(subDir),
                                                   moduleJson["id"].toString());
            }
        }

        QStringList requiredFields;
        for (const QJsonValue &mountedValue : mountedModNames) {
            const QString moduleName = mountedValue.toString().trimmed();
            if (!nameToInfo.contains(moduleName)) continue;
            QFile moduleFile(nameToInfo[moduleName].first + "/config.json");
            if (!moduleFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QJsonArray format = QJsonDocument::fromJson(moduleFile.readAll()).object()["format"].toArray();
            QStringList moduleFields;
            for (const QJsonValue &value : format) {
                const QString field = value.toString().trimmed();
                if (!field.isEmpty() && !moduleFields.contains(field)) moduleFields.append(field);
            }
            if (moduleFields.isEmpty()) moduleFields = {"video", "title", "desc"};
            for (const QString &field : moduleFields) {
                if (!requiredFields.contains(field)) requiredFields.append(field);
            }
        }

        // 4. 弹窗收集通用参数（各模块共享同一套 video/title/desc）
        QDialog dialog(this);
        dialog.setWindowTitle(QString("批量运行模块 - %1").arg(envName));
        dialog.resize(420, 260);

        QVBoxLayout *dlgLayout = new QVBoxLayout(&dialog);
        dlgLayout->addWidget(new QLabel(QString("环境: %1 | HOST端口: %2 | 待运行模块数: %3")
                                            .arg(envName, hostPort).arg(mountedModNames.size()), &dialog));

        QFormLayout *formLayout = new QFormLayout();
        QMap<QString, QLineEdit*> paramEditors;
        for (const QString &field : requiredFields) {
            if (field == "video") {
                // video 字段：QLineEdit + 浏览按钮
                QWidget *videoContainer = new QWidget(&dialog);
                QHBoxLayout *videoLayout = new QHBoxLayout(videoContainer);
                videoLayout->setContentsMargins(0, 0, 0, 0);
                videoLayout->setSpacing(6);
                QLineEdit *edit = new QLineEdit(&dialog);
                edit->setPlaceholderText("选择视频或图片文件");
                QPushButton *btnBrowse = new QPushButton("浏览...", &dialog);
                btnBrowse->setCursor(Qt::PointingHandCursor);
                btnBrowse->setFixedWidth(80);
                btnBrowse->setStyleSheet(
                    "QPushButton { background-color: #0984e3; color: white; border: none;"
                    "  border-radius: 4px; padding: 6px 12px; font-size: 12px; font-weight: 600; }"
                    "QPushButton:hover { background-color: #0770c4; }");
                videoLayout->addWidget(edit, 1);
                videoLayout->addWidget(btnBrowse);
                paramEditors["video"] = edit;
                formLayout->addRow("video:", videoContainer);

                connect(btnBrowse, &QPushButton::clicked, &dialog, [edit]() {
                    QString filePath = QFileDialog::getOpenFileName(
                        nullptr, "选择视频或图片文件", QString(),
                        "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm);;"
                        "图片文件 (*.png *.jpg *.jpeg *.gif *.bmp *.webp);;"
                        "所有文件 (*.*)");
                    if (!filePath.isEmpty()) {
                        edit->setText(filePath);
                    }
                });
            } else {
                QLineEdit *edit = new QLineEdit(&dialog);
                edit->setPlaceholderText(QString("Enter %1").arg(field));
                paramEditors[field] = edit;
                formLayout->addRow(field + ":", edit);
            }
        }
        dlgLayout->addLayout(formLayout);

        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        btnBox->button(QDialogButtonBox::Ok)->setText("全部运行");
        dlgLayout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(btnBox, &QDialogButtonBox::accepted, &dialog, [&dialog, &paramEditors]() {
            for (auto it = paramEditors.begin(); it != paramEditors.end(); ++it) {
                if (it.value()->text().trimmed().isEmpty()) {
                    QMessageBox::warning(&dialog, "运行模块",
                                         QString("%1 为必填项！").arg(it.key()));
                    it.value()->setFocus();
                    return;
                }
            }
            dialog.accept();
        });

        if (dialog.exec() != QDialog::Accepted) return;

        // 收集参数值
        QMap<QString, QString> inputValues;
        for (auto it = paramEditors.begin(); it != paramEditors.end(); ++it)
            inputValues[it.key()] = it.value()->text().trimmed();

        // 6. 构建任务队列（收集所有待执行模块，预校验，不立即启动）
        struct ModuleTask {
            QString displayName;
            QString runner;
            QStringList args;
            QStringList fields;
            QString workingDir;
        };
        QList<ModuleTask> taskQueue;
        int preCheckFail = 0;

        for (int i = 0; i < mountedModNames.size(); ++i) {
            QString modDisplayName = mountedModNames[i].toString().trimmed();
            if (!nameToInfo.contains(modDisplayName)) {
                qDebug() << "跳过未找到的模块:" << modDisplayName;
                preCheckFail++;
                continue;
            }

            QPair<QString, QString> pair = nameToInfo[modDisplayName];
            QString modPath = pair.first;

            // 6a. 读取模块配置
            QFile cfgFile(modPath + "/config.json");
            if (!cfgFile.open(QIODevice::ReadOnly | QIODevice::Text)) { preCheckFail++; continue; }
            QJsonObject modJson = QJsonDocument::fromJson(cfgFile.readAll()).object();
            cfgFile.close();

            QStringList taskFields;
            for (const QJsonValue &value : modJson["format"].toArray()) {
                const QString field = value.toString().trimmed();
                if (!field.isEmpty() && !taskFields.contains(field)) taskFields.append(field);
            }
            if (taskFields.isEmpty()) taskFields = {"video", "title", "desc"};

            // 6b. 查找可执行脚本（优先 script/entry 字段，回退到任意 .py）
            QStringList candidates;
            QString script = modJson["script"].toString().trimmed();
            if (script.isEmpty()) script = modJson["entry"].toString().trimmed();
            if (!script.isEmpty()) candidates << script;

            QDir modDir(modPath);
            for (const QFileInfo &fi : modDir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name))
                candidates << fi.fileName();

            QString scriptPath;
            for (const QString &c : candidates) {
                QFileInfo sf(c);
                QString cp = sf.isAbsolute() ? c : modDir.absoluteFilePath(c);
                if (QFile::exists(cp)) { scriptPath = cp; break; }
            }
            if (scriptPath.isEmpty()) {
                qDebug() << modDisplayName << "无可用脚本";
                preCheckFail++; continue;
            }

            // 6c. 确定 runner（优先从 config.json 读取）
            QString runner = modJson["runner"].toString().trimmed();
            if (runner.isEmpty()) runner = modJson["runtime"].toString().trimmed();
            if (runner.isEmpty()) runner = "python";

            // 6d. 组装命令行参数
            QStringList args;
            args << scriptPath << "--host" << hostPort;
            for (const QString &field : taskFields)
                args << QString("--%1").arg(field) << inputValues[field];

            ModuleTask task;
            task.displayName = modDisplayName;
            task.runner = runner;
            task.args = args;
            task.fields = taskFields;
            task.workingDir = modPath;
            taskQueue.append(task);
        }

        if (taskQueue.isEmpty()) {
            QMessageBox::information(this, "运行模块",
                                     QString("环境【%1】无可用模块！\n成功: 0 | 失败: %2 | 总计: %3")
                                         .arg(envName).arg(preCheckFail).arg(mountedModNames.size()));
            return;
        }

        // 7. 创建终端风格输出窗口
        QDialog *batchDlg = new QDialog(this);
        batchDlg->setWindowTitle(QString("批量运行模块 - %1").arg(envName));
        batchDlg->setFixedSize(680, 520);
        // 生命周期由 deleteLater() 管理，不用 WA_DeleteOnClose

        QVBoxLayout *batchLay = new QVBoxLayout(batchDlg);
        batchLay->setSpacing(10);

        QLabel *lblBatchTitle = new QLabel(batchDlg);
        lblBatchTitle->setWordWrap(true);
        lblBatchTitle->setText(QString("<b>环境: %1</b>&nbsp;&nbsp;|&nbsp;&nbsp;HOST: %2&nbsp;&nbsp;|&nbsp;&nbsp;模块总数: %3")
                                   .arg(envName, hostPort).arg(taskQueue.size()));
        batchLay->addWidget(lblBatchTitle);

        QProgressBar *batchBar = new QProgressBar(batchDlg);
        batchBar->setRange(0, taskQueue.size());
        batchBar->setValue(0);
        batchBar->setTextVisible(true);
        batchBar->setFixedHeight(20);
        batchLay->addWidget(batchBar);

        QTextEdit *batchLogEdit = new QTextEdit(batchDlg);
        batchLogEdit->setReadOnly(true);
        batchLogEdit->setStyleSheet(
            "QTextEdit {"
            "  background-color: #1e1e1e; color: #d4d4d4;"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px; border: 1px solid #333;"
            "  border-radius: 4px; padding: 8px;"
            "}");
        batchLay->addWidget(batchLogEdit);

        QHBoxLayout *batchBtnLay = new QHBoxLayout();
        batchBtnLay->addStretch();
        QPushButton *batchBtnClose = new QPushButton("关闭", batchDlg);
        batchBtnClose->setEnabled(false);
        batchBtnClose->setStyleSheet(
            "QPushButton { background-color: #1e90ff; color: white; border: none;"
            "  border-radius: 4px; padding: 8px 28px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background-color: #3742fa; }"
            "QPushButton:disabled { background-color: #a0a0a0; }");
        batchBtnLay->addWidget(batchBtnClose);
        batchLay->addLayout(batchBtnLay);

        auto batchAppend = [batchLogEdit](const QString &text) {
            batchLogEdit->append(text);
            QScrollBar *sb = batchLogEdit->verticalScrollBar();
            if (sb) sb->setValue(sb->maximum());
        };

        batchAppend(QString("══════════════════════════════════════"));
        batchAppend(QString("  环境: %1 | HOST: %2").arg(envName, hostPort));
        batchAppend(QString("  待执行模块: %1 个").arg(taskQueue.size()));
        batchAppend(QString("══════════════════════════════════════\n"));

        // 8. 异步串行执行 —— 用 QProcess::finished 信号驱动任务链，不阻塞 UI
        auto state = QSharedPointer<QPair<int, int>>::create(0, preCheckFail);
        auto queue = QSharedPointer<QList<ModuleTask>>::create(taskQueue);
        auto runNext = QSharedPointer<std::function<void()>>::create();

        *runNext = [this, envName, hostPort, inputValues,
                    mountedModNames, state, queue, runNext,
                    batchDlg, batchBar, batchAppend, batchBtnClose, lblBatchTitle,
                    batchLogEdit]() mutable
        {
            if (queue->isEmpty()) {
                // 全部完成
                batchBar->setValue(batchBar->maximum());
                int total = mountedModNames.size();
                int success = state->first;
                int fail = state->second;
                batchAppend("");
                batchAppend(QString("══════════════════════════════════════"));
                batchAppend(QString("  全部完成！成功: %1 | 失败: %2 | 总计: %3").arg(success).arg(fail).arg(total));
                batchAppend(QString("══════════════════════════════════════"));
                if (fail > 0) {
                    batchBar->setFormat(QString("完成 — 成功: %1 / 失败: %2").arg(success).arg(fail));
                    lblBatchTitle->setText(lblBatchTitle->text() + QString(" — %1成/%2败").arg(success).arg(fail));
                } else {
                    batchBar->setFormat("全部成功");
                    lblBatchTitle->setText(lblBatchTitle->text() + " — 全部成功 ?");
                }
                batchBtnClose->setEnabled(true);
                QTimer::singleShot(0, this, [runNext]() { *runNext = {}; });
                return;
            }

            ModuleTask task = queue->takeFirst();
            int currentIdx = batchBar->maximum() - queue->size() - 1;
            batchBar->setValue(currentIdx);
            batchBar->setFormat(QString("正在执行 [%1/%2]: %3")
                                    .arg(currentIdx + 1).arg(batchBar->maximum()).arg(task.displayName));

            batchAppend(QString("── [%1/%2] %3 ──")
                            .arg(currentIdx + 1).arg(batchBar->maximum()).arg(task.displayName));
            batchAppend(QString("  $ %1 %2").arg(task.runner, task.args.join(" ")));

            QProcess *proc = new QProcess(batchDlg);
            proc->setWorkingDirectory(task.workingDir);
            proc->setProcessChannelMode(QProcess::MergedChannels);

            QProcessEnvironment procEnv = QProcessEnvironment::systemEnvironment();
            procEnv.insert("HOST", hostPort);
            for (const QString &field : task.fields)
                procEnv.insert(field, inputValues[field]);
            proc->setProcessEnvironment(procEnv);

            connect(proc, &QProcess::readyReadStandardOutput, batchDlg, [this, proc, task, batchAppend]() {
                QString output = QString::fromLocal8Bit(proc->readAllStandardOutput());
                if (!output.trimmed().isEmpty()) {
                    batchAppend(output.trimmed());
                    writeLog(QString("模块输出[%1]").arg(task.displayName), output.trimmed());
                }
            });

            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), batchDlg,
                    [this, proc, task, state, runNext, batchAppend](int exitCode, QProcess::ExitStatus exitStatus) {
                        // 读取剩余输出写入日志
                        QString remaining = QString::fromLocal8Bit(proc->readAll());
                        if (!remaining.trimmed().isEmpty()) {
                            batchAppend(remaining.trimmed());
                            writeLog(QString("模块输出[%1]").arg(task.displayName), remaining.trimmed());
                        }

                        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                            batchAppend(QString("  ? %1 完成 (exitCode: 0)\n").arg(task.displayName));
                            writeLog("模块完成", QString("批量 — %1, exitCode: 0").arg(task.displayName));
                            state->first++;
                        } else {
                            batchAppend(QString("  ? %1 失败 (exitCode: %2, exitStatus: %3)\n")
                                            .arg(task.displayName).arg(exitCode).arg(exitStatus));
                            writeLog("模块失败", QString("批量 — %1, exitCode: %2, exitStatus: %3")
                                                         .arg(task.displayName).arg(exitCode).arg(exitStatus));
                            state->second++;
                        }
                        proc->deleteLater();
                        // 延迟 300ms 让 UI 有机会刷新，然后启动下一个任务
                        QTimer::singleShot(300, this, [runNext]() { (*runNext)(); });
                    });

            proc->start(task.runner, task.args);
            if (!proc->waitForStarted(3000)) {
                QString errOutput = QString::fromLocal8Bit(proc->readAll());
                batchAppend(QString("  ? %1 启动超时: %2").arg(task.displayName, proc->errorString()));
                writeLog("模块失败", QString("批量 — %1, 启动超时, 错误: %2")
                                             .arg(task.displayName).arg(proc->errorString()));
                if (!errOutput.trimmed().isEmpty()) {
                    batchAppend(QString("  %1").arg(errOutput.trimmed()));
                    writeLog(QString("模块启动失败详情[%1]").arg(task.displayName), errOutput.trimmed());
                }
                state->second++;
                proc->deleteLater();
                QTimer::singleShot(300, this, [runNext]() { (*runNext)(); });
            } else {
                writeLog("模块运行", QString("批量 — %1, runner: %2, 参数: %3")
                                             .arg(task.displayName, task.runner, task.args.join(" ")));
            }
        };

        // 启动第一个任务
        batchAppend("$ 开始执行...\n");
        QTimer::singleShot(100, this, [runNext]() { (*runNext)(); });

        // 禁止执行期间关闭对话框（X 按钮 + Alt+F4 均拦截）
        batchDlg->setWindowFlags(batchDlg->windowFlags() & ~Qt::WindowCloseButtonHint);
        // 关闭按钮在全部完成后才启用，由 finished 回调设置 btnClose->setEnabled(true)
        connect(batchBtnClose, &QPushButton::clicked, batchDlg, &QDialog::accept);
        batchDlg->exec();
        // exec() 返回后安全删除（用户点了关闭按钮）
        batchDlg->deleteLater();
    });




    cardMainLayout->addWidget(btnRunModule);
    cardMainLayout->addStretch();
    // tagWidget->deleteLater();




    auto onAttachTriggered = [this]() {
        // 利用 qobject_cast 和 sender() 精确抓取当前点击的究竟是哪个环境里的按钮
        QPushButton *clickedBtn = qobject_cast<QPushButton*>(sender());
        if (!clickedBtn) return;

        // 从按钮的父级（cardFrame）动态捞出当前环境专属的路径
        QWidget *pFrame = clickedBtn->parentWidget();
        if (!pFrame) return;

        QString currentJsonPath = pFrame->property("config_path").toString();

        // 重新构建该卡片内部局部的 lambda 动态渲染功能
        QLayout *pLayout = pFrame->layout();
        if (!pLayout) return;

        // 读取本地系统中 modules/ 文件夹下导入的所有真实可用模块
        QDir modulesDir(QCoreApplication::applicationDirPath() + "/modules");
        QStringList availableModules;
        for (const QString &subDir : modulesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile modFile(modulesDir.absoluteFilePath(subDir + "/config.json"));
            if (modFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QJsonObject mJson = QJsonDocument::fromJson(modFile.readAll()).object();
                QString mName = mJson["name"].toString();
                if (!mName.isEmpty()) availableModules << mName;
                modFile.close();
            }
        }

        qDebug() << "\n===== [挂载按钮点击] =====";
        qDebug() << "按钮文本:" << clickedBtn->text();
        qDebug() << "parent widget objectName:" << pFrame->objectName();
        qDebug() << "currentJsonPath:" << currentJsonPath;
        qDebug() << "文件存在?" << QFile::exists(currentJsonPath);
        // 列出真实的 profiles 文件夹名做对比
        qDebug() << "profiles/ 下真实子文件夹:"
                 << QDir(dataDir() + "/profiles")
                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot);



        if (availableModules.isEmpty()) {
            QMessageBox::information(pFrame, "提示", "当前系统模块管理中未导入任何模块，请先去导入模块！");
            return;
        }

        // 弹窗供用户选择需要挂载的真实模块名
        QDialog dialog(this);
        dialog.setWindowTitle("选择要挂载的模块");
        dialog.setFixedWidth(300);
        QVBoxLayout *dlgLayout = new QVBoxLayout(&dialog);

        QComboBox *modCombo = new QComboBox(&dialog);
        modCombo->addItems(availableModules);
        dlgLayout->addWidget(new QLabel("请选择模块："));
        dlgLayout->addWidget(modCombo);

        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        dlgLayout->addWidget(btnBox);
        connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted) {
            QString selectedModule = modCombo->currentText();

            // 1. 读取当前环境的真实 config.json
            QFile file(currentJsonPath);
            QJsonObject envJson;
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                envJson = QJsonDocument::fromJson(file.readAll()).object();
                file.close();
            }

            QJsonArray modulesArr = envJson["modules"].toArray();

            // 防止重复挂载同一个模块
            for(int i=0; i<modulesArr.size(); ++i) {
                if(modulesArr[i].toString() == selectedModule) {
                    QMessageBox::warning(pFrame, "提示", "该模块已挂载，请勿重复添加！");
                    return;
                }
            }

            // 2. 将选择的新模块名追加写入
            modulesArr.append(selectedModule);
            envJson["modules"] = modulesArr;

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(QJsonDocument(envJson).toJson(QJsonDocument::Indented));
                file.close();

                // 3. ? 延迟刷新卡片 UI（等当前信号处理完毕后再销毁旧卡片，避免重复）
                QTimer::singleShot(0, this, &MainWindow::refreshEnvCards);
            }
        }
    };

    // 正确地连接信号槽，不赋予额外的生命周期依赖
    connect(btnAttach, &QPushButton::clicked, this, onAttachTriggered);
    connect(btnAddMod, &QPushButton::clicked, this, onAttachTriggered);

    // 追加进主布局容器
    if (m_envLayout) {
        m_envLayout->addWidget(cardFrame);
    }
}




//分组管理
void MainWindow::on_pushButton_3_clicked()
{
    writeLog("点击", "导航 → 自动化管理");
    ui->stackedWidget->setCurrentWidget(ui->page_5);
}





// 创建浏览器
void MainWindow::on_pushButton_7_clicked()
{
    refreshSavedProxyCombo();
    writeLog("点击", "导航 → 新建浏览器");
    ui->stackedWidget->setCurrentWidget(ui->page_2);
}

// 环境管理
void MainWindow::on_pushButton_6_clicked()
{
    writeLog("点击", "导航 → 环境管理");
    ui->stackedWidget->setCurrentWidget(ui->page);
    queueEnvironmentIpChecks();
}

// 代理管理
void MainWindow::on_pushButton_4_clicked()
{
    refreshProxyManagerCards();
    writeLog("点击", "导航 → 代理管理");
    ui->stackedWidget->setCurrentWidget(ui->page_3);
}

// API
void MainWindow::on_pushButton_2_clicked()
{
    writeLog("点击", "导航 → API调用");
    ui->stackedWidget->setCurrentWidget(ui->page_4);
}



void MainWindow::on_pushButton_clicked()
{
    writeLog("点击", "导航 → 模块管理");
    ui->stackedWidget->setCurrentWidget(ui->page_6);
}



//创建 浏览器 确定


QJsonArray MainWindow::loadProxyProfiles() const
{
    const QString path = dataDir() + "/proxies.json";
    const bool fileExists = QFileInfo::exists(path);
    QJsonArray profiles;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        profiles = document.isArray() ? document.array() : document.object()["proxies"].toArray();
    }

    // One-time migration of proxy entries saved by older versions.
    if (!fileExists) {
        const QStringList legacy = QSettings("ProfilePower", "ProfilePower").value("proxies/saved").toStringList();
        int index = 1;
        for (const QString &value : legacy) {
            const QStringList parts = value.split(QChar(0x1f));
            if (parts.size() < 5) continue;
            QJsonObject profile;
            profile["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            profile["name"] = QString::fromUtf8("旧代理 %1").arg(index++);
            profile["mode"] = "custom";
            profile["protocol"] = parts[0];
            profile["host"] = parts[1];
            profile["port"] = parts[2];
            profile["username"] = parts[3];
            profile["password"] = parts[4];
            profile["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            profiles.append(profile);
        }
        saveProxyProfiles(profiles);
    }
    return profiles;
}

bool MainWindow::saveProxyProfiles(const QJsonArray &profiles) const
{
    QDir().mkpath(dataDir());
    QJsonObject root;
    root["schema_version"] = 1;
    root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["proxies"] = profiles;
    QSaveFile file(dataDir() + "/proxies.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) return false;
    return file.commit();
}

void MainWindow::refreshSavedProxyCombo(const QString &selectedId)
{
    if (!comboBoxSavedProxy) return;
    QString targetId = selectedId;
    if (targetId.isEmpty()) targetId = comboBoxSavedProxy->currentData().toMap().value("id").toString();

    comboBoxSavedProxy->blockSignals(true);
    comboBoxSavedProxy->clear();
    const QJsonArray profiles = loadProxyProfiles();
    for (const QJsonValue &value : profiles) {
        const QJsonObject profile = value.toObject();
        const QString mode = profile["mode"].toString();
        QString modeName = QString::fromUtf8("自定义");
        if (mode == "dynamic") modeName = QString::fromUtf8("动态");
        else if (mode == "integrated") modeName = QString::fromUtf8("集成");
        else if (mode == "wireguard") modeName = "WireGuard";
        comboBoxSavedProxy->addItem(QString("%1  ·  %2").arg(profile["name"].toString(), modeName), profile.toVariantMap());
    }
    if (comboBoxSavedProxy->count() == 0) {
        comboBoxSavedProxy->addItem(QString::fromUtf8("暂无代理，请先到代理管理添加"));
        comboBoxSavedProxy->setEnabled(false);
    } else {
        comboBoxSavedProxy->setEnabled(true);
        for (int i = 0; i < comboBoxSavedProxy->count(); ++i) {
            if (comboBoxSavedProxy->itemData(i).toMap().value("id").toString() == targetId) {
                comboBoxSavedProxy->setCurrentIndex(i);
                break;
            }
        }
    }
    comboBoxSavedProxy->blockSignals(false);
    if (proxyInputTabs && ui->tabWidget->currentIndex() == 1) updateProxyInputsFromCurrentMode();
}

void MainWindow::fetchDynamicProxy(const QString &apiUrl, QLabel *statusLabel)
{
    const quint64 requestSerial = ++m_dynamicProxyRequestSerial;
    QLabel *target = statusLabel ? statusLabel : labelDynamicProxyStatus;
    const QUrl url = QUrl::fromUserInput(apiUrl.trimmed());
    if (!url.isValid() || url.host().isEmpty()) {
        if (target) target->setText(QString::fromUtf8("API 地址无效"));
        return;
    }
    if (target) target->setText(QString::fromUtf8("正在获取代理..."));
    auto manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(QNetworkRequest(url));
    QPointer<QLabel> guardedStatus(target);
    QTimer::singleShot(15000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager, guardedStatus, requestSerial]() {
        QJsonObject parsed;
        const bool ok = reply->error() == QNetworkReply::NoError && parseProxyPayload(reply->readAll(), &parsed);
        if (requestSerial != m_dynamicProxyRequestSerial) {
            reply->deleteLater();
            manager->deleteLater();
            return;
        }
        if (!ok) {
            if (guardedStatus) guardedStatus->setText(QString::fromUtf8("获取失败，请检查 API"));
        } else {
            m_resolvedDynamicProxy = parsed;
            const QString protocol = parsed["protocol"].toString("HTTP").toUpper();
            const int typeIndex = ui->comboBox_11->findText(protocol, Qt::MatchFixedString);
            if (typeIndex >= 0) ui->comboBox_11->setCurrentIndex(typeIndex);
            lineEditProxyHost->setText(parsed["host"].toString());
            lineEditProxyPort->setText(parsed["port"].toString());
            lineEditProxyUser->setText(parsed["username"].toString());
            lineEditProxyPass->setText(parsed["password"].toString());
            const QString endpoint = QString("%1://%2:%3").arg(protocol, parsed["host"].toString(), parsed["port"].toString());
            if (guardedStatus) guardedStatus->setText(QString::fromUtf8("已获取：") + endpoint);
            if (labelDynamicProxyStatus && labelDynamicProxyStatus != guardedStatus) labelDynamicProxyStatus->setText(endpoint);
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::testProxyProfile(const QJsonObject &profile, QLabel *statusLabel)
{
    QPointer<QLabel> guardedStatus(statusLabel);
    if (guardedStatus) guardedStatus->setText(QString::fromUtf8("测试中..."));
    if (profile["protocol"].toString() == "WIREGUARD") {
        QString error;
        auto manager = createProxyNetworkManager(profile, &error);
        if (!manager) {
            if (guardedStatus) guardedStatus->setText(error);
            return;
        }
        QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("https://api.ipify.org?format=json")));
        QTimer::singleShot(20000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [reply, manager, guardedStatus]() {
            const QString ip = QJsonDocument::fromJson(reply->readAll()).object()["ip"].toString();
            if (guardedStatus) guardedStatus->setText(reply->error() == QNetworkReply::NoError && !ip.isEmpty()
                ? QString::fromUtf8("出口 IP：") + ip : QString::fromUtf8("节点连接失败，请检查节点或网络。"));
            reply->deleteLater(); manager->deleteLater();
        });
        return;
    }
    const QString mode = profile["mode"].toString();
    if (mode == "dynamic") {
        const QUrl url = QUrl::fromUserInput(profile["api_url"].toString());
        if (!url.isValid() || url.host().isEmpty()) {
            if (guardedStatus) guardedStatus->setText(QString::fromUtf8("API 地址无效"));
            return;
        }
        auto manager = new QNetworkAccessManager(this);
        QNetworkReply *reply = manager->get(QNetworkRequest(url));
        QTimer::singleShot(15000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager, guardedStatus]() {
            QJsonObject parsed;
            const bool parsedOk = reply->error() == QNetworkReply::NoError
                                  && parseProxyPayload(reply->readAll(), &parsed);
            reply->deleteLater();
            manager->deleteLater();
            if (!parsedOk) {
                if (guardedStatus) guardedStatus->setText(QString::fromUtf8("API 请求失败"));
                return;
            }

            QString error;
            auto probeManager = createProxyNetworkManager(parsed, &error);
            if (!probeManager) { if (guardedStatus) guardedStatus->setText(error); return; }
            QNetworkReply *probe = probeManager->get(QNetworkRequest(QUrl("https://api.ipify.org?format=json")));
            QTimer::singleShot(15000, probe, [probe]() { if (probe->isRunning()) probe->abort(); });
            connect(probe, &QNetworkReply::finished, this, [probe, probeManager, guardedStatus]() {
                if (probe->error() == QNetworkReply::NoError) {
                    const QString ip = QJsonDocument::fromJson(probe->readAll()).object()["ip"].toString();
                    if (guardedStatus) guardedStatus->setText(ip.isEmpty() ? QString::fromUtf8("代理连接成功")
                                                                          : QString::fromUtf8("出口 IP：") + ip);
                } else if (guardedStatus) {
                    guardedStatus->setText(QString::fromUtf8("代理连接失败"));
                }
                probe->deleteLater();
                probeManager->deleteLater();
            });
        });
        return;
    }

    const QString host = profile["host"].toString().trimmed();
    const int port = profile["port"].toString().toInt();
    if (host.isEmpty() || port <= 0) {
        if (guardedStatus) guardedStatus->setText(QString::fromUtf8("主机或端口无效"));
        return;
    }
    QString error;
    auto manager = createProxyNetworkManager(profile, &error);
    if (!manager) { if (guardedStatus) guardedStatus->setText(error); return; }
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("https://api.ipify.org?format=json")));
    QTimer::singleShot(15000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [reply, manager, guardedStatus]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QString ip = QJsonDocument::fromJson(reply->readAll()).object()["ip"].toString();
            if (guardedStatus) guardedStatus->setText(ip.isEmpty() ? QString::fromUtf8("连接成功") : QString::fromUtf8("出口 IP：") + ip);
        } else if (guardedStatus) {
            guardedStatus->setText(QString::fromUtf8("连接失败"));
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::updateProxyInputsFromCurrentMode()
{
    if (!proxyInputTabs || !comboBoxSavedProxy) return;
    const bool imported = ui->tabWidget->currentIndex() == 1 && proxyInputTabs->currentIndex() == 1;
    const QJsonObject profile = QJsonObject::fromVariantMap(comboBoxSavedProxy->currentData().toMap());
    if (imported && profile["mode"].toString() == "dynamic") {
        const QString apiUrl = profile["api_url"].toString();
        if (m_requestedDynamicUrl != apiUrl) {
            m_resolvedDynamicProxy = {};
            m_requestedDynamicUrl = apiUrl;
            fetchDynamicProxy(apiUrl, labelSavedProxyStatus);
        }
        return;
    }
    if (!m_requestedDynamicUrl.isEmpty()) {
        ++m_dynamicProxyRequestSerial;
        m_requestedDynamicUrl.clear();
        m_resolvedDynamicProxy = {};
    }
    if (labelSavedProxyStatus) labelSavedProxyStatus->setText(profile.isEmpty()
        ? QString::fromUtf8("尚未导入代理，请选择配置文件或文件夹。") : ProxyConfig::summary(profile));
}

bool MainWindow::validateCreateBrowserForm(QString *errorMessage) const
{
    auto fail = [errorMessage](const QString &message) { if (errorMessage) *errorMessage = message; return false; };
    const QString name = ui->lineEdit_2->text().trimmed();
    if (name.isEmpty()) return fail("Environment name is required.");
    if (name.size() > 80) return fail("Environment name must not exceed 80 characters.");
    const QString browserPath = ui->comboBox_2->currentData().toString();
    if (browserPath.isEmpty() || !QFileInfo::exists(browserPath)) return fail("Select an installed browser executable.");
    if (ui->comboBox_9->currentText() == "Custom" && ui->lineEdit_3->text().trimmed().isEmpty()) return fail("Custom User-Agent cannot be empty.");
    QDir profiles(dataDir() + "/profiles");
    for (const QString &folder : profiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile file(profiles.absoluteFilePath(folder + "/config.json"));
        if (file.open(QIODevice::ReadOnly)) {
            const QString existing = QJsonDocument::fromJson(file.readAll()).object()["env_name"].toString();
            if (existing.compare(name, Qt::CaseInsensitive) == 0) return fail("An environment with the same name already exists.");
        }
    }
    QString proxyError;
    if (!ProxyConfig::validate(currentProxyProfile(), &proxyError)) return fail(proxyError);
    for (const QString &line : ui->textEdit->toPlainText().split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const QUrl url = QUrl::fromUserInput(line.trimmed());
        if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) return fail(QString("Invalid startup URL: %1").arg(line));
    }
    if (ui->tabWidget_8->currentIndex() == 2 && !QRegularExpression("^[0-9]{3,5}[xX,][0-9]{3,5}$").match(lineEditResolution->text()).hasMatch()) return fail("Custom resolution must use WIDTHxHEIGHT.");
    if (ui->tabWidget_5->currentIndex() == 2 && (lineEditLatitude->text().isEmpty() || lineEditLongitude->text().isEmpty())) return fail("Custom geolocation requires latitude and longitude.");
    return true;
}

void MainWindow::resetCreateBrowserForm()
{
    ui->lineEdit_2->clear(); ui->comboBox_9->setCurrentText("Auto generated");
    lineEditGroup->clear(); lineEditTag->clear(); ui->lineEdit_4->clear(); ui->lineEdit_5->clear(); ui->textEdit->clear();
    ui->tabWidget->setCurrentIndex(0); proxyInputTabs->setCurrentIndex(0); ui->comboBox_11->setCurrentText("HTTP");
    lineEditProxyHost->clear(); lineEditProxyPort->clear(); lineEditProxyUser->clear(); lineEditProxyPass->clear();
    lineEditDynamicProxyUrl->clear(); labelDynamicProxyStatus->clear();
    ui->tabWidget_3->setCurrentIndex(2); ui->tabWidget_4->setCurrentIndex(1); ui->tabWidget_5->setCurrentIndex(1);
    ui->tabWidget_6->setCurrentIndex(0); ui->tabWidget_7->setCurrentIndex(0); ui->tabWidget_8->setCurrentIndex(0);
    ui->tabWidget_9->setCurrentIndex(0); ui->tabWidget_2->setCurrentIndex(0);
    lineEditAcceptLanguage->setText("zh-CN,zh;q=0.9,en;q=0.8"); comboBoxUiLanguage->setCurrentText("zh-CN");
    lineEditResolution->setText("1920x1080"); lineEditLatitude->clear(); lineEditLongitude->clear(); lineEditCustomFonts->clear();
    for (QCheckBox *box : {ui->checkBox, ui->checkBox_2, ui->checkBox_3, ui->checkBox_4, ui->checkBox_5, ui->checkBox_6}) box->setChecked(false);
}

void MainWindow::on_pushButton_8_clicked()
{
    updateProxyInputsFromCurrentMode();
    QString validationError;
    if (!validateCreateBrowserForm(&validationError)) {
        QMessageBox::warning(this, "Create browser", validationError);
        return;
    }

    const QString envName = ui->lineEdit_2->text().trimmed();
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    uuid.remove('-');
    const QString envId = "env_" + uuid;
    const QString profilesRoot = dataDir() + "/profiles";
    const QString currentEnvPath = QDir(profilesRoot).absoluteFilePath(envId);
    const QString chromeDataPath = QDir(currentEnvPath).absoluteFilePath("chrome-data");
    if (!QDir().mkpath(chromeDataPath)) {
        QMessageBox::critical(this, "Create browser", "Cannot create the profile directory.");
        return;
    }

    QJsonObject config;
    config["schema_version"] = 2;
    config["env_id"] = envId;
    config["env_name"] = envName;
    config["created_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    config["chrome_user_data_dir"] = "./chrome-data";
    config["runtime_extension"] = "./runtime-extension";
    config["browser_name"] = ui->comboBox_2->currentText();
    config["browser_path"] = QDir::cleanPath(ui->comboBox_2->currentData().toString());
    config["browser_channel"] = ui->comboBox_3->currentText();
    QString browserEngine = ui->comboBox_4->currentText();
    if (browserEngine == "Auto") {
        const QString browserIdentity = ui->comboBox_2->currentText() + " " + ui->comboBox_2->currentData().toString();
        browserEngine = browserIdentity.contains("edge", Qt::CaseInsensitive) ? "Edge" : "Chrome";
    }
    const QRegularExpressionMatch browserVersionMatch = QRegularExpression("(\\d+)").match(ui->comboBox_2->currentText());
    const QString browserMajorVersion = browserVersionMatch.hasMatch() ? browserVersionMatch.captured(1) : "146";
    config["browser_engine_mode"] = ui->comboBox_4->currentText();
    config["browser_engine"] = browserEngine;
    config["browser_major_version"] = browserMajorVersion;
    config["os_family"] = ui->comboBox_6->currentText();
    config["os_version"] = ui->comboBox_8->currentText();
    config["cpu_arch"] = ui->comboBox_7->currentText();
    config["device_type"] = ui->comboBox_5->currentText();
    config["ua_profile"] = ui->comboBox_9->currentText();
    config["user_agent"] = ui->lineEdit_3->text().trimmed();
    config["group"] = lineEditGroup->text().trimmed().isEmpty() ? "Default" : lineEditGroup->text().trimmed();
    config["tag"] = lineEditTag->text().trimmed();
    config["cookie"] = ui->lineEdit_4->text().trimmed();
    config["remark"] = ui->lineEdit_5->text().trimmed();
    config["account_platform"] = ui->comboBox_10->currentText();
    config["modules"] = QJsonArray();

    const QJsonObject selectedProxy = currentProxyProfile();
    config["proxy_mode_index"] = ui->tabWidget->currentIndex();
    config["proxy_mode"] = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
    config["proxy_input_mode"] = proxyInputTabs->currentIndex() == 0 ? "manual" : "import";
    config["proxy_type"] = selectedProxy["protocol"];
    config["proxy_ip_channel"] = ui->comboBox_12->currentText();
    config["proxy_host"] = selectedProxy["host"].toString();
    config["proxy_port"] = selectedProxy["port"].toString();
    config["proxy_username"] = selectedProxy["username"].toString();
    config["proxy_password"] = selectedProxy["password"].toString();
    config["dynamic_proxy_url"] = selectedProxy["api_url"].toString();
    config["integrated_proxy_provider"] = selectedProxy["provider"].toString();
    config["proxy_profile_id"] = selectedProxy["id"].toString();
    config["proxy_profile_mode"] = selectedProxy["mode"].toString();
    // Snapshot the node so moving the source file or editing the library cannot break this environment.
    if (selectedProxy["protocol"].toString() == "WIREGUARD") config["wireguard_proxy"] = selectedProxy;

    QStringList startupUrls;
    for (const QString &line : ui->textEdit->toPlainText().split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        startupUrls.append(QUrl::fromUserInput(line.trimmed()).toString());
    }
    config["startup_urls"] = startupUrls.join('\n');
    config["startup_url_list"] = QJsonArray::fromStringList(startupUrls);

    const int webRtcIndex = ui->tabWidget_3->currentIndex();
    config["webrtc_mode"] = ui->tabWidget_3->tabText(webRtcIndex);
    config["webrtc_mode_index"] = webRtcIndex;
    config["disable_webrtc"] = webRtcIndex == 3;

    const int timezoneIndex = ui->tabWidget_4->currentIndex();
    config["timezone_mode"] = ui->tabWidget_4->tabText(timezoneIndex);
    config["timezone_mode_index"] = timezoneIndex;
    config["timezone"] = timezoneIndex == 1 ? QString() : ui->comboBox_14->currentText().trimmed();

    const int geoIndex = ui->tabWidget_5->currentIndex();
    config["geolocation_mode"] = ui->tabWidget_5->tabText(geoIndex);
    config["geolocation_mode_index"] = geoIndex;
    config["latitude"] = lineEditLatitude->text().isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(lineEditLatitude->text().toDouble());
    config["longitude"] = lineEditLongitude->text().isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(lineEditLongitude->text().toDouble());

    const int languageIndex = ui->tabWidget_6->currentIndex();
    QString acceptLanguage = lineEditAcceptLanguage->text().trimmed();
    if (acceptLanguage.isEmpty()) acceptLanguage = QLocale::system().bcp47Name() + ",en;q=0.8";
    config["language_mode"] = ui->tabWidget_6->tabText(languageIndex);
    config["accept_language"] = acceptLanguage;
    QJsonArray languages;
    for (const QString &entry : acceptLanguage.split(',', Qt::SkipEmptyParts)) languages.append(entry.section(';', 0, 0).trimmed());
    config["languages"] = languages;

    const int uiLanguageIndex = ui->tabWidget_7->currentIndex();
    const QString primaryLanguage = languages.isEmpty() ? "en-US" : languages.at(0).toString("en-US");
    QString uiLanguage = uiLanguageIndex == 2 ? comboBoxUiLanguage->currentText().trimmed()
                                              : (uiLanguageIndex == 1 ? QLocale::system().bcp47Name() : primaryLanguage);
    config["ui_language_mode"] = ui->tabWidget_7->tabText(uiLanguageIndex);
    config["ui_language"] = uiLanguage;

    const QStringList resolutions{"1920x1080", "1680x1050", "1536x864", "1440x900", "1366x768", "1280x720"};
    const int resolutionIndex = ui->tabWidget_8->currentIndex();
    QString resolution = resolutionIndex == 0 ? resolutions.at(QRandomGenerator::global()->bounded(resolutions.size()))
                         : resolutionIndex == 1 ? comboBoxResolutionPreset->currentText() : lineEditResolution->text();
    resolution.replace(QRegularExpression("[xX]"), ",");
    config["resolution_mode"] = ui->tabWidget_8->tabText(resolutionIndex);
    config["resolution"] = resolution;

    config["font_mode"] = ui->tabWidget_9->tabText(ui->tabWidget_9->currentIndex());
    QJsonArray customFonts;
    for (const QString &font : lineEditCustomFonts->text().split(',', Qt::SkipEmptyParts)) customFonts.append(font.trimmed());
    config["custom_fonts"] = customFonts;
    config["webgl_mode"] = ui->tabWidget_2->tabText(ui->tabWidget_2->currentIndex());
    config["webgl_vendor"] = ui->comboBox_13->currentText();
    config["webgl_renderer"] = ui->lineEdit_6->text().trimmed();
    const QString osFamily = ui->comboBox_6->currentText();
    const QString osVersion = ui->comboBox_8->currentText();
    const QString cpuArch = ui->comboBox_7->currentText();
    QString navigatorPlatform = "Win32";
    if (osFamily == "macOS") navigatorPlatform = "MacIntel";
    else if (osFamily == "Linux") navigatorPlatform = cpuArch == "arm64" ? "Linux aarch64" : "Linux x86_64";
    QString uaPlatformVersion;
    if (osVersion == "Windows 10") uaPlatformVersion = "10.0.0";
    else if (osVersion == "Windows 11") uaPlatformVersion = "13.0.0";
    else if (osVersion.startsWith("macOS ")) uaPlatformVersion = osVersion.section(' ', 1, 1) + ".0.0";
    config["navigator_platform"] = navigatorPlatform;
    config["ua_ch_platform"] = osFamily;
    config["ua_ch_platform_version"] = uaPlatformVersion;
    config["ua_ch_architecture"] = cpuArch == "arm64" ? "arm" : "x86";
    config["ua_ch_bitness"] = "64";
    config["ua_ch_mobile"] = false;
    config["hardware_concurrency"] = ui->comboBox_5->currentText() == "Laptop" ? 4 : 8;
    config["device_memory"] = ui->comboBox_5->currentText() == "Laptop" ? 4 : 8;

    QJsonObject noise;
    noise["canvas_noise"] = ui->checkBox->isChecked();
    noise["audio_noise"] = ui->checkBox_2->isChecked();
    noise["client_rects_noise"] = ui->checkBox_3->isChecked();
    noise["webgl_noise"] = ui->checkBox_4->isChecked();
    noise["font_noise"] = ui->checkBox_5->isChecked();
    noise["media_devices_noise"] = ui->checkBox_6->isChecked();
    config["hardware_noise"] = noise;

    QString extensionPath;
    QString runtimeError;
    if (!ProfileRuntime::writeExtension(currentEnvPath, config, &extensionPath, &runtimeError)) {
        QDir(currentEnvPath).removeRecursively();
        QMessageBox::critical(this, "Create browser", QString("Cannot create runtime extension:\n%1").arg(runtimeError));
        return;
    }

    const QString jsonPath = QDir(currentEnvPath).absoluteFilePath("config.json");
    QSaveFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(config).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        QDir(currentEnvPath).removeRecursively();
        QMessageBox::critical(this, "Create browser", QString("Cannot save config.json:\n%1").arg(file.errorString()));
        return;
    }

    writeLog("FingerprintDebug", QString("name=%1 engine=%2 browser=%3 os=%4/%5 arch=%6 device=%7 platform=%8 UA=%9")
        .arg(envName, browserEngine, config["browser_name"].toString(), osFamily, osVersion,
             cpuArch, config["device_type"].toString(), navigatorPlatform, config["user_agent"].toString()));
    writeLog("CreateEnvironment", QString("name=%1 id=%2 browser=%3 proxyMode=%4").arg(envName, envId, config["browser_name"].toString(), config["proxy_mode"].toString()));
    QMessageBox::information(this, "Create browser", QString("Environment '%1' was created.").arg(envName));
    resetCreateBrowserForm();
    loadEnvironmentsFromJson();
    QTimer::singleShot(0, this, &MainWindow::refreshEnvCards);
    ui->stackedWidget->setCurrentWidget(ui->page);
}



// 将 config.json 中存储的 chrome_user_data_dir 解析为绝对路径
// 新版本存储相对路径 "./chrome-data"，旧版本存储绝对路径，此函数兼容两种格式
QString MainWindow::resolveChromeDataPath(const QString &configJsonPath, const QString &storedPath) const
{
    // 如果已经是绝对路径（旧版本兼容）且目录存在，直接返回
    if (QDir::isAbsolutePath(storedPath)) {
        QDir dir(storedPath);
        if (dir.exists()) {
            return storedPath;
        }
        // 绝对路径指向的目录不存在（可能是移动了文件夹），
        // 回退使用相对路径 "./chrome-data" 重新解析
    }

    // 确定要解析的相对路径：新版直接存 "./chrome-data"，旧版绝对路径失效时也回退到此
    QString relativePath = QDir::isAbsolutePath(storedPath) ? "./chrome-data" : storedPath;

    // 相对路径：以 config.json 所在目录为基准解析
    QFileInfo jsonFileInfo(configJsonPath);
    QDir jsonDir = jsonFileInfo.absoluteDir();
    QString resolved = jsonDir.absoluteFilePath(relativePath);
    return QDir::cleanPath(resolved);
}

MainWindow::BrowserLaunchConfig MainWindow::buildLaunchConfig(const QJsonObject &configJson) const
{
    BrowserLaunchConfig config;
    config.envId = configJson["env_id"].toString();
    config.proxyFingerprint = environmentProxyFingerprint(configJson);
    config.envName = configJson["env_name"].toString();
    config.chromeUserDataDir = configJson["chrome_user_data_dir"].toString();
    config.browserPath = configJson["browser_path"].toString();
    config.userAgent = configJson["user_agent"].toString();
    config.acceptLanguage = configJson["accept_language"].toString("zh-CN,zh;q=0.9,en;q=0.8");
    config.uiLanguage = configJson["ui_language"].toString("zh-CN");
    config.timezone = configJson["timezone"].toString();
    config.resolution = configJson["resolution"].toString();
    config.extensionPath = configJson["runtime_extension"].toString();
    config.webrtcMode = configJson["webrtc_mode"].toString();

    const QString proxyType = configJson["proxy_type"].toString().remove('"').trimmed();
    const QString proxyHost = configJson["proxy_host"].toString().remove('"').trimmed();
    const int proxyPort = configJson["proxy_port"].toString().remove('"').trimmed().toInt();
    if (proxyType.compare("WIREGUARD", Qt::CaseInsensitive) == 0) {
        config.proxyType = "wireguard";
        config.wireguardProfile = configJson["wireguard_proxy"].toObject();
    } else if (!proxyType.isEmpty() && proxyType.compare("No Proxy", Qt::CaseInsensitive) != 0
        && proxyType != QString::fromUtf8("无代理") && !proxyHost.isEmpty() && proxyPort > 0) {
        config.proxyType = proxyType.toLower();
        config.proxyServer = QString("%1://%2:%3").arg(proxyType.toLower(),
            proxyHost.contains(':') ? '[' + proxyHost + ']' : proxyHost).arg(proxyPort);
        config.proxyUsername = configJson["proxy_username"].toString();
        config.proxyPassword = configJson["proxy_password"].toString();
        config.proxyHost = proxyHost;
        config.proxyPort = proxyPort;
    }

    config.disableWebRtc = configJson["disable_webrtc"].toBool(
        config.webrtcMode.contains(QString::fromUtf8("禁用"), Qt::CaseInsensitive)
        || config.webrtcMode.contains("Disable", Qt::CaseInsensitive));
    const QString geoMode = configJson["geolocation_mode"].toString();
    config.disableGeolocation = geoMode.contains(QString::fromUtf8("禁用"), Qt::CaseInsensitive)
                                || geoMode.contains("Disable", Qt::CaseInsensitive);

    const QString webglMode = configJson["webgl_mode"].toString();
    config.disableWebGl = webglMode.contains(QString::fromUtf8("禁用"), Qt::CaseInsensitive)
                          || webglMode.contains("Disable", Qt::CaseInsensitive);

    const QStringList lines = configJson["startup_urls"].toString()
                                  .split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (QString url : lines) {
        url = url.trimmed();
        if (!url.isEmpty()) {
            config.startupUrls.append(url);
        }
    }

    return config;
}



bool MainWindow::openBrowserEnvironment(const BrowserLaunchConfig &config, int *debugPortOut)
{
    if (debugPortOut) *debugPortOut = 0;
    QTcpServer debugPortPicker;
    if (!debugPortPicker.listen(QHostAddress::LocalHost, 0)) {
        QMessageBox::warning(this, "Browser launch", "Could not allocate a local debugging port.");
        return false;
    }
    int debugPort = debugPortPicker.serverPort();
    debugPortPicker.close();

    QString chromeExePath = config.browserPath;
    if (chromeExePath.isEmpty()) {
        chromeExePath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
    }

    if (!QFile::exists(chromeExePath)) {
        QMessageBox::warning(this, "提示", "未找到浏览器程序，请检查新建环境中保存的浏览器路径！");
        return false;
    }

    auto profileLock = std::make_unique<QLockFile>(
        QDir(config.chromeUserDataDir).absoluteFilePath(".profilepower.lock"));
    if (!profileLock->tryLock(0)) {
        QMessageBox::warning(this, "Browser launch", "This environment is already open in another application instance.");
        return false;
    }

    // ========== Proxy relay startup (auth support) ==========
    QString effectiveProxyServer = config.proxyServer;
    std::unique_ptr<WireGuardProxy> wireguard;
    if (config.proxyType == "wireguard") {
        wireguard = std::make_unique<WireGuardProxy>();
        QString proxyError;
        if (!wireguard->start(config.wireguardProfile, &proxyError)) {
            QMessageBox::warning(this, QString::fromUtf8("WireGuard 节点"), proxyError);
            return false;
        }
        effectiveProxyServer = wireguard->server();
    }
    QProcess *relayProcess = nullptr;
    writeLog("ProxyDebug", QString("credentials=[%1] host=[%2] port=[%3] server=[%4]")
        .arg(config.proxyUsername.isEmpty() && config.proxyPassword.isEmpty() ? "none" : "configured",
             config.proxyHost, QString::number(config.proxyPort), config.proxyServer));
    if ((!config.proxyUsername.isEmpty() || !config.proxyPassword.isEmpty())
        && !config.proxyHost.isEmpty() && config.proxyPort > 0) {

        // 1. Pick a free local port
        quint16 relayPort = 0;
        QTcpServer portPicker;
        if (portPicker.listen(QHostAddress::LocalHost, 0)) {
            relayPort = portPicker.serverPort();
            portPicker.close();
        }
        if (relayPort == 0) {
            QMessageBox::warning(this, "Proxy relay", "Could not allocate a local proxy relay port.");
            return false;
        }

        // 2. Find Python
        QString python = QStandardPaths::findExecutable("python");
        if (python.isEmpty()) {
            python = QStandardPaths::findExecutable("python3");
        }

        QString relayScript = QCoreApplication::applicationDirPath() + "/proxy_relay_v2.py";
        if (!QFile::exists(relayScript)) relayScript = QCoreApplication::applicationDirPath() + "/../../proxy_relay_v2.py";
        const bool useV2 = QFile::exists(relayScript);
        if (!useV2) {
            relayScript = QCoreApplication::applicationDirPath() + "/proxy_relay.py";
            if (!QFile::exists(relayScript)) relayScript = QCoreApplication::applicationDirPath() + "/../../proxy_relay.py";
        }

        if (!python.isEmpty() && QFile::exists(relayScript)) {
            relayProcess = new QProcess(this);
            relayProcess->setProcessChannelMode(QProcess::MergedChannels);
            QStringList relayArgs;
            relayArgs << relayScript
                      << QString::number(relayPort);
            if (useV2) {
                QProcessEnvironment relayEnvironment = QProcessEnvironment::systemEnvironment();
                relayEnvironment.insert("PROFILE_PROXY_TYPE", config.proxyType);
                relayEnvironment.insert("PROFILE_PROXY_HOST", config.proxyHost);
                relayEnvironment.insert("PROFILE_PROXY_PORT", QString::number(config.proxyPort));
                relayEnvironment.insert("PROFILE_PROXY_USER", config.proxyUsername);
                relayEnvironment.insert("PROFILE_PROXY_PASS", config.proxyPassword);
                relayProcess->setProcessEnvironment(relayEnvironment);
            } else {
                relayArgs << config.proxyHost << QString::number(config.proxyPort)
                          << config.proxyUsername << config.proxyPassword;
            }

            relayProcess->start(python, relayArgs);
            if (relayProcess->waitForStarted(3000)) {
                QByteArray relayOutput = relayProcess->readAll();
                QElapsedTimer relayTimer;
                relayTimer.start();
                while (!relayOutput.contains("READY ") && relayTimer.elapsed() < 3000
                       && relayProcess->state() != QProcess::NotRunning) {
                    relayProcess->waitForReadyRead(100);
                    relayOutput += relayProcess->readAll();
                }
                if (!relayOutput.contains("READY ")) {
                    writeLog("ProxyRelay", QString("Relay did not become ready: %1")
                             .arg(QString::fromUtf8(relayOutput).trimmed()));
                    relayProcess->terminate();
                    if (!relayProcess->waitForFinished(1000)) relayProcess->kill();
                    relayProcess->deleteLater();
                    relayProcess = nullptr;
                } else {
                effectiveProxyServer = QString("http://127.0.0.1:%1").arg(relayPort);
                writeLog("ProxyRelay", QString("Relay started on 127.0.0.1:%1 -> %2:%3")
                         .arg(relayPort).arg(config.proxyHost).arg(config.proxyPort));
                }
            } else {
                writeLog("ProxyRelay", QString("Relay failed to start: %1").arg(relayProcess->errorString()));
                relayProcess->deleteLater();
                relayProcess = nullptr;
            }
        } else {
            writeLog("ProxyRelay", "Cannot start relay: Python or proxy_relay.py not found");
        }
    }
    if ((!config.proxyUsername.isEmpty() || !config.proxyPassword.isEmpty()) && !relayProcess) {
        QMessageBox::warning(this, "Proxy relay", "The authenticated proxy relay could not be started.");
        return false;
    }

    QStringList arguments;
    arguments << QString("--user-data-dir=%1").arg(QDir::toNativeSeparators(config.chromeUserDataDir));
    arguments << QString("--remote-debugging-port=%1").arg(debugPort);

    if (!config.userAgent.isEmpty()) {
        arguments << QString("--user-agent=%1").arg(config.userAgent);
    }
    if (!effectiveProxyServer.isEmpty()) {
        arguments << QString("--proxy-server=%1").arg(effectiveProxyServer);
    } else {
        arguments << "--no-proxy-server";
    }
    if (!config.acceptLanguage.isEmpty()) {
        arguments << QString("--accept-lang=%1").arg(config.acceptLanguage);
    }
    if (!config.uiLanguage.isEmpty()) {
        arguments << QString("--lang=%1").arg(config.uiLanguage);
    }
    if (!config.resolution.isEmpty()) {
        arguments << QString("--window-size=%1").arg(config.resolution);
    }
    if (!config.extensionPath.isEmpty() && QDir(config.extensionPath).exists()) {
        arguments << QString("--load-extension=%1").arg(QDir::toNativeSeparators(config.extensionPath));
    }
    const bool proxyWebRtc = config.webrtcMode.contains(QString::fromUtf8("转发"))
                             || config.webrtcMode.contains(QString::fromUtf8("替换"))
                             || config.webrtcMode.contains("UDP", Qt::CaseInsensitive);
    if (config.disableWebRtc || proxyWebRtc || config.proxyType == "wireguard") {
        arguments << "--force-webrtc-ip-handling-policy=disable_non_proxied_udp";
    }
    if (config.disableWebRtc) {
        arguments << "--disable-features=WebRtcHideLocalIpsWithMdns";
    }
    if (config.disableGeolocation) {
        arguments << "--deny-permission-prompts";
    }
    if (config.disableWebGl) {
        arguments << "--disable-webgl";
    }
    arguments << "--no-first-run";
    arguments << "--no-default-browser-check";
    arguments << "--skip-first-run-ui";

    QProcess *cdpProcess = nullptr;
    QString cdpScript;
    const QString applicationDir = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {
             applicationDir + "/browser_cdp_runtime.py",
             applicationDir + "/../../../browser_cdp_runtime.py",
             applicationDir + "/../../browser_cdp_runtime.py"
         }) {
        if (QFile::exists(candidate)) {
            cdpScript = QDir::cleanPath(candidate);
            break;
        }
    }
    const QString runtimeConfigPath = QDir::cleanPath(QDir(config.chromeUserDataDir).absoluteFilePath("../config.json"));
    QString cdpPython = QStandardPaths::findExecutable("python");
    if (cdpPython.isEmpty()) cdpPython = QStandardPaths::findExecutable("python3");
    if (!cdpPython.isEmpty() && QFile::exists(cdpScript) && QFile::exists(runtimeConfigPath)) {
        cdpProcess = new QProcess(this);
        cdpProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(cdpProcess, &QProcess::readyReadStandardOutput, this, [this, cdpProcess]() {
            const QString output = QString::fromUtf8(cdpProcess->readAllStandardOutput()).trimmed();
            if (!output.isEmpty()) writeLog("FingerprintCDP", output);
        });
        writeLog("FingerprintCDP", QString("Helper prepared: config=%1").arg(runtimeConfigPath));
    } else {
        writeLog("FingerprintCDP", QString("Helper unavailable: python=%1 script=%2 config=%3")
                 .arg(cdpPython, cdpScript, runtimeConfigPath));
    }
    if (cdpProcess) arguments << "about:blank";
    else arguments << config.startupUrls;

    const QString devToolsPortPath = QDir(config.chromeUserDataDir).absoluteFilePath("DevToolsActivePort");
    QFile::remove(devToolsPortPath);
    QProcess *chromeProcess = new QProcess(this);
    if (!config.timezone.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("TZ", config.timezone);
        chromeProcess->setProcessEnvironment(env);
    }

    chromeProcess->setProperty("my_port", debugPort);
    if (wireguard) {
        WireGuardProxy *activeWireguard = wireguard.release();
        activeWireguard->setParent(chromeProcess);
        // A dead node must never leave a browser silently using a direct connection.
        connect(activeWireguard->process(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), chromeProcess,
                [chromeProcess]() { if (chromeProcess->state() != QProcess::NotRunning) chromeProcess->terminate(); });
    }
    chromeProcess->setProperty("env_name", config.envName);


    chromeProcess->start(chromeExePath, arguments);

    if (!chromeProcess->waitForStarted(3000)) {
        writeLog("Browser launch", QString("Failed to start %1: %2").arg(config.envName, chromeProcess->errorString()));
        if (relayProcess) {
            relayProcess->terminate();
            if (!relayProcess->waitForFinished(1000)) relayProcess->kill();
            relayProcess->deleteLater();
        }
        if (cdpProcess) cdpProcess->deleteLater();
        chromeProcess->deleteLater();
        QMessageBox::warning(this, "Browser launch", "The browser process could not be started.");
        return false;
    }

    QElapsedTimer portTimer;
    portTimer.start();
    while (debugPort == 0 && portTimer.elapsed() < 5000) {
        QFile portFile(devToolsPortPath);
        if (portFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            bool ok = false;
            const int candidate = QString::fromLatin1(portFile.readLine()).trimmed().toInt(&ok);
            if (ok && candidate > 0 && candidate <= 65535) debugPort = candidate;
        }
        if (debugPort == 0) QThread::msleep(50);
    }

    if (debugPort == 0) {
        chromeProcess->terminate();
        if (!chromeProcess->waitForFinished(1500)) chromeProcess->kill();
        if (relayProcess) {
            relayProcess->terminate();
            if (!relayProcess->waitForFinished(1000)) relayProcess->kill();
            relayProcess->deleteLater();
        }
        if (cdpProcess) cdpProcess->deleteLater();
        chromeProcess->deleteLater();
        QMessageBox::warning(this, "Browser launch", "Chrome did not publish its debugging port.");
        return false;
    }

    if (chromeProcess->state() == QProcess::NotRunning) {
        if (relayProcess) {
            relayProcess->terminate();
            if (!relayProcess->waitForFinished(1000)) relayProcess->kill();
            relayProcess->deleteLater();
        }
        if (cdpProcess) cdpProcess->deleteLater();
        chromeProcess->deleteLater();
        QMessageBox::warning(this, "Browser launch", "The browser exited before startup completed.");
        return false;
    }

    if (cdpProcess) {
        cdpProcess->start(cdpPython, {cdpScript, QString::number(debugPort), runtimeConfigPath});
        if (!cdpProcess->waitForStarted(1500)) {
            writeLog("FingerprintCDP", QString("Cannot start helper: %1").arg(cdpProcess->errorString()));
            cdpProcess->deleteLater();
            cdpProcess = nullptr;
        } else {
            writeLog("FingerprintCDP", QString("Helper started: port=%1 config=%2").arg(debugPort).arg(runtimeConfigPath));
        }
    }

#ifdef Q_OS_WIN
    if (!effectiveProxyServer.isEmpty()) {
        chromeProcess->setProperty("env_id", config.envId);
        chromeProcess->setProperty("proxyFingerprint", config.proxyFingerprint);
        if (auto idleCheck = m_environmentIpChecks.take(config.envId)) idleCheck->deleteLater();
        m_environmentIpQueue.removeIf([&config](const QJsonObject &job) { return job["env_id"].toString() == config.envId; });
        QTimer::singleShot(0, this, [this]() { startNextEnvironmentIpCheck(); });
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            if (ui->tableWidget->item(row, 3) && ui->tableWidget->item(row, 3)->text() == config.envId
                && ui->tableWidget->item(row, 5)) ui->tableWidget->item(row, 5)->setText(QString::fromUtf8("检测中…"));
        }
        const QUrl endpoint(effectiveProxyServer);
        const QJsonObject proxy{{"protocol", endpoint.scheme().toUpper()}, {"host", endpoint.host()},
            {"port", QString::number(endpoint.port())}};
        QString badgeError;
        auto network = createProxyNetworkManager(proxy, &badgeError);
        // Query through the effective relay/tunnel, never through a separate node
        // connection or the machine's default route. A failed check stays unverified.
        auto badge = new BrowserProxyBadge(chromeProcess->processId(), network, chromeProcess);
        badge->setEnvironmentName(config.envName);
        connect(badge, &BrowserProxyBadge::exitIpChanged, this,
                [this, id = config.envId, fingerprint = config.proxyFingerprint](const QString &ip) {
            updateEnvironmentExitIp(id, fingerprint, ip);
        });
        if (!network) updateEnvironmentExitIp(config.envId, config.proxyFingerprint, {});
    }
#endif

    chromeProcess->setProperty("my_port", debugPort);
    QLockFile *runningProfileLock = profileLock.release();
    connect(chromeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, chromeProcess, relayProcess, cdpProcess, runningProfileLock]() {
        const int deadPort = chromeProcess->property("my_port").toInt();
        const QString environmentName = chromeProcess->property("env_name").toString();
        for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
            QTableWidgetItem *portItem = ui->tableWidget_2->item(i, 5);
            if (portItem && portItem->text().toInt() == deadPort) {
                ui->tableWidget_2->removeRow(i);
                break;
            }
        }
        for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
            if (QTableWidgetItem *indexItem = ui->tableWidget_2->item(i, 0)) {
                indexItem->setText(QString::number(i + 1));
            }
        }
        auto stopAuxiliary = [](QProcess *process) {
            if (!process) return;
            if (process->state() != QProcess::NotRunning) process->terminate();
            QTimer::singleShot(1500, process, [process]() {
                if (process->state() != QProcess::NotRunning) process->kill();
                process->deleteLater();
            });
        };
        stopAuxiliary(relayProcess);
        stopAuxiliary(cdpProcess);
        writeLog("Browser closed", QString("name=%1 port=%2").arg(environmentName).arg(deadPort));
        delete runningProfileLock;
        chromeProcess->deleteLater();
        updateEnvCardStatus(environmentName, false);
    });

    if (debugPortOut) *debugPortOut = debugPort;
    writeLog("Browser launch", QString("Started name=%1 port=%2").arg(config.envName).arg(debugPort));
    updateEnvCardStatus(config.envName, true);
    return true;

    return false;
}



void MainWindow::updateEnvCardStatus(const QString &envName, bool running)
{
    // 遍历 m_envLayout 中的所有卡片，找到 env_name 匹配的那个
    if (!m_envLayout) return;

    for (int i = 0; i < m_envLayout->count(); ++i) {
        QLayoutItem *item = m_envLayout->itemAt(i);
        if (!item || !item->widget()) continue;

        QString cardEnvName = item->widget()->property("env_name").toString();
        if (cardEnvName != envName) continue;

        // 找到了匹配的卡片，更新它的状态标签
        // 卡片结构：cardFrame(QVBoxLayout) → headerLayout(QHBoxLayout) → titleLeftLayout → lblStatus
        QFrame *cardFrame = qobject_cast<QFrame*>(item->widget());
        if (!cardFrame) return;

        QVBoxLayout *cardLayout = qobject_cast<QVBoxLayout*>(cardFrame->layout());
        if (!cardLayout || cardLayout->count() == 0) return;

        // headerLayout 是第一个子布局
        QHBoxLayout *headerLayout = qobject_cast<QHBoxLayout*>(cardLayout->itemAt(0)->layout());
        if (!headerLayout || headerLayout->count() == 0) return;

        // titleLeftLayout 是 headerLayout 的第一个子项
        QHBoxLayout *titleLeftLayout = qobject_cast<QHBoxLayout*>(headerLayout->itemAt(0)->layout());
        if (!titleLeftLayout || titleLeftLayout->count() < 2) return;

        QLabel *lblStatus = qobject_cast<QLabel*>(titleLeftLayout->itemAt(1)->widget());
        if (!lblStatus) return;

        if (running) {
            lblStatus->setText("● 运行中");
            lblStatus->setStyleSheet("color: #2ed573; font-size: 13px; font-weight: bold; margin-left: 10px;");
        } else {
            lblStatus->setText("● 未运行");
            lblStatus->setStyleSheet("color: #a4b0be; font-size: 13px; font-weight: bold; margin-left: 10px;");
        }
        writeLog("状态变更", QString("环境: %1 → %2").arg(envName, running ? "运行中" : "未运行"));
        break;
    }
}

// 【核心功能】：从本地 profiles 文件夹遍历读取 JSON 配置并刷到 UI 上
void MainWindow::loadEnvironmentsFromJson()
{
    // 定位到主程序运行根目录下的 profiles 文件夹
    QDir profilesDir(dataDir() + "/profiles");
    if (!profilesDir.exists()) {
        // 如果目录不存在，说明还没有创建任何指纹，创建空文件夹并直接返回
        profilesDir.mkpath(".");
        ui->tableWidget->setRowCount(0);
        rightTable->setRowCount(0);
        return;
    }

    // 扫描获取 profiles 目录下的所有子环境文件夹
    QStringList subDirs = profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // 设置双表的总行数
    int rowCount = subDirs.size();
    ui->tableWidget->setRowCount(rowCount);
    rightTable->setRowCount(rowCount);

    int validRow = 0;
    for (int i = 0; i < rowCount; ++i) {
        QString jsonPath = profilesDir.absoluteFilePath(subDirs[i] + "/config.json");
        QFile file(jsonPath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue; // 如果某个文件夹里的 config.json 坏了或不存在，跳过
        }

        QByteArray jsonData = file.readAll();
        file.close();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) continue;

        QJsonObject configJson = jsonDoc.object();

        // --- 1. 从 JSON 解析真实的指纹配置参数 ---
        QString envId      = configJson["env_id"].toString();
        if (envId.isEmpty()) envId = subDirs[i];
        configJson["env_id"] = envId;
        QString envName    = configJson["env_name"].toString();
        QString remark     = configJson["remark"].toString();
        QString proxyType  = configJson["proxy_type"].toString();
        QString userAgent  = configJson["user_agent"].toString();
        // 核心路径：读取此前保存的用户专属指纹浏览器数据路径
        // 兼容旧版绝对路径 + 新版相对路径，统一解析为绝对路径
        QString chromePath = resolveChromeDataPath(jsonPath, configJson["chrome_user_data_dir"].toString());
        // 同步更新 configJson，确保后续存入 UserRole+3 的缓存 JSON 也是绝对路径
        configJson["chrome_user_data_dir"] = chromePath;

        // --- 2. 填充左侧主表的数据 ---

        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setCheckState(Qt::Unchecked);
        ui->tableWidget->setItem(validRow, 0, checkItem);

        // 后面各列：根据你实际的列位置填入
        ui->tableWidget->setItem(validRow, 1, new QTableWidgetItem(QString::number(validRow + 1))); // 序号

        // 环境名称（??核心技巧：将 chromePath 隐藏绑定在环境名单元格的 UserRole 里面）
        QTableWidgetItem *nameItem = new QTableWidgetItem(envName);
        nameItem->setData(Qt::UserRole, chromePath);      // 藏入数据目录路径
        nameItem->setData(Qt::UserRole + 1, userAgent);   // 藏入对应的自定义 UA
        nameItem->setData(Qt::UserRole + 2, jsonPath);
        nameItem->setData(Qt::UserRole + 3, QString::fromUtf8(QJsonDocument(configJson).toJson(QJsonDocument::Compact)));
        ui->tableWidget->setItem(validRow, 2, nameItem);

        ui->tableWidget->setItem(validRow, 3, new QTableWidgetItem(envId));     // 账号信息/环境ID
        ui->tableWidget->setItem(validRow, 4, new QTableWidgetItem(proxyType)); // 代理类型
        ui->tableWidget->setItem(validRow, 5, new QTableWidgetItem(environmentIpText(configJson)));
        ui->tableWidget->setItem(validRow, 6, new QTableWidgetItem(remark));    // 备注
        for (int column = 1; column < ui->tableWidget->columnCount(); ++column) {
            auto item = ui->tableWidget->item(validRow, column);
            if (!item) continue;
            item->setToolTip(item->text());
            if (column == 1) item->setTextAlignment(Qt::AlignCenter);
        }

        // --- 3. 填充右侧固定表的数据（动态生成带信号槽绑定的“打开”组合键） ---
        // --- 3. 填充右侧固定表的数据（动态生成带信号槽绑定的“打开”组合键） ---
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(8, 8, 8, 8);
        actionLayout->setSpacing(8);

        // 打开按钮
        QPushButton *btnOpen = new QPushButton("打开");
        btnOpen->setCursor(Qt::PointingHandCursor);
        btnOpen->setObjectName("environmentOpenButton");
        btnOpen->setProperty("variant", "primary");
        btnOpen->setFixedSize(60, 32);

        // ??【核心灵魂】：把当前的行号通过 setProperty 传给按钮，方便点击时知道点击的是哪一行
        btnOpen->setProperty("row_index", validRow);

        // ??【核心灵魂】：绑定信号槽到通用的处理槽函数上
        connect(btnOpen, &QPushButton::clicked, this, &MainWindow::on_btnOpen_clicked);

        actionLayout->addWidget(btnOpen);

        QPushButton *btnSetting = new QPushButton(QString::fromUtf8("更多"));
        btnSetting->setObjectName("environmentMoreButton");
        btnSetting->setProperty("variant", "secondary");
        btnSetting->setCursor(Qt::PointingHandCursor);
        btnSetting->setAccessibleName(QString::fromUtf8("环境操作菜单"));
        btnSetting->setToolTip(QString::fromUtf8("更多操作"));
        btnSetting->setFixedSize(58, 32);
        QMenu *settingMenu = new QMenu(btnSetting);
        settingMenu->setObjectName("environmentActionMenu");
        QAction *actEdit = settingMenu->addAction(QString::fromUtf8("修改环境"));
        settingMenu->addSeparator();
        QAction *actDelete = settingMenu->addAction(QString::fromUtf8("删除环境"));

        // ??【核心灵魂】：把当前行号标记到每个 Action 上，方便点击时知道点的是哪一行
        actEdit->setProperty("row_index", validRow);
        actDelete->setProperty("row_index", validRow);

        // ??【核心灵魂】：把这些动作连接到你的自定义槽函数
        connect(actEdit, &QAction::triggered, this, &MainWindow::on_btnEdit_triggered);
        connect(actDelete, &QAction::triggered, this, &MainWindow::on_btnDelete_triggered);

        // 绑定：把菜单塞进按钮里，变成真正的下拉框按钮
        btnSetting->setMenu(settingMenu);

        actionLayout->addWidget(btnSetting);


        rightTable->setCellWidget(validRow, 0, actionWidget);


        ui->tableWidget->setRowHeight(validRow, 52);
        rightTable->setRowHeight(validRow, 52);

        validRow++;
    }

    // 如果有因损坏跳过的文件，修正最终行数显示
    ui->tableWidget->setRowCount(validRow);
    rightTable->setRowCount(validRow);
    QTimer::singleShot(0, this, [this]() { queueEnvironmentIpChecks(); });
}


// 【核心功能】：当点击任何一行的“打开”按钮时触发
void MainWindow::launchBrowserEnvironmentFromJson(const QJsonObject &launchJson,
                                                   const QString &envId,
                                                   const QString &envName,
                                                   const QString &proxyType)
{
    QJsonObject config = launchJson;
    config["env_id"] = envId;
    BrowserLaunchConfig launchConfig = buildLaunchConfig(config);
    int debugPort = 0;
    if (!openBrowserEnvironment(launchConfig, &debugPort)) return;

    if (ui->tableWidget_2->columnCount() < 6) {
        ui->tableWidget_2->setColumnCount(6);
        ui->tableWidget_2->setHorizontalHeaderLabels(
            {"Index", "Group", "Name", "Proxy", "IP", "Port"});
    }

    const int runRow = ui->tableWidget_2->rowCount();
    ui->tableWidget_2->insertRow(runRow);
    QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(runRow + 1));
    QTableWidgetItem *groupItem = new QTableWidgetItem(launchJson["group"].toString("Default"));
    groupItem->setData(Qt::UserRole, envId);
    groupItem->setData(Qt::UserRole + 1, launchConfig.proxyFingerprint);
    QString ipText = environmentIpText(config);
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        if (ui->tableWidget->item(row, 3) && ui->tableWidget->item(row, 3)->text() == envId
            && ui->tableWidget->item(row, 5)) { ipText = ui->tableWidget->item(row, 5)->text(); break; }
    }
    QList<QTableWidgetItem *> items{
        indexItem,
        groupItem,
        new QTableWidgetItem(envName),
        new QTableWidgetItem(proxyType),
        new QTableWidgetItem(ipText),
        new QTableWidgetItem(QString::number(debugPort))
    };
    for (int column = 0; column < items.size(); ++column) {
        items[column]->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_2->setItem(runRow, column, items[column]);
    }
    ui->tableWidget_2->setRowHeight(runRow, 35);
    writeLog("Open environment", QString("name=%1 port=%2 data=%3")
             .arg(envName).arg(debugPort).arg(launchConfig.chromeUserDataDir));
}

void MainWindow::on_btnOpen_clicked()
{
    // 1. 获取发送当前信号的按钮指针
    QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) return;

    // 2. 提取出保存在按钮中的行号
    int row = clickedButton->property("row_index").toInt();

    // 3. 抓取原始数据
    QTableWidgetItem *nameItem  = ui->tableWidget->item(row, 2);
    QTableWidgetItem *idItem    = ui->tableWidget->item(row, 3);
    QTableWidgetItem *proxyItem = ui->tableWidget->item(row, 4);

    if (!nameItem) return;

    QString envName    = nameItem->text();
    QString envId      = idItem ? idItem->text() : "未分组";
    QString proxyType  = proxyItem ? proxyItem->text() : "无代理";

    QString chromeUserDataDir = nameItem->data(Qt::UserRole).toString();
    QString customUA          = nameItem->data(Qt::UserRole + 1).toString();
        QJsonObject launchJson;
    // Always read latest config from disk as primary source
    QString configPath = QFileInfo(chromeUserDataDir).dir().absoluteFilePath("config.json");
    QFile launchConfigFile(configPath);
    if (launchConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(launchConfigFile.readAll(), &parseError);
        launchConfigFile.close();
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            launchJson = doc.object();
        }
    }
    // Fallback: try cached config from table
    if (launchJson.isEmpty()) {
        const QString cachedJson = nameItem->data(Qt::UserRole + 3).toString();
        if (!cachedJson.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(cachedJson.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                launchJson = doc.object();
            }
        }
    }
    // Last resort: minimal defaults
    if (launchJson.isEmpty()) {
        launchJson["env_name"] = envName;
        launchJson["chrome_user_data_dir"] = chromeUserDataDir;
        launchJson["user_agent"] = customUA;
    }

    // Resolve relative chrome_user_data_dir from disk config
    {
        QString rawDataDir = launchJson["chrome_user_data_dir"].toString();
        QString configDir = QFileInfo(configPath).absolutePath();
        QString resolvedDir = QDir(configDir).absoluteFilePath(rawDataDir);
        launchJson["chrome_user_data_dir"] = QDir::cleanPath(resolvedDir);
        const QString rawExtensionDir = launchJson["runtime_extension"].toString("./runtime-extension");
        const QString resolvedExtensionDir = QDir::isAbsolutePath(rawExtensionDir)
                                                 ? rawExtensionDir
                                                 : QDir(configDir).absoluteFilePath(rawExtensionDir);
        launchJson["runtime_extension"] = QDir::cleanPath(resolvedExtensionDir);
    }

    // 4. 分配端口
    for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
        QTableWidgetItem *runningGroupItem = ui->tableWidget_2->item(i, 1);
        QTableWidgetItem *runningNameItem = ui->tableWidget_2->item(i, 2);
        const QString runningId = runningGroupItem ? runningGroupItem->data(Qt::UserRole).toString() : QString();
        if (runningId == envId || (runningNameItem && runningNameItem->text() == envName)) {
            QMessageBox::warning(this, "Open environment", QString("Environment '%1' is already running.").arg(envName));
            return;
        }
    }
    const bool usesDynamicProxy = launchJson["proxy_profile_mode"].toString() == "dynamic"
                                  || launchJson["proxy_mode_index"].toInt(-1) == 2;
    const QUrl dynamicUrl = QUrl::fromUserInput(launchJson["dynamic_proxy_url"].toString().trimmed());
    if (!usesDynamicProxy) {
        launchBrowserEnvironmentFromJson(launchJson, envId, envName, proxyType);
        return;
    }
    if (!dynamicUrl.isValid() || dynamicUrl.host().isEmpty()) {
        QMessageBox::warning(this, "Dynamic proxy", "The dynamic proxy API URL is invalid.");
        return;
    }

    QPointer<QPushButton> guardedButton(clickedButton);
    guardedButton->setEnabled(false);
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(QNetworkRequest(dynamicUrl));
    QTimer::singleShot(15000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, manager, guardedButton, launchJson, envId, envName, proxyType]() mutable {
        if (guardedButton) guardedButton->setEnabled(true);
        QJsonObject resolvedProxy;
        const bool ok = reply->error() == QNetworkReply::NoError
                        && parseProxyPayload(reply->readAll(), &resolvedProxy);
        reply->deleteLater();
        manager->deleteLater();
        if (!ok) {
            QMessageBox::warning(this, "Dynamic proxy", "Could not obtain a valid proxy endpoint from the API.");
            return;
        }
        launchJson["proxy_type"] = resolvedProxy["protocol"];
        launchJson["proxy_host"] = resolvedProxy["host"];
        launchJson["proxy_port"] = resolvedProxy["port"];
        launchJson["proxy_username"] = resolvedProxy["username"];
        launchJson["proxy_password"] = resolvedProxy["password"];
        launchBrowserEnvironmentFromJson(launchJson, envId, envName,
                                         resolvedProxy["protocol"].toString());
    });
    return;

}

// 【下拉菜单：修改环境点击响应】
void MainWindow::on_btnEdit_triggered()
{
    // 1. 获取触发当前动作的 QAction
    QAction *clickedAction = qobject_cast<QAction*>(sender());
    if (!clickedAction) return;

    // 2. 提取出保存在 Action 里的行号
    int row = clickedAction->property("row_index").toInt();

    // 3. 拿到行里的真实数据（比如环境名称）
    QTableWidgetItem *nameItem = ui->tableWidget->item(row, 2);
    if (!nameItem) return;
    QString envName = nameItem->text();

    // 提示测试（实际业务中可以在这里弹出修改窗口或跳转到修改页面）
    QMessageBox::information(this, "修改环境", QString("你点击了第 %1 行【%2】的修改动作").arg(row + 1).arg(envName));
    writeLog("修改环境", QString("行: %1, 名称: %2 (功能待完善)").arg(row + 1).arg(envName));
}

// 【下拉菜单：删除环境点击响应】
void MainWindow::on_btnDelete_triggered()
{
    QAction *clickedAction = qobject_cast<QAction*>(sender());
    if (!clickedAction) return;

    int row = clickedAction->property("row_index").toInt();

    QTableWidgetItem *nameItem = ui->tableWidget->item(row, 2);
    if (!nameItem) return;
    QString envName = nameItem->text();
    // 隐藏绑定的环境路径
    QString chromePath = nameItem->data(Qt::UserRole).toString();

    // 弹出确认删除框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除", QString("确定要删除环境【%1】吗？\n(注意：这将无法恢复！)").arg(envName),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        writeLog("删除环境", QString("名称: %1, 路径: %2").arg(envName, chromePath));

        for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
            QTableWidgetItem *runningName = ui->tableWidget_2->item(i, 2);
            if (runningName && runningName->text().compare(envName, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "无法删除", "请先关闭该环境的浏览器进程，再执行删除。");
                return;
            }
        }

        QDir envDir(chromePath);
        if (!envDir.cdUp()) {
            QMessageBox::critical(this, "删除失败", "无法解析环境目录。");
            return;
        }

        const QString environmentRoot = QDir::fromNativeSeparators(QDir::cleanPath(envDir.absolutePath()));
        const QString profilesRoot = QDir::fromNativeSeparators(
            QDir::cleanPath(QDir(dataDir()).absoluteFilePath("profiles")));
        if (!environmentRoot.startsWith(profilesRoot + "/", Qt::CaseInsensitive)) {
            writeLog("删除环境", QString("拒绝删除越界路径: %1").arg(environmentRoot));
            QMessageBox::critical(this, "删除失败", "环境目录不在 ProfilePower 数据目录内，已拒绝删除。");
            return;
        }

        qDebug() << "删除环境 - chromePath:" << chromePath;
        qDebug() << "删除环境 - envDir 绝对路径:" << envDir.absolutePath();
        qDebug() << "删除环境 - envDir 是否存在:" << envDir.exists();

        if (envDir.exists() && !envDir.removeRecursively()) {
            writeLog("删除环境", QString("失败 — 目录可能被占用: %1").arg(environmentRoot));
            QMessageBox::critical(this, "删除失败", "无法删除环境目录。请确认浏览器已关闭且目录未被占用。");
            return;
        }

        loadEnvironmentsFromJson();
        QTimer::singleShot(0, this, &MainWindow::refreshEnvCards);

        QMessageBox::information(this, "成功", "环境已成功删除！");
    }
}



// 【核心功能】：从本地 modules 文件夹遍历读取 JSON 配置并刷到 tableWidget_3 上
void MainWindow::loadModulesFromJson()
{
    QDir modulesDir(QCoreApplication::applicationDirPath() + "/modules");

    // 初始化设置 tableWidget_3 的列宽及表头（4列：ID、名字、运行环境、操作）
    ui->tableWidget_3->setColumnCount(4);
    ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "模块 ID" << "模块名称" << "运行环境" << "操作");
    ui->tableWidget_3->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableWidget_3->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget_3->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->tableWidget_3->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->tableWidget_3->setColumnWidth(3, 150); // 操作列固定 150px，放两个按钮绰绰有余
    ui->tableWidget_3->verticalHeader()->setVisible(false);

    if (!modulesDir.exists()) {
        modulesDir.mkpath(".");
        ui->tableWidget_3->setRowCount(0);
        return;
    }

    QStringList subDirs = modulesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    int validRow = 0;
    ui->tableWidget_3->setRowCount(subDirs.size());

    for (int i = 0; i < subDirs.size(); ++i) {
        // 记录当前子文件夹的绝对路径，方便后面删除时使用
        QString currentModFolderPath = modulesDir.absoluteFilePath(subDirs[i]);
        QString jsonPath = currentModFolderPath + "/config.json";
        QFile file(jsonPath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QByteArray jsonData = file.readAll();
        file.close();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) continue;

        QJsonObject moduleJson = jsonDoc.object();
        QString modId   = moduleJson["id"].toString();
        QString modName = moduleJson["name"].toString();
        QString modEnv  = moduleJson["env"].toString();

        // 1. 填充基础文本数据
        QTableWidgetItem *idItem = new QTableWidgetItem(modId);
        QTableWidgetItem *nameItem = new QTableWidgetItem(modName);
        QTableWidgetItem *envItem = new QTableWidgetItem(modEnv);

        idItem->setTextAlignment(Qt::AlignCenter);
        nameItem->setTextAlignment(Qt::AlignCenter);
        envItem->setTextAlignment(Qt::AlignCenter);

        ui->tableWidget_3->setItem(validRow, 0, idItem);
        ui->tableWidget_3->setItem(validRow, 1, nameItem);
        ui->tableWidget_3->setItem(validRow, 2, envItem);

        // 2. 动态创建右侧的操作按钮（删除、运行）
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(4, 2, 4, 2);
        actionLayout->setSpacing(6);

        // 按钮 A：删除按钮（自适应行高）
        QPushButton *btnDelete = new QPushButton("删除");
        btnDelete->setCursor(Qt::PointingHandCursor);
        btnDelete->setMinimumWidth(46);
        btnDelete->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        btnDelete->setStyleSheet(
            "QPushButton {"
            "  background-color: #ff4757;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 5px;"
            "  padding: 2px 10px;"
            "  font-size: 12px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "  background-color: #ff6b81;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #e8414e;"
            "}");
        btnDelete->setProperty("row_index", validRow);
        btnDelete->setProperty("module_id", modId);
        btnDelete->setProperty("module_path", currentModFolderPath);

        // 按钮 B：运行按钮（自适应行高）
        QPushButton *btnRun = new QPushButton("运行");
        btnRun->setCursor(Qt::PointingHandCursor);
        btnRun->setMinimumWidth(46);
        btnRun->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        btnRun->setStyleSheet(
            "QPushButton {"
            "  background-color: #0984e3;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 5px;"
            "  padding: 2px 10px;"
            "  font-size: 12px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "  background-color: #0770c4;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #065da6;"
            "}");
        btnRun->setProperty("row_index", validRow);
        btnRun->setProperty("module_id", modId);
        btnRun->setProperty("module_path", currentModFolderPath);

        actionLayout->addWidget(btnDelete); // 将删除按钮添加进布局
        actionLayout->addWidget(btnRun);
        actionWidget->setLayout(actionLayout);

        // 将组合按钮塞进第 4 列 (索引 3)
        ui->tableWidget_3->setCellWidget(validRow, 3, actionWidget);
        ui->tableWidget_3->setRowHeight(validRow, 52);

        // 3. 绑定按钮点击事件到槽函数
        //  绑定删除逻辑
        connect(btnDelete, &QPushButton::clicked, this, [this, btnDelete]() {
            QString modId = btnDelete->property("module_id").toString();
            QString modPath = btnDelete->property("module_path").toString();

            // 弹出确认删除框，防止误触
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "确认删除",
                                          QString("确定要物理删除模块【%1】吗？\n(注意：这将无法恢复！)").arg(modId),
                                          QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                QDir dir(modPath);
                if (dir.exists()) {
                    // 物理递归删除整个模块文件夹（包含里面的 config.json 和其它所有脚本文件）
                    if (dir.removeRecursively()) {
                        qDebug() << "成功删除本地模块文件夹：" << modPath;
                        writeLog("删除模块", QString("成功 — ID: %1, 路径: %2").arg(modId, modPath));
                        QMessageBox::information(this, "成功", "模块已成功删除！");
                    } else {
                        QMessageBox::critical(this, "错误", "无法删除模块文件夹，请检查文件是否被占用或权限不足！");
                    }
                }

                // 物理文件删完后，重新加载一次列表刷新 UI 界面
                loadModulesFromJson();
            }
        });



        // 绑定运行逻辑
        connect(btnRun, &QPushButton::clicked, this, [this, btnRun]() {
            int row = btnRun->property("row_index").toInt();
            QString modId = btnRun->property("module_id").toString();
            QString modPath = btnRun->property("module_path").toString();

            if (modPath.isEmpty()) {
                QMessageBox::warning(this, "Run Module", "Module folder was not found.");
                return;
            }

            QFile configFile(QDir(modPath).absoluteFilePath("config.json"));
            if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Run Module", "Cannot read module config.json.");
                return;
            }

            QJsonParseError parseError;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(configFile.readAll(), &parseError);
            configFile.close();
            if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
                QMessageBox::warning(this, "Run Module", QString("Invalid config.json: %1").arg(parseError.errorString()));
                return;
            }

            QJsonObject moduleJson = jsonDoc.object();
            QJsonArray formatArray = moduleJson["format"].toArray();

            QStringList fieldNames;
            for (const QJsonValue &value : formatArray) {
                QString fieldName = value.toString().trimmed();
                if (!fieldName.isEmpty() && !fieldNames.contains(fieldName)) {
                    fieldNames << fieldName;
                }
            }

            QStringList scriptCandidates;
            QString configuredScript = moduleJson["script"].toString().trimmed();
            if (configuredScript.isEmpty()) {
                configuredScript = moduleJson["entry"].toString().trimmed();
            }
            if (!configuredScript.isEmpty()) {
                scriptCandidates << configuredScript;
            }

            QDir moduleDir(modPath);
            QFileInfoList pyFiles = moduleDir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name);
            for (const QFileInfo &fileInfo : pyFiles) {
                scriptCandidates << fileInfo.fileName();
            }

            QString scriptPath;
            for (const QString &candidate : scriptCandidates) {
                QFileInfo scriptInfo(candidate);
                QString candidatePath = scriptInfo.isAbsolute() ? candidate : moduleDir.absoluteFilePath(candidate);
                if (QFile::exists(candidatePath)) {
                    scriptPath = candidatePath;
                    break;
                }
            }

            if (scriptPath.isEmpty()) {
                QMessageBox::warning(this, "Run Module", "No runnable script was found. Configure script/entry or add a .py file.");
                return;
            }

            QDialog dialog(this);
            dialog.setWindowTitle(QString("Run Module - %1").arg(modId));
            dialog.resize(420, 260);

            QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
            QLabel *hintLabel = new QLabel(QString("Module: %1 (row %2)").arg(modId).arg(row + 1), &dialog);
            dialogLayout->addWidget(hintLabel);

            QFormLayout *formLayout = new QFormLayout();
            QComboBox *portCombo = new QComboBox(&dialog);
            for (int i = 0; i < ui->tableWidget_2->rowCount(); ++i) {
                QTableWidgetItem *nameItem = ui->tableWidget_2->item(i, 2);
                QTableWidgetItem *portItem = ui->tableWidget_2->item(i, 5);
                if (!portItem || portItem->text().trimmed().isEmpty()) continue;

                QString port = portItem->text().trimmed();
                QString envName = nameItem ? nameItem->text().trimmed() : QString("Env%1").arg(i + 1);
                portCombo->addItem(QString("%1 (%2)").arg(envName, port), port);
            }

            if (portCombo->count() == 0) {
                QMessageBox::warning(this, "Run Module", "No browser environment is running. Open an environment first.");
                return;
            }

            formLayout->addRow("HOST:", portCombo);

            // QList<QPair<QString, QLineEdit*>> editors;
            // for (const QString &fieldName : fieldNames) {
            //     QLineEdit *editor = new QLineEdit(&dialog);
            //     editor->setPlaceholderText(QString("Enter %1").arg(fieldName));
            //     editors.append(qMakePair(fieldName, editor));
            //     formLayout->addRow(fieldName + ":", editor);
            // }

            QList<QPair<QString, QLineEdit*>> editors;
            if (fieldNames.isEmpty()) fieldNames = {"video", "title", "desc"};
            for (const QString &fieldName : fieldNames) {
                if (fieldName == "video") {
                    // video 字段：QLineEdit + 浏览按钮
                    QWidget *videoContainer = new QWidget(&dialog);
                    QHBoxLayout *videoLayout = new QHBoxLayout(videoContainer);
                    videoLayout->setContentsMargins(0, 0, 0, 0);
                    videoLayout->setSpacing(6);
                    QLineEdit *editor = new QLineEdit(&dialog);
                    editor->setPlaceholderText("选择视频或图片文件");
                    QPushButton *btnBrowse = new QPushButton("浏览...", &dialog);
                    btnBrowse->setCursor(Qt::PointingHandCursor);
                    btnBrowse->setFixedWidth(80);
                    btnBrowse->setStyleSheet(
                        "QPushButton { background-color: #0984e3; color: white; border: none;"
                        "  border-radius: 4px; padding: 6px 12px; font-size: 12px; font-weight: 600; }"
                        "QPushButton:hover { background-color: #0770c4; }");
                    connect(btnBrowse, &QPushButton::clicked, &dialog, [editor]() {
                        QString filePath = QFileDialog::getOpenFileName(
                            nullptr, "选择视频或图片文件", QString(),
                            "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm);;"
                            "图片文件 (*.png *.jpg *.jpeg *.gif *.bmp *.webp);;"
                            "所有文件 (*.*)");
                        if (!filePath.isEmpty()) {
                            editor->setText(filePath);
                        }
                    });
                    videoLayout->addWidget(editor, 1);
                    videoLayout->addWidget(btnBrowse);
                    editors.append(qMakePair(fieldName, editor));
                    formLayout->addRow(fieldName + ":", videoContainer);
                } else {
                    QLineEdit *editor = new QLineEdit(&dialog);
                    editor->setPlaceholderText(QString("Enter %1").arg(fieldName));
                    editors.append(qMakePair(fieldName, editor));
                    formLayout->addRow(fieldName + ":", editor);
                }
            }



            dialogLayout->addLayout(formLayout);

            QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            buttonBox->button(QDialogButtonBox::Ok)->setText("Run");
            buttonBox->button(QDialogButtonBox::Cancel)->setText("Cancel");
            dialogLayout->addWidget(buttonBox);

            connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog, &editors]() {
                for (const auto &pair : editors) {
                    if (pair.second->text().trimmed().isEmpty()) {
                        QMessageBox::warning(&dialog, "Run Module", QString("%1 is required.").arg(pair.first));
                        pair.second->setFocus();
                        return;
                    }
                }
                dialog.accept();
            });

            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            QString hostPort = portCombo->currentData().toString();
            QJsonObject runInput;
            runInput["HOST"] = hostPort;
            for (const auto &pair : editors) {
                runInput[pair.first] = pair.second->text().trimmed();
            }

            // QString runInputPath = moduleDir.absoluteFilePath(QString("run_input_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz")));
            // QFile runInputFile(runInputPath);
            // if (runInputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            //     runInputFile.write(QJsonDocument(runInput).toJson(QJsonDocument::Indented));
            //     runInputFile.close();
            // } else {
            //     QMessageBox::warning(this, "Run Module", "Cannot write runtime input file.");
            //     return;
            // }

            QString runner = moduleJson["runner"].toString().trimmed();
            if (runner.isEmpty()) {
                runner = moduleJson["runtime"].toString().trimmed();
            }
            if (runner.isEmpty()) {
                runner = "python";
            }

            QStringList arguments;

            // 【第一位置必须放脚本文件】
            arguments << scriptPath;
            // host端口参数
            arguments << "--host" << hostPort;
            // 自定义字段参数
            for (const auto &pair : editors) {
                arguments << QString("--%1").arg(pair.first) << pair.second->text().trimmed();
            }


            // arguments  << "--host" << hostPort;
            // for (const auto &pair : editors) {
            //     arguments << QString("--%1").arg(pair.first) << pair.second->text().trimmed();
            // }

            // ========== 终端风格输出窗口 ==========
            QDialog *termDlg = new QDialog(this);
            termDlg->setWindowTitle(QString("模块运行 - %1").arg(modId));
            termDlg->setFixedSize(640, 500);
            // 生命周期由 deleteLater() 管理，不用 WA_DeleteOnClose

            QVBoxLayout *termLay = new QVBoxLayout(termDlg);
            termLay->setSpacing(10);

            QLabel *lblTermTitle = new QLabel(termDlg);
            lblTermTitle->setWordWrap(true);
            lblTermTitle->setText(QString("<b>模块: %1</b>&nbsp;&nbsp;|&nbsp;&nbsp;脚本: %2&nbsp;&nbsp;|&nbsp;&nbsp;HOST: %3&nbsp;&nbsp;|&nbsp;&nbsp;状态: 运行中")
                                      .arg(modId, QFileInfo(scriptPath).fileName(), hostPort));
            termLay->addWidget(lblTermTitle);

            QProgressBar *bar = new QProgressBar(termDlg);
            bar->setRange(0, 0); // 不确定进度（忙等动画）
            bar->setTextVisible(false);
            bar->setFixedHeight(18);
            termLay->addWidget(bar);



            QTextEdit *logEdit = new QTextEdit(termDlg);
            logEdit->setReadOnly(true);
            logEdit->setStyleSheet(
                "QTextEdit {"
                "  background-color: #1e1e1e; color: #d4d4d4;"
                "  font-family: 'Consolas', 'Courier New', monospace;"
                "  font-size: 12px; border: 1px solid #333;"
                "  border-radius: 4px; padding: 8px;"
                "}");
            termLay->addWidget(logEdit);

            QHBoxLayout *btnLay = new QHBoxLayout();
            btnLay->addStretch();
            QPushButton *btnClose = new QPushButton("关闭", termDlg);
            btnClose->setEnabled(false);
            btnClose->setStyleSheet(
                "QPushButton { background-color: #1e90ff; color: white; border: none;"
                "  border-radius: 4px; padding: 8px 28px; font-size: 13px; font-weight: bold; }"
                "QPushButton:hover { background-color: #3742fa; }"
                "QPushButton:disabled { background-color: #a0a0a0; }");
            btnLay->addWidget(btnClose);
            termLay->addLayout(btnLay);

            auto appendLog = [logEdit](const QString &text) {
                logEdit->append(text);
                QScrollBar *sb = logEdit->verticalScrollBar();
                if (sb) sb->setValue(sb->maximum());
            };

            // 启动信息
            appendLog(QString("$ %1 %2").arg(runner, arguments.join(" ")));
            appendLog(QString("  工作目录: %1").arg(modPath));
            appendLog("");

            QProcess *moduleProcess = new QProcess(termDlg);
            moduleProcess->setWorkingDirectory(modPath);
            moduleProcess->setProcessChannelMode(QProcess::MergedChannels);

            QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
            processEnv.insert("HOST", hostPort);
            for (const auto &pair : editors) {
                processEnv.insert(pair.first, pair.second->text().trimmed());
            }
            moduleProcess->setProcessEnvironment(processEnv);

            connect(moduleProcess, &QProcess::readyReadStandardOutput, termDlg, [this, moduleProcess, modId, appendLog]() {
                QString output = QString::fromLocal8Bit(moduleProcess->readAllStandardOutput());
                if (!output.trimmed().isEmpty()) {
                    appendLog(output.trimmed());
                    writeLog(QString("模块输出[%1]").arg(modId), output.trimmed());
                }
            });

            connect(moduleProcess, &QProcess::errorOccurred, termDlg, [this, moduleProcess, modId, appendLog, bar, btnClose](QProcess::ProcessError error) {
                QString errOutput = QString::fromLocal8Bit(moduleProcess->readAll());
                appendLog(QString("\n? 进程错误: %1 (错误类型: %2)").arg(moduleProcess->errorString()).arg(error));
                writeLog("模块错误", QString("单独运行 — %1, 错误类型: %2, 描述: %3")
                                             .arg(modId).arg(error).arg(moduleProcess->errorString()));
                if (!errOutput.trimmed().isEmpty()) {
                    appendLog(errOutput.trimmed());
                    writeLog(QString("模块错误详情[%1]").arg(modId), errOutput.trimmed());
                }
                bar->setRange(0, 100);
                bar->setValue(0);
                bar->setFormat("启动失败");
                btnClose->setEnabled(true);
            });

            connect(moduleProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), termDlg,
                    [this, moduleProcess, modId, appendLog, bar, btnClose, lblTermTitle](int exitCode, QProcess::ExitStatus exitStatus) {
                        // 读取剩余输出
                        QString remaining = QString::fromLocal8Bit(moduleProcess->readAll());
                        if (!remaining.trimmed().isEmpty()) {
                            appendLog(remaining.trimmed());
                            writeLog(QString("模块输出[%1]").arg(modId), remaining.trimmed());
                        }

                        bar->setRange(0, 100);
                        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                            bar->setValue(100);
                            bar->setFormat("执行成功");
                            appendLog("\n? 模块执行完毕 (exitCode: 0)");
                            writeLog("模块完成", QString("单独运行 — %1, exitCode: 0").arg(modId));
                            lblTermTitle->setText(lblTermTitle->text().replace("运行中", "已完成 ?"));
                        } else {
                            bar->setValue(0);
                            bar->setFormat(QString("执行失败 (exitCode: %1)").arg(exitCode));
                            appendLog(QString("\n? 模块执行失败 (exitCode: %1, exitStatus: %2)")
                                          .arg(exitCode).arg(exitStatus));
                            writeLog("模块失败", QString("单独运行 — %1, exitCode: %2, exitStatus: %3")
                                                         .arg(modId).arg(exitCode).arg(exitStatus));
                            lblTermTitle->setText(lblTermTitle->text().replace("运行中", "执行失败 ?"));
                        }
                        btnClose->setEnabled(true);
                        moduleProcess->deleteLater();
                    });

            moduleProcess->start(runner, arguments);
            if (!moduleProcess->waitForStarted(3000)) {
                QString errOutput = QString::fromLocal8Bit(moduleProcess->readAll());
                appendLog(QString("\n? 脚本启动失败: %1").arg(moduleProcess->errorString()));
                appendLog(QString("  Runner: %1").arg(runner));
                appendLog(QString("  参数: %1").arg(arguments.join(" ")));
                writeLog("模块失败", QString("单独运行 — %1, 启动失败: %2")
                                             .arg(modId).arg(moduleProcess->errorString()));
                if (!errOutput.trimmed().isEmpty()) {
                    appendLog(errOutput.trimmed());
                    writeLog(QString("模块启动失败详情[%1]").arg(modId), errOutput.trimmed());
                }
                bar->setRange(0, 100);
                bar->setValue(0);
                bar->setFormat("启动失败");
                btnClose->setEnabled(true);
                moduleProcess->deleteLater();
            } else {
                appendLog("$ 进程已启动，等待输出...\n");
                writeLog("模块运行", QString("单独运行 — %1, runner: %2, 参数: %3")
                                             .arg(modId, runner, arguments.join(" ")));
            }

            // 禁止执行期间关闭对话框（X 按钮拦截）
            termDlg->setWindowFlags(termDlg->windowFlags() & ~Qt::WindowCloseButtonHint);
            connect(btnClose, &QPushButton::clicked, termDlg, &QDialog::accept);
            termDlg->exec();
            termDlg->deleteLater();
        });

        validRow++;
    }
    ui->tableWidget_3->setRowCount(validRow);
}



void MainWindow::refreshEnvCards()
{
    if (!m_envLayout) {
        qDebug() << "[refreshEnvCards] m_envLayout 为空，跳过";
        return;
    }

    qDebug() << "\n===== [refreshEnvCards] 开始刷新 =====";
    int totalItems = m_envLayout->count();
    qDebug() << "[refreshEnvCards] 旧布局总项数:" << totalItems;

    // 1. 先遍历收集卡片的 widget 指针（只找 QFrame 类型的卡片，跳过 stretch 等 spacer）
    QList<QWidget*> oldCards;
    for (int i = totalItems - 1; i >= 0; --i) {
        QLayoutItem *item = m_envLayout->itemAt(i);
        if (!item) continue;
        QWidget *w = item->widget();
        if (!w) {
            qDebug() << "[refreshEnvCards] 跳过非widget项(可能是stretch), index:" << i;
            continue;
        }
        // 检查是否是 EnvCard
        QFrame *cardFrame = qobject_cast<QFrame*>(w);
        if (cardFrame && cardFrame->objectName() == "EnvCard") {
            QString cardName = cardFrame->property("env_name").toString();
            QString cardPath = cardFrame->property("config_path").toString();
            qDebug() << "[refreshEnvCards] 找到旧卡片 index:" << i << "名称:" << cardName;
            oldCards.prepend(cardFrame); // prepend 保持原有顺序
        }
    }
    qDebug() << "[refreshEnvCards] 共找到" << oldCards.size() << "张旧卡片";

    // 2. 从布局中移除并销毁所有旧卡片
    for (QWidget *card : oldCards) {
        m_envLayout->removeWidget(card);
        card->hide();
        delete card;
    }

    // 清理 stretch 等非 widget 残留项（全部清除，后面再补 stretch）
    while (m_envLayout->count() > 0) {
        QLayoutItem *item = m_envLayout->takeAt(0);
        delete item;
    }

    // 2. 重新读取 profiles/ 重建卡片
    QDir profilesDir(dataDir() + "/profiles");
    QStringList envSubDirs = profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    qDebug() << "[refreshEnvCards] profiles/ 下子文件夹:" << envSubDirs;

    int newCardCount = 0;
    for (const QString &subDirName : envSubDirs) {
        QString jsonFilePath = profilesDir.absoluteFilePath(subDirName + "/config.json");
        QFile file(jsonFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[refreshEnvCards] 跳过(无法读取):" << jsonFilePath;
            continue;
        }

        QJsonObject configJson = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        EnvironmentInfo info;
        info.name = configJson["env_name"].toString();
        info.configPath = jsonFilePath;
        info.status = "未运行";
        info.group = "默认组";

        QJsonArray modsArray = configJson["modules"].toArray();
        for (int m = 0; m < modsArray.size(); ++m) {
            info.modules.append(modsArray[m].toString());
        }

        qDebug() << "[refreshEnvCards] 创建新卡片:" << info.name << "配置文件:" << jsonFilePath;
        createEnvCard(info);
        newCardCount++;
    }

    qDebug() << "[refreshEnvCards] 共创建" << newCardCount << "张新卡片";

    // 3. 补回弹簧 stretch
    m_envLayout->addStretch();

    qDebug() << "[refreshEnvCards] 当前布局项数(含弹簧):" << m_envLayout->count();
    qDebug() << "===== [refreshEnvCards] 刷新完成 =====\n";
}



// 辅助函数：递归拷贝整个文件夹（包含所有子目录和文件）
bool copyDirectoryRecursively(const QString &srcPath, const QString &dstPath)
{
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;

    QDir dstDir(dstPath);
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) return false;
    }

    // 遍历源目录下的所有文件和文件夹（排除 . 和 .. 目录）
    QFileInfoList list = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QFileInfo fileInfo, list) {
        QString srcSubPath = fileInfo.absoluteFilePath();
        QString dstSubPath = dstDir.absoluteFilePath(fileInfo.fileName());

        if (fileInfo.isDir()) {
            // 如果是文件夹，递归调用自身
            if (!copyDirectoryRecursively(srcSubPath, dstSubPath)) {
                return false;
            }
        } else {
            // 如果是文件，直接拷贝
            if (QFile::exists(dstSubPath)) {
                QFile::remove(dstSubPath); // 存在同名文件先删除
            }
            if (!QFile::copy(srcSubPath, dstSubPath)) {
                return false;
            }
        }
    }
    return true;
}



//加载程序包
void MainWindow::on_pushButton_10_clicked()
{
    QFileDialog dialog(this, tr("选择模块文件夹"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    for (QListView *view : dialog.findChildren<QListView*>()) {
        view->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    for (QTreeView *view : dialog.findChildren<QTreeView*>()) {
        view->setSelectionMode(QAbstractItemView::MultiSelection);
    }

    if (dialog.exec() != QDialog::Accepted) return;

    QStringList folderPaths = dialog.selectedFiles();
    folderPaths.removeDuplicates();
    if (folderPaths.isEmpty()) return;

    QDir runDir(QCoreApplication::applicationDirPath());
    QString modulesRoot = runDir.absoluteFilePath("modules");
    if (!QDir().mkpath(modulesRoot)) {
        QMessageBox::critical(this, "Import module", "Cannot create the modules directory.");
        return;
    }

    int successCount = 0;
    QStringList failedMessages;

    for (const QString &folderPath : folderPaths) {
        QString jsonFilePath = QDir(folderPath).absoluteFilePath("config.json");
        QFile file(jsonFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            failedMessages << QString("%1：未检测到有效的 config.json").arg(QDir::toNativeSeparators(folderPath));
            writeLog("导入模块", QString("失败 — 未检测到 config.json，来源: %1").arg(folderPath));
            continue;
        }

        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
        if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
            failedMessages << QString("%1：config.json 格式错误（%2）")
                                  .arg(QDir::toNativeSeparators(folderPath), parseError.errorString());
            writeLog("导入模块", QString("失败 — config.json 格式错误，来源: %1").arg(folderPath));
            continue;
        }

        QJsonObject moduleJson = jsonDoc.object();
        QString modId = moduleJson["id"].toString().trimmed();
        QString modName = moduleJson["name"].toString().trimmed();

        if (modId.isEmpty() || modName.isEmpty()) {
            failedMessages << QString("%1：缺失必须的 id 或 name 字段").arg(QDir::toNativeSeparators(folderPath));
            writeLog("导入模块", QString("失败 — 缺失 id 或 name，来源: %1").arg(folderPath));
            continue;
        }

        static const QRegularExpression safeModuleId("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$");
        if (!safeModuleId.match(modId).hasMatch()) {
            failedMessages << QString("%1：模块 id 只能包含字母、数字、下划线和连字符").arg(modName);
            writeLog("导入模块", QString("失败 — 非法模块 ID: %1").arg(modId));
            continue;
        }

        const QString cleanModulesRoot = QDir::cleanPath(QFileInfo(modulesRoot).absoluteFilePath());
        QString targetModPath = QDir::cleanPath(QDir(cleanModulesRoot).absoluteFilePath(modId));
        const QString requiredPrefix = cleanModulesRoot + QDir::separator();
        if (!targetModPath.startsWith(requiredPrefix, Qt::CaseInsensitive)) {
            failedMessages << QString("%1：模块目标路径越界").arg(modName);
            writeLog("导入模块", QString("失败 — 模块目标路径越界: %1").arg(targetModPath));
            continue;
        }

        const QString sourceCanonical = QFileInfo(folderPath).canonicalFilePath();
        const QString targetCanonical = QFileInfo(targetModPath).canonicalFilePath();
        if (!sourceCanonical.isEmpty() && !targetCanonical.isEmpty()
            && sourceCanonical.compare(targetCanonical, Qt::CaseInsensitive) == 0) {
            failedMessages << QString("%1：不能从已安装的目标目录覆盖自身").arg(modName);
            continue;
        }

        const QString stagingName = ".import-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString stagingPath = QDir(cleanModulesRoot).absoluteFilePath(stagingName);
        if (!copyDirectoryRecursively(folderPath, stagingPath)) {
            QDir(stagingPath).removeRecursively();
            failedMessages << QString("%1：文件拷贝失败").arg(modName);
            writeLog("导入模块", QString("失败 — 暂存拷贝失败，名称: %1, ID: %2").arg(modName, modId));
            continue;
        }

        QDir targetDir(targetModPath);
        if (targetDir.exists()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "覆盖提示",
                QString("模块 ID【%1】已存在，是否覆盖旧模块？").arg(modId),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No) {
                QDir(stagingPath).removeRecursively();
                failedMessages << QString("%1：用户取消覆盖").arg(modName);
                writeLog("导入模块", QString("跳过 — 用户取消覆盖，名称: %1, ID: %2").arg(modName, modId));
                continue;
            }

            const QString backupName = ".backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!QDir(cleanModulesRoot).rename(modId, backupName)) {
                QDir(stagingPath).removeRecursively();
                failedMessages << QString("%1：旧模块备份失败").arg(modName);
                continue;
            }
            if (!QDir(cleanModulesRoot).rename(stagingName, modId)) {
                QDir(cleanModulesRoot).rename(backupName, modId);
                QDir(stagingPath).removeRecursively();
                failedMessages << QString("%1：模块替换失败，已恢复旧版本").arg(modName);
                continue;
            }
            QDir(QDir(cleanModulesRoot).absoluteFilePath(backupName)).removeRecursively();
            successCount++;
            writeLog("导入模块", QString("成功 — 名称: %1, ID: %2, 来源: %3").arg(modName, modId, folderPath));
        } else if (QDir(cleanModulesRoot).rename(stagingName, modId)) {
            successCount++;
            writeLog("导入模块", QString("成功 — 名称: %1, ID: %2, 来源: %3").arg(modName, modId, folderPath));
        } else {
            QDir(stagingPath).removeRecursively();
            failedMessages << QString("%1：模块安装失败").arg(modName);
            writeLog("导入模块", QString("失败 — 暂存目录重命名失败，名称: %1, ID: %2").arg(modName, modId));
        }
    }

    loadModulesFromJson();

    if (failedMessages.isEmpty()) {
        QMessageBox::information(this, "成功", QString("已成功导入 %1 个模块。").arg(successCount));
    } else {
        QMessageBox::warning(
            this,
            "导入完成",
            QString("成功导入 %1 个模块，%2 个失败/跳过：\n\n%3")
                .arg(successCount)
                .arg(failedMessages.size())
                .arg(failedMessages.join("\n")));
    }
}


QString MainWindow::dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ProfilePower";
    QDir().mkpath(dir);
    return dir;
}
