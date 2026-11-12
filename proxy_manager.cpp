#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "proxy_config.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QUuid>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString modeName(const QString &mode)
{
    if (mode == "dynamic") return QString::fromUtf8("动态代理");
    if (mode == "integrated") return QString::fromUtf8("集成代理");
    if (mode == "wireguard") return QString::fromUtf8("WireGuard 节点");
    return QString::fromUtf8("自定义代理");
}

QString profileSummary(const QJsonObject &profile)
{
    if (profile["mode"].toString() == "wireguard") return ProxyConfig::summary(profile);
    const QString mode = profile["mode"].toString();
    if (mode == "dynamic") return profile["api_url"].toString();
    const QString endpoint = QString("%1://%2:%3")
                                 .arg(profile["protocol"].toString("HTTP").toLower(),
                                      profile["host"].toString(), profile["port"].toString());
    if (mode == "integrated") return profile["provider"].toString() + "  ·  " + endpoint;
    return endpoint;
}
}

void MainWindow::initProxyManager()
{
    if (m_proxyCardsLayout) return;

    ui->page_3->setStyleSheet("QWidget#page_3 { background: #f3f5f8; }");
    auto rootLayout = new QVBoxLayout(ui->page_3);
    rootLayout->setContentsMargins(26, 20, 26, 20);
    rootLayout->setSpacing(16);

    auto headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    ui->label_4->setText(QString::fromUtf8("代理管理"));
    headerLayout->addWidget(ui->label_4);
    headerLayout->addStretch();

    const QString addButtonStyle =
        "QPushButton { background: #ffffff; color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; padding: 9px 15px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { border-color: #2563eb; color: #1d4ed8; background: #f8fbff; }"
        "QPushButton:pressed { background: #eaf2ff; }";

    auto addCustom = new QPushButton(QString::fromUtf8("+ 手动填写"), ui->page_3);
    auto addDynamic = new QPushButton(QString::fromUtf8("+ 导入配置文件"), ui->page_3);
    auto addIntegrated = new QPushButton(QString::fromUtf8("+ 导入文件夹"), ui->page_3);
    addCustom->setObjectName("addCustomProxyButton");
    addDynamic->setObjectName("addDynamicProxyButton");
    addIntegrated->setObjectName("addIntegratedProxyButton");
    for (QPushButton *button : {addCustom, addDynamic, addIntegrated}) {
        button->setStyleSheet(addButtonStyle);
        button->setCursor(Qt::PointingHandCursor);
        headerLayout->addWidget(button);
    }
    rootLayout->addLayout(headerLayout);

    auto scrollArea = new QScrollArea(ui->page_3);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    auto cardsContent = new QWidget(scrollArea);
    cardsContent->setStyleSheet("background: transparent;");
    m_proxyCardsLayout = new QVBoxLayout(cardsContent);
    m_proxyCardsLayout->setContentsMargins(0, 0, 8, 0);
    m_proxyCardsLayout->setSpacing(12);
    m_proxyCardsLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(cardsContent);
    rootLayout->addWidget(scrollArea, 1);

    connect(addCustom, &QPushButton::clicked, this, [this]() { openProxyEditor("custom"); });
    connect(addDynamic, &QPushButton::clicked, this, [this]() { chooseProxyFiles(); });
    connect(addIntegrated, &QPushButton::clicked, this, [this]() { chooseProxyFiles(true); });
    refreshProxyManagerCards();
}

void MainWindow::refreshProxyManagerCards()
{
    if (!m_proxyCardsLayout) return;
    while (QLayoutItem *item = m_proxyCardsLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_proxyEmptyLabel = nullptr;

    const QJsonArray profiles = loadProxyProfiles();
    if (profiles.isEmpty()) {
        m_proxyEmptyLabel = new QLabel(QString::fromUtf8("暂无代理"));
        m_proxyEmptyLabel->setAlignment(Qt::AlignCenter);
        m_proxyEmptyLabel->setMinimumHeight(220);
        m_proxyEmptyLabel->setStyleSheet("color: #94a3b8; font-size: 14px; background: #ffffff; border: 1px dashed #cbd5e1; border-radius: 8px;");
        m_proxyCardsLayout->addWidget(m_proxyEmptyLabel);
        return;
    }

    const QString secondaryButtonStyle =
        "QPushButton { background: #ffffff; color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; padding: 7px 14px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { border-color: #94a3b8; background: #f8fafc; }";
    const QString deleteButtonStyle =
        "QPushButton { background: transparent; color: #dc2626; border: 1px solid transparent; border-radius: 6px; padding: 7px 12px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: #fff1f2; border-color: #fecdd3; }";

    for (const QJsonValue &value : profiles) {
        const QJsonObject profile = value.toObject();
        const QString id = profile["id"].toString();
        const QString mode = profile["mode"].toString("custom");

        auto card = new QFrame();
        card->setObjectName("ProxyCard");
        card->setStyleSheet(
            "QFrame#ProxyCard { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }"
            "QFrame#ProxyCard:hover { border-color: #b8c7dc; }");
        auto cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 17, 20, 16);
        cardLayout->setSpacing(12);

        auto titleRow = new QHBoxLayout();
        auto title = new QLabel(profile["name"].toString(), card);
        title->setTextFormat(Qt::PlainText);
        title->setStyleSheet("color: #172033; font-size: 16px; font-weight: 700; border: none; background: transparent;");
        auto badge = new QLabel(modeName(mode), card);
        QString badgeStyle = "color: #1d4ed8; background: #eaf2ff;";
        if (mode == "dynamic") badgeStyle = "color: #087f5b; background: #e8f7f1;";
        else if (mode == "integrated") badgeStyle = "color: #b45309; background: #fff4e5;";
        badge->setStyleSheet(badgeStyle + " border: none; border-radius: 5px; padding: 4px 9px; font-size: 11px; font-weight: 700;");
        titleRow->addWidget(title);
        titleRow->addWidget(badge);
        titleRow->addStretch();
        cardLayout->addLayout(titleRow);

        auto summary = new QLabel(profileSummary(profile), card);
        summary->setTextFormat(Qt::PlainText);
        summary->setWordWrap(true);
        summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
        summary->setStyleSheet("color: #475569; font-size: 13px; border: none; background: transparent;");
        cardLayout->addWidget(summary);

        auto metadataRow = new QHBoxLayout();
        QString authentication = profile["username"].toString().isEmpty()
                                     ? QString::fromUtf8("无账号认证")
                                     : QString::fromUtf8("账号：") + profile["username"].toString();
        auto metadata = new QLabel(authentication, card);
        metadata->setStyleSheet("color: #94a3b8; font-size: 12px; border: none; background: transparent;");
        auto status = new QLabel(QString::fromUtf8("未测试"), card);
        status->setStyleSheet("color: #64748b; font-size: 12px; border: none; background: transparent;");
        metadataRow->addWidget(metadata);
        metadataRow->addSpacing(14);
        metadataRow->addWidget(status);
        metadataRow->addStretch();
        cardLayout->addLayout(metadataRow);

        auto separator = new QFrame(card);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("border: none; background: #edf1f5; max-height: 1px;");
        cardLayout->addWidget(separator);

        auto actions = new QHBoxLayout();
        actions->addStretch();
        auto testButton = new QPushButton(QString::fromUtf8("测试连接"), card);
        auto editButton = new QPushButton(QString::fromUtf8("编辑"), card);
        auto deleteButton = new QPushButton(QString::fromUtf8("删除"), card);
        testButton->setStyleSheet(secondaryButtonStyle);
        editButton->setStyleSheet(secondaryButtonStyle);
        deleteButton->setStyleSheet(deleteButtonStyle);
        for (QPushButton *button : {testButton, editButton, deleteButton}) button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(testButton);
        actions->addWidget(editButton);
        actions->addWidget(deleteButton);
        cardLayout->addLayout(actions);

        connect(testButton, &QPushButton::clicked, this, [this, profile, status]() { testProxyProfile(profile, status); });
        connect(editButton, &QPushButton::clicked, this, [this, mode, id]() { openProxyEditor(mode, id); });
        connect(deleteButton, &QPushButton::clicked, this, [this, id, profile]() {
            const auto answer = QMessageBox::question(this, QString::fromUtf8("删除代理"),
                                                       QString::fromUtf8("确定删除代理“%1”吗？").arg(profile["name"].toString()));
            if (answer != QMessageBox::Yes) return;
            QJsonArray updated;
            for (const QJsonValue &item : loadProxyProfiles()) {
                if (item.toObject()["id"].toString() != id) updated.append(item);
            }
            if (!saveProxyProfiles(updated)) {
                QMessageBox::warning(this, QString::fromUtf8("删除代理"), QString::fromUtf8("代理数据保存失败。"));
                return;
            }
            refreshProxyManagerCards();
            refreshSavedProxyCombo();
        });

        m_proxyCardsLayout->addWidget(card);
    }
}

void MainWindow::openProxyEditor(const QString &mode, const QString &profileId)
{
    if (mode == "wireguard") { openWireGuardEditor(profileId); return; }
    QJsonObject existing;
    const QJsonArray profiles = loadProxyProfiles();
    for (const QJsonValue &value : profiles) {
        if (value.toObject()["id"].toString() == profileId) {
            existing = value.toObject();
            break;
        }
    }
    const bool editing = !existing.isEmpty();

    QDialog dialog(this);
    dialog.setWindowTitle((editing ? QString::fromUtf8("编辑") : QString::fromUtf8("添加")) + modeName(mode));
    dialog.setMinimumWidth(520);
    dialog.setStyleSheet(
        "QDialog { background: #f8fafc; }"
        "QLabel { color: #334155; font-size: 13px; }"
        "QLineEdit, QComboBox { background: #ffffff; color: #172033; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px 11px; min-height: 22px; }"
        "QLineEdit:focus, QComboBox:focus { border-color: #2563eb; }"
        "QPushButton { border-radius: 6px; padding: 8px 20px; min-height: 22px; font-weight: 600; }"
        "QPushButton:hover { background: #eaf2ff; }");
    auto rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(18);

    auto heading = new QLabel((editing ? QString::fromUtf8("编辑") : QString::fromUtf8("添加")) + modeName(mode), &dialog);
    heading->setStyleSheet("color: #172033; font-size: 18px; font-weight: 700;");
    rootLayout->addWidget(heading);

    auto form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(13);
    auto nameEdit = new QLineEdit(existing["name"].toString(), &dialog);
    nameEdit->setObjectName("proxyNameEdit");
    nameEdit->setPlaceholderText(QString::fromUtf8("例如：美国住宅代理"));
    form->addRow(QString::fromUtf8("名称"), nameEdit);

    auto protocolCombo = new QComboBox(&dialog);
    protocolCombo->setObjectName("proxyProtocolCombo");
    protocolCombo->addItems({"HTTP", "HTTPS", "SOCKS5"});
    protocolCombo->setCurrentText(existing["protocol"].toString("HTTP"));
    auto hostEdit = new QLineEdit(existing["host"].toString(), &dialog);
    auto portEdit = new QLineEdit(existing["port"].toString(), &dialog);
    hostEdit->setObjectName("proxyHostEdit");
    portEdit->setObjectName("proxyPortEdit");
    portEdit->setValidator(new QIntValidator(1, 65535, portEdit));
    auto usernameEdit = new QLineEdit(existing["username"].toString(), &dialog);
    auto passwordEdit = new QLineEdit(existing["password"].toString(), &dialog);
    usernameEdit->setObjectName("proxyUsernameEdit");
    passwordEdit->setObjectName("proxyPasswordEdit");
    passwordEdit->setEchoMode(QLineEdit::Password);
    auto apiUrlEdit = new QLineEdit(existing["api_url"].toString(), &dialog);
    apiUrlEdit->setObjectName("proxyApiUrlEdit");
    apiUrlEdit->setPlaceholderText("https://provider.example/api/get-proxy");
    auto providerCombo = new QComboBox(&dialog);
    providerCombo->setObjectName("proxyProviderCombo");
    providerCombo->addItems({"Bright Data", "Oxylabs", "IPRoyal", "Custom gateway"});
    providerCombo->setCurrentText(existing["provider"].toString("Bright Data"));

    const bool dynamicMode = mode == "dynamic";
    const bool integratedMode = mode == "integrated";
    protocolCombo->setVisible(!dynamicMode);
    hostEdit->setVisible(!dynamicMode);
    portEdit->setVisible(!dynamicMode);
    usernameEdit->setVisible(!dynamicMode);
    passwordEdit->setVisible(!dynamicMode);
    apiUrlEdit->setVisible(dynamicMode);
    providerCombo->setVisible(integratedMode);

    if (mode == "dynamic") {
        form->addRow("API URL", apiUrlEdit);
    } else {
        if (mode == "integrated") form->addRow(QString::fromUtf8("服务商"), providerCombo);
        form->addRow(QString::fromUtf8("协议"), protocolCombo);
        form->addRow(mode == "integrated" ? QString::fromUtf8("网关") : QString::fromUtf8("主机"), hostEdit);
        form->addRow(QString::fromUtf8("端口"), portEdit);
        form->addRow(QString::fromUtf8("用户名"), usernameEdit);
        form->addRow(QString::fromUtf8("密码"), passwordEdit);
    }
    rootLayout->addLayout(form);

    auto applyProviderDefaults = [providerCombo, hostEdit, portEdit]() {
        const QString provider = providerCombo->currentText();
        if (provider == "Bright Data") { hostEdit->setText("brd.superproxy.io"); portEdit->setText("33335"); }
        else if (provider == "Oxylabs") { hostEdit->setText("pr.oxylabs.io"); portEdit->setText("7777"); }
        else if (provider == "IPRoyal") { hostEdit->setText("geo.iproyal.com"); portEdit->setText("12321"); }
    };
    if (mode == "integrated") {
        connect(providerCombo, &QComboBox::currentTextChanged, &dialog, [applyProviderDefaults]() { applyProviderDefaults(); });
        if (!editing) applyProviderDefaults();
    }

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QString::fromUtf8("保存"));
    buttons->button(QDialogButtonBox::Save)->setObjectName("proxySaveButton");
    buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("取消"));
    buttons->button(QDialogButtonBox::Save)->setStyleSheet("background: #2563eb; color: white; border: 1px solid #2563eb;");
    buttons->button(QDialogButtonBox::Cancel)->setStyleSheet("background: #ffffff; color: #334155; border: 1px solid #cbd5e1;");
    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, [&]() {
        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, QString::fromUtf8("保存代理"), QString::fromUtf8("请输入代理名称。"));
            return;
        }
        for (const QJsonValue &value : profiles) {
            const QJsonObject current = value.toObject();
            if (current["id"].toString() != profileId && current["name"].toString().compare(name, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(&dialog, QString::fromUtf8("保存代理"), QString::fromUtf8("代理名称已存在。"));
                return;
            }
        }
        if (mode == "dynamic") {
            const QUrl url = QUrl::fromUserInput(apiUrlEdit->text().trimmed());
            if (!url.isValid() || url.host().isEmpty()) {
                QMessageBox::warning(&dialog, QString::fromUtf8("保存代理"), QString::fromUtf8("请输入有效的动态代理 API 地址。"));
                return;
            }
        } else {
            QString error;
            const QJsonObject candidate{{"protocol", protocolCombo->currentText()},
                {"host", hostEdit->text().trimmed()}, {"port", portEdit->text().trimmed()}};
            if (!ProxyConfig::validate(candidate, &error)) {
                QMessageBox::warning(&dialog, QString::fromUtf8("保存代理"), error);
                return;
            }
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) return;

    QJsonObject profile = existing;
    if (!editing) {
        profile["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        profile["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    profile["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    profile["name"] = nameEdit->text().trimmed();
    profile["mode"] = mode;
    if (mode == "dynamic") {
        profile["api_url"] = apiUrlEdit->text().trimmed();
        profile["protocol"] = "HTTP";
        profile.remove("host"); profile.remove("port");
        profile.remove("username"); profile.remove("password"); profile.remove("provider");
    } else {
        profile["protocol"] = protocolCombo->currentText();
        profile["host"] = hostEdit->text().trimmed();
        profile["port"] = portEdit->text().trimmed();
        profile["username"] = usernameEdit->text();
        profile["password"] = passwordEdit->text();
        if (mode == "integrated") profile["provider"] = providerCombo->currentText();
        else profile.remove("provider");
        profile.remove("api_url");
    }

    QJsonArray updated;
    bool replaced = false;
    for (const QJsonValue &value : profiles) {
        if (value.toObject()["id"].toString() == profile["id"].toString()) {
            updated.append(profile);
            replaced = true;
        } else updated.append(value);
    }
    if (!replaced) updated.append(profile);
    if (!saveProxyProfiles(updated)) {
        QMessageBox::warning(this, QString::fromUtf8("保存代理"), QString::fromUtf8("代理数据保存失败。"));
        return;
    }
    refreshProxyManagerCards();
    refreshSavedProxyCombo(profile["id"].toString());
}
