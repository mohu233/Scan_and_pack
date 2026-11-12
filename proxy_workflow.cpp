#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "proxy_config.h"
#include "wireguard_proxy.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QUuid>

void MainWindow::initProxyModeUi()
{
    // Reuse the existing manual form and library instead of duplicating proxy inputs.
    while (ui->tabWidget->count()) ui->tabWidget->removeTab(0);
    ui->tab_3->hide();
    ui->tab_4->hide();
    auto directPage = new QWidget(ui->tabWidget);
    directPage->setObjectName("noProxyPage");
    auto directLayout = new QVBoxLayout(directPage);
    auto directHint = new QLabel(QString::fromUtf8("不使用代理，浏览器直接连接网络。"), directPage);
    directHint->setStyleSheet("color: #64748b; padding: 12px;");
    directLayout->addWidget(directHint);
    ui->tabWidget->addTab(directPage, QString::fromUtf8("无代理"));

    auto addPage = new QWidget(ui->tabWidget);
    addPage->setObjectName("addProxyPage");
    auto addLayout = new QVBoxLayout(addPage);
    addLayout->setContentsMargins(8, 8, 8, 8);
    addLayout->setSpacing(12);
    proxyInputTabs = new QTabWidget(addPage);
    proxyInputTabs->setObjectName("proxyInputTabs");
    proxyInputTabs->addTab(ui->tab, QString::fromUtf8("手动填写"));
    proxyInputTabs->addTab(ui->tab_2, QString::fromUtf8("导入配置文件"));
    addLayout->addWidget(proxyInputTabs);

    // The same connectivity test works for both manual proxies and imported nodes.
    ui->gridLayout_4->removeWidget(ui->pushButton_9);
    auto testRow = new QHBoxLayout();
    testRow->addStretch();
    testRow->addWidget(ui->pushButton_9);
    addLayout->addLayout(testRow);
    ui->tabWidget->addTab(addPage, QString::fromUtf8("添加代理"));

    ui->comboBox_11->removeItem(ui->comboBox_11->findText("No Proxy"));
    ui->comboBox_11->setCurrentText("HTTP");
    lineEditProxyHost->setObjectName("manualProxyHost");
    lineEditProxyPort->setObjectName("manualProxyPort");
    lineEditProxyUser->setObjectName("manualProxyUsername");
    lineEditProxyPass->setObjectName("manualProxyPassword");
    labelProxyHost->setText(QString::fromUtf8("代理主机"));
    labelProxyPort->setText(QString::fromUtf8("代理端口"));
    labelProxyUser->setText(QString::fromUtf8("用户名"));
    labelProxyPass->setText(QString::fromUtf8("密码"));
    lineEditProxyUser->setPlaceholderText(QString::fromUtf8("选填"));
    lineEditProxyPass->setPlaceholderText(QString::fromUtf8("选填"));

    // Keep the selector readable even with an IPv6 address and several DNS servers.
    ui->horizontalLayout_9->removeWidget(labelSavedProxyStatus);
    labelSavedProxyStatus->setWordWrap(true);
    labelSavedProxyStatus->setTextFormat(Qt::PlainText);
    ui->tab_2->layout()->addWidget(labelSavedProxyStatus);
    auto importRow = new QHBoxLayout();
    importRow->setSpacing(10);
    auto importFiles = new QPushButton(QString::fromUtf8("选择配置文件（可多选）"), ui->tab_2);
    auto importFolder = new QPushButton(QString::fromUtf8("导入文件夹"), ui->tab_2);
    importFiles->setObjectName("importProxyFilesButton");
    importFolder->setObjectName("importProxyFolderButton");
    const QString importStyle = "QPushButton { background: white; color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px 14px; min-height: 22px; font-size: 12px; } QPushButton:hover { border-color: #2563eb; color: #1d4ed8; }";
    importFiles->setStyleSheet(importStyle);
    importFolder->setStyleSheet(importStyle);
    importFiles->setCursor(Qt::PointingHandCursor);
    importFolder->setCursor(Qt::PointingHandCursor);
    importRow->addWidget(importFiles);
    importRow->addWidget(importFolder);
    importRow->addStretch();
    qobject_cast<QVBoxLayout *>(ui->tab_2->layout())->insertLayout(0, importRow);
    auto hint = new QLabel(QString::fromUtf8("支持 WireGuard .conf、代理 JSON、HTTP/HTTPS/SOCKS5 URL 文本。文件夹会扫描子目录；重复节点自动跳过。"), ui->tab_2);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    hint->setStyleSheet("color: #64748b; font-size: 12px; padding: 8px 0;");
    ui->tab_2->layout()->addWidget(hint);
    connect(importFiles, &QPushButton::clicked, this, [this]() { chooseProxyFiles(); });
    connect(importFolder, &QPushButton::clicked, this, [this]() { chooseProxyFiles(true); });
    connect(proxyInputTabs, &QTabWidget::currentChanged, this, [this]() { updateProxyInputsFromCurrentMode(); });
    ui->tabWidget->setCurrentIndex(0);
    updateProxyInputsFromCurrentMode();
}

QJsonObject MainWindow::currentProxyProfile() const
{
    if (!proxyInputTabs || ui->tabWidget->currentIndex() == 0) return QJsonObject{{"protocol", "No Proxy"}, {"mode", "none"}};
    if (proxyInputTabs->currentIndex() == 0) {
        return QJsonObject{{"mode", "custom"}, {"protocol", ui->comboBox_11->currentText()},
                           {"host", lineEditProxyHost->text().trimmed()}, {"port", lineEditProxyPort->text().trimmed()},
                           {"username", lineEditProxyUser->text()}, {"password", lineEditProxyPass->text()}};
    }
    QJsonObject profile = QJsonObject::fromVariantMap(comboBoxSavedProxy->currentData().toMap());
    if (profile["mode"].toString() == "dynamic") {
        for (auto it = m_resolvedDynamicProxy.constBegin(); it != m_resolvedDynamicProxy.constEnd(); ++it) profile[it.key()] = it.value();
    }
    return profile;
}

QNetworkAccessManager *MainWindow::createProxyNetworkManager(const QJsonObject &profile, QString *error)
{
    if (!ProxyConfig::validate(profile, error)) return nullptr;
    auto manager = new QNetworkAccessManager(this);
    const QString protocol = profile["protocol"].toString().toUpper();
    if (protocol == "WIREGUARD") {
        auto wireguard = new WireGuardProxy(manager);
        if (!wireguard->start(profile, error)) { manager->deleteLater(); return nullptr; }
        manager->setProxy(wireguard->networkProxy());
    } else if (protocol == "NO PROXY") {
        manager->setProxy(QNetworkProxy::NoProxy);
    } else if (protocol == "HTTPS") {
        // Qt's HttpProxy speaks plaintext to an upstream proxy. Use the existing
        // TLS-capable relay so the test uses the same transport as the browser.
        QString python = QStandardPaths::findExecutable("python");
        if (python.isEmpty()) python = QStandardPaths::findExecutable("python3");
        QString script;
        const QDir app(QCoreApplication::applicationDirPath());
        for (const QString relative : {"proxy_relay_v2.py", "../../../proxy_relay_v2.py", "../../proxy_relay_v2.py"}) {
            const QString candidate = QDir::cleanPath(app.absoluteFilePath(relative));
            if (QFileInfo::exists(candidate)) { script = candidate; break; }
        }
        QTcpServer picker;
        picker.setProxy(QNetworkProxy::NoProxy);
        if (python.isEmpty() || script.isEmpty() || !picker.listen(QHostAddress::LocalHost, 0)) {
            if (error) *error = QString::fromUtf8("HTTPS 代理测试需要 Python 3 及完整的代理辅助脚本。");
            manager->deleteLater(); return nullptr;
        }
        const quint16 port = picker.serverPort();
        picker.close();
        auto relay = new QProcess(manager);
        relay->setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        for (const auto &field : {qMakePair("TYPE", "protocol"), qMakePair("HOST", "host"), qMakePair("PORT", "port"), qMakePair("USER", "username"), qMakePair("PASS", "password")}) {
            environment.insert("PROFILE_PROXY_" + QString::fromLatin1(field.first), profile[QString::fromLatin1(field.second)].toString());
        }
        relay->setProcessEnvironment(environment);
        relay->start(python, {script, QString::number(port)});
        QByteArray output;
        if (relay->waitForStarted(3000)) {
            QElapsedTimer timer;
            timer.start();
            while (!output.contains("READY ") && timer.elapsed() < 3000 && relay->state() != QProcess::NotRunning) {
                relay->waitForReadyRead(100);
                output += relay->readAllStandardOutput();
            }
        }
        auto stop = [relay]() {
            if (relay->state() != QProcess::NotRunning) {
                relay->terminate();
                if (!relay->waitForFinished(1000)) { relay->kill(); relay->waitForFinished(1000); }
            }
        };
        if (!output.contains("READY ")) {
            stop();
            if (error) *error = QString::fromUtf8("HTTPS 代理测试进程启动失败，请检查 Python 安装。");
            manager->deleteLater(); return nullptr;
        }
        connect(manager, &QObject::destroyed, relay, stop);
        connect(relay, &QProcess::readyReadStandardOutput, relay, [relay]() { relay->readAllStandardOutput(); });
        manager->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", port));
    } else {
        manager->setProxy(QNetworkProxy(protocol == "SOCKS5" ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy,
            profile["host"].toString(), profile["port"].toString().toInt(), profile["username"].toString(), profile["password"].toString()));
    }
    return manager;
}

bool MainWindow::importProxyFiles(const QStringList &paths, QString *message)
{
    QJsonArray profiles = loadProxyProfiles();
    QSet<QString> fingerprints, names;
    for (const QJsonValue &value : profiles) {
        fingerprints.insert(ProxyConfig::fingerprint(value.toObject()));
        names.insert(value.toObject()["name"].toString().toCaseFolded());
    }
    int added = 0, duplicates = 0;
    QString selectedId;
    QStringList errors;
    for (const QString &path : paths) {
        QString error;
        const QJsonArray imported = ProxyConfig::readFile(path, &error);
        if (!error.isEmpty()) { errors << QFileInfo(path).fileName() + QString::fromUtf8("：") + error; continue; }
        for (const QJsonValue &value : imported) {
            QJsonObject profile = value.toObject();
            const QString identity = ProxyConfig::fingerprint(profile);
            if (fingerprints.contains(identity)) {
                ++duplicates;
                for (const QJsonValue &existing : profiles) if (ProxyConfig::fingerprint(existing.toObject()) == identity) selectedId = existing.toObject()["id"].toString();
                continue;
            }
            QString name = profile["name"].toString().trimmed().left(80);
            const QString baseName = name;
            int suffix = 2;
            while (names.contains(name.toCaseFolded())) name = baseName + QString(" (%1)").arg(suffix++);
            profile["name"] = name;
            profile["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            profile["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            profile["updated_at"] = profile["created_at"];
            // Imported identifiers never overwrite entries in the existing proxy library.
            profiles.append(profile);
            fingerprints.insert(identity);
            names.insert(name.toCaseFolded());
            selectedId = profile["id"].toString();
            ++added;
        }
    }
    if (added && !saveProxyProfiles(profiles)) {
        if (message) *message = QString::fromUtf8("配置解析成功，但代理数据保存失败。未导入节点。");
        return false;
    }
    if (message) {
        *message = QString::fromUtf8("已导入 %1 个节点，跳过 %2 个重复节点。").arg(added).arg(duplicates);
        if (!errors.isEmpty()) *message += QString::fromUtf8("\n以下文件未导入：\n") + errors.mid(0, 12).join('\n');
    }
    refreshSavedProxyCombo(selectedId);
    refreshProxyManagerCards();
    if (!selectedId.isEmpty()) {
        ui->tabWidget->setCurrentIndex(1);
        proxyInputTabs->setCurrentIndex(1);
        updateProxyInputsFromCurrentMode();
    }
    return (added + duplicates) > 0;
}

void MainWindow::chooseProxyFiles(bool folder)
{
    QStringList paths;
    const QString initial = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (folder) {
        const QString directory = QFileDialog::getExistingDirectory(this, QString::fromUtf8("选择节点配置文件夹"), initial);
        if (directory.isEmpty()) return;
        QDirIterator files(directory, {"*.conf", "*.json", "*.txt"}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (files.hasNext() && paths.size() < 1000) paths << files.next();
        if (paths.isEmpty()) { QMessageBox::information(this, QString::fromUtf8("导入代理"), QString::fromUtf8("文件夹中没有 .conf、.json 或 .txt 配置文件。")); return; }
    } else {
        paths = QFileDialog::getOpenFileNames(this, QString::fromUtf8("选择代理配置文件"), initial,
            QString::fromUtf8("代理配置 (*.conf *.json *.txt);;所有文件 (*)"));
        if (paths.isEmpty()) return;
    }
    QString message;
    const bool imported = importProxyFiles(paths, &message);
    if (imported) QMessageBox::information(this, QString::fromUtf8("导入代理"), message);
    else QMessageBox::warning(this, QString::fromUtf8("导入代理"), message);
}

void MainWindow::openWireGuardEditor(const QString &profileId)
{
    QJsonArray profiles = loadProxyProfiles();
    int index = -1;
    for (int i = 0; i < profiles.size(); ++i) if (profiles[i].toObject()["id"].toString() == profileId) index = i;
    if (index < 0) return;
    QJsonObject profile = profiles[index].toObject();
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("编辑 WireGuard 节点"));
    dialog.setMinimumWidth(520);
    auto layout = new QVBoxLayout(&dialog);
    auto name = new QLineEdit(profile["name"].toString(), &dialog);
    auto summary = new QLabel(ProxyConfig::summary(profile), &dialog);
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(new QLabel(QString::fromUtf8("节点名称"), &dialog));
    layout->addWidget(name);
    layout->addWidget(summary);
    auto replace = new QPushButton(QString::fromUtf8("替换配置文件"), &dialog);
    layout->addWidget(replace);
    connect(replace, &QPushButton::clicked, &dialog, [&]() {
        const QString path = QFileDialog::getOpenFileName(&dialog, QString::fromUtf8("选择 WireGuard 配置"), {}, "WireGuard (*.conf)");
        if (path.isEmpty()) return;
        QString error;
        const QJsonArray parsed = ProxyConfig::readFile(path, &error);
        if (parsed.size() != 1 || parsed[0].toObject()["protocol"].toString() != "WIREGUARD") {
            QMessageBox::warning(&dialog, QString::fromUtf8("配置无效"), error.isEmpty() ? QString::fromUtf8("请选择 WireGuard 配置文件。") : error); return;
        }
        const QJsonObject replacement = parsed[0].toObject();
        for (auto it = replacement.constBegin(); it != replacement.constEnd(); ++it) if (it.key() != "name") profile[it.key()] = it.value();
        summary->setText(ProxyConfig::summary(profile));
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString proposed = name->text().trimmed();
        if (proposed.isEmpty() || proposed.size() > 80) { QMessageBox::warning(&dialog, QString::fromUtf8("名称无效"), QString::fromUtf8("请输入 1–80 字符的名称。")); return; }
        for (const QJsonValue &value : profiles) if (value.toObject()["id"].toString() != profileId && value.toObject()["name"].toString().compare(proposed, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(&dialog, QString::fromUtf8("名称已存在"), QString::fromUtf8("请使用其他节点名称。")); return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return;
    profile["name"] = name->text().trimmed();
    profile["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    profiles[index] = profile;
    if (!saveProxyProfiles(profiles)) { QMessageBox::warning(this, QString::fromUtf8("保存失败"), QString::fromUtf8("节点数据保存失败。")); return; }
    refreshSavedProxyCombo(profileId);
    refreshProxyManagerCards();
}
