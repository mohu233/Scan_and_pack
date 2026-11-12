#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDirIterator>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QScrollBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScrollArea>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUuid>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "proxy_config.h"
#include "wireguard_proxy.h"
#include "browser_proxy_badge.h"
#include "proxy_exit_ip.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
QString fixture()
{
    const QString key = QString::fromLatin1(QByteArray(32, 'a').toBase64());
    return QString("[Interface]\nPrivateKey = %1\nAddress = 10.0.0.2/32, fd00::2/128\nDNS = 1.1.1.1, 9.9.9.9\nListenPort = 51820\nMTU = 1420\n\n[Peer]\nPublicKey = %1\nAllowedIPs = 0.0.0.0/0, ::/0\nEndpoint = [::1]:51820\nPersistentKeepalive = 25\n").arg(key);
}

QString writeFixture(QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    QFile file(dir.filePath(name));
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size()) return {};
    return file.fileName();
}

void dismissMessage()
{
    QTimer::singleShot(0, []() {
        for (QWidget *widget : QApplication::topLevelWidgets()) if (auto box = qobject_cast<QMessageBox *>(widget)) box->accept();
    });
}

class ExitIpTestProxy : public QTcpServer
{
public:
    QByteArray body = R"({"ip":"203.0.113.8"})";
    int requests = 0;
    bool hold = false;
    QList<QPointer<QTcpSocket>> pending;

    ExitIpTestProxy()
    {
        setProxy(QNetworkProxy::NoProxy);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (hasPendingConnections()) {
                auto socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                    socket->setProperty("answered", true);
                    ++requests;
                    if (hold) pending << socket;
                    else respond(socket);
                });
            }
        });
    }

    void respond(QTcpSocket *socket)
    {
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
            + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }
};
}

class ProxyWorkflowTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("ProfilePowerProxyTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        // The offscreen platform has no Windows font database of its own.
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    }

    void init()
    {
        // Each case owns a separate fixture directory. In particular, the
        // imported WireGuard fixture must not become another case's IP probe.
        QCoreApplication::setApplicationName("ProfilePowerProxyTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    void parsesDualStackAndRedactsSummary()
    {
        QJsonObject profile;
        QString error;
        QVERIFY(ProxyConfig::parseWireGuard(fixture(), &profile, &error));
        QCOMPARE(profile["protocol"].toString(), QString("WIREGUARD"));
        QCOMPARE(profile["host"].toString(), QString("::1"));
        QCOMPARE(profile["port"].toString(), QString("51820"));
        QVERIFY(profile["wireguard_config"].toString().contains("fd00::2/128"));
        QVERIFY(!profile["wireguard_config"].toString().contains("ListenPort"));
        QVERIFY(!ProxyConfig::summary(profile).contains("PrivateKey"));
        QJsonObject reparsed;
        QVERIFY(ProxyConfig::parseWireGuard(profile["wireguard_config"].toString(), &reparsed, &error));
        QCOMPARE(ProxyConfig::fingerprint(profile), ProxyConfig::fingerprint(reparsed));
    }

    void rejectsBrokenAndExecutableConfigs_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("hook") << fixture() + "\nPostUp = run.exe\n";
        QTest::newRow("extra-socks-listener") << fixture() + "\n[Socks5]\nBindAddress = 0.0.0.0:1080\n";
        QTest::newRow("second-peer") << fixture() + "\n[Peer]\n";
        QTest::newRow("bad-key") << QString(fixture()).replace("PrivateKey = ", "PrivateKey = !");
        QTest::newRow("bad-port") << QString(fixture()).replace("[::1]:51820", "node.example:99999");
        QTest::newRow("bad-cidr") << QString(fixture()).replace("10.0.0.2/32", "10.0.0.2/99");
        QTest::newRow("bad-dns") << QString(fixture()).replace("DNS = 1.1.1.1, 9.9.9.9", "DNS = not-an-ip");
        QTest::newRow("duplicate-key") << fixture() + "Endpoint = localhost:2\n";
    }

    void rejectsBrokenAndExecutableConfigs()
    {
        QFETCH(QString, text);
        QString error;
        QVERIFY(!ProxyConfig::parseWireGuard(text, nullptr, &error));
        QVERIFY(!error.isEmpty());
    }

    void readsProxyJsonAndUrls()
    {
        QTemporaryDir dir;
        QString error;
        const QString json = writeFixture(dir, "proxies.json", R"({"proxies":[{"name":"test","protocol":"socks5","host":"::1","port":1080}]})");
        const QJsonArray proxies = ProxyConfig::readFile(json, &error);
        QCOMPARE(proxies.size(), 1);
        QCOMPARE(proxies[0].toObject()["port"].toString(), QString("1080"));
        const QString url = writeFixture(dir, "proxy.txt", "# comment\nsocks5://user:p%40ss@127.0.0.1:1080\n");
        const QJsonArray urls = ProxyConfig::readFile(url, &error);
        QCOMPARE(urls.size(), 1);
        QCOMPARE(urls[0].toObject()["password"].toString(), QString("p@ss"));
        QVERIFY(ProxyConfig::readFile(writeFixture(dir, "invalid.json", "[{\"host\":\"localhost\",\"port\":65536}]"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void manualAndDirectNeverReuseImportedCredentials()
    {
        MainWindow window;
        QCOMPARE(window.ui->tabWidget->count(), 2);
        QCOMPARE(window.ui->tabWidget->tabText(0), QString::fromUtf8("无代理"));
        QCOMPARE(window.ui->tabWidget->tabText(1), QString::fromUtf8("添加代理"));
        QCOMPARE(window.proxyInputTabs->count(), 2);
        window.ui->tabWidget->setCurrentIndex(1);
        window.lineEditProxyHost->setText("127.0.0.1");
        window.lineEditProxyPort->setText("1234");
        window.lineEditProxyPass->setText("test-secret");
        QString error;
        QVERIFY(ProxyConfig::validate(window.currentProxyProfile(), &error));
        window.ui->tabWidget->setCurrentIndex(0);
        const auto direct = window.currentProxyProfile();
        QCOMPARE(direct["protocol"].toString(), QString("No Proxy"));
        QVERIFY(!direct.contains("password"));
        QVERIFY(!direct.contains("host"));
        window.ui->tabWidget->setCurrentIndex(1);
        QCOMPARE(window.currentProxyProfile()["host"].toString(), QString("127.0.0.1"));
        window.resetCreateBrowserForm();
        QCOMPARE(window.currentProxyProfile()["protocol"].toString(), QString("No Proxy"));
        const QJsonObject ipv6{{"proxy_type", "SOCKS5"}, {"proxy_host", "::1"}, {"proxy_port", "1080"}};
        QCOMPARE(window.buildLaunchConfig(ipv6).proxyServer, QString("socks5://[::1]:1080"));
        QJsonObject stale = ipv6;
        stale["proxy_type"] = "No Proxy";
        QVERIFY(window.buildLaunchConfig(stale).proxyServer.isEmpty());
    }

    void importsDeduplicatesPersistsAndSnapshots()
    {
        QTemporaryDir dir;
        const QString path = writeFixture(dir, "test-node.conf", fixture().toUtf8());
        QString message;
        MainWindow window;
        QVERIFY(window.saveProxyProfiles({}));
        QVERIFY(window.importProxyFiles({path}, &message));
        QCOMPARE(window.loadProxyProfiles().size(), 1);
        QCOMPARE(window.proxyInputTabs->currentIndex(), 1);
        QCOMPARE(window.ui->tabWidget->currentIndex(), 1);
        QVERIFY(window.importProxyFiles({path}, &message));
        QCOMPARE(window.loadProxyProfiles().size(), 1);
        const QString selectedId = window.currentProxyProfile()["id"].toString();
        const QString canonical = window.currentProxyProfile()["wireguard_config"].toString();
        window.refreshSavedProxyCombo(selectedId);
        QCOMPARE(window.currentProxyProfile()["id"].toString(), selectedId);
        window.lineEditProxyHost->setText("manual-draft.example");
        window.lineEditProxyPort->setText("8080");
        window.proxyInputTabs->setCurrentIndex(0);
        QCOMPARE(window.currentProxyProfile()["host"].toString(), QString("manual-draft.example"));
        window.proxyInputTabs->setCurrentIndex(1);
        QVERIFY(window.currentProxyProfile()["protocol"].toString() == "WIREGUARD");
        QVERIFY(QFile::remove(path));
        QVERIFY(ProxyConfig::validate(window.currentProxyProfile(), &message));

        window.ui->comboBox_2->clear();
        window.ui->comboBox_2->addItem("Test browser", QCoreApplication::applicationFilePath());
        window.ui->lineEdit_2->setText("wireguard-snapshot-test");
        QString error;
        QVERIFY2(window.validateCreateBrowserForm(&error), qPrintable(error));
        dismissMessage();
        window.on_pushButton_8_clicked();
        QDir profiles(MainWindow::dataDir() + "/profiles");
        bool found = false;
        for (const QString &folder : profiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile config(profiles.filePath(folder + "/config.json"));
            QVERIFY(config.open(QIODevice::ReadOnly));
            const QJsonObject object = QJsonDocument::fromJson(config.readAll()).object();
            if (object["env_name"].toString() != "wireguard-snapshot-test") continue;
            found = true;
            QVERIFY(object["wireguard_proxy"].toObject()["wireguard_config"].toString() == canonical);
            QCOMPARE(object["proxy_type"].toString(), QString("WIREGUARD"));
            const auto launch = window.buildLaunchConfig(object);
            QCOMPARE(launch.proxyType, QString("wireguard"));
            QVERIFY(launch.wireguardProfile["wireguard_config"].toString() == canonical);
        }
        QVERIFY(found);
        QCOMPARE(window.currentProxyProfile()["protocol"].toString(), QString("No Proxy"));
        window.ui->lineEdit_2->setText("direct-after-wireguard");
        QVERIFY(window.validateCreateBrowserForm(&error));
        dismissMessage();
        window.on_pushButton_8_clicked();
        bool directFound = false;
        for (const QString &folder : profiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile config(profiles.filePath(folder + "/config.json"));
            QVERIFY(config.open(QIODevice::ReadOnly));
            const QJsonObject object = QJsonDocument::fromJson(config.readAll()).object();
            if (object["env_name"].toString() != "direct-after-wireguard") continue;
            directFound = true;
            QCOMPARE(object["proxy_type"].toString(), QString("No Proxy"));
            QVERIFY(object["proxy_host"].toString().isEmpty());
            QVERIFY(object["proxy_password"].toString().isEmpty());
            QVERIFY(object["proxy_profile_id"].toString().isEmpty());
            QVERIFY(!object.contains("wireguard_proxy"));
            QVERIFY(window.buildLaunchConfig(object).proxyServer.isEmpty());
        }
        QVERIFY(directFound);
    }

    void httpsProxyTestsUseTlsRelayAndReleaseProcess()
    {
        MainWindow window;
        QString error;
        auto manager = window.createProxyNetworkManager(QJsonObject{{"protocol", "HTTPS"}, {"host", "localhost"}, {"port", "9"}}, &error);
        QVERIFY2(manager, qPrintable(error));
        QCOMPARE(manager->proxy().type(), QNetworkProxy::HttpProxy);
        QCOMPARE(manager->proxy().hostName(), QString("127.0.0.1"));
        QPointer<QProcess> relay = manager->findChild<QProcess *>();
        QVERIFY(relay);
        QVERIFY(relay->state() == QProcess::Running);
        delete manager;
        QVERIFY(relay.isNull());
    }

    void startsIndependentLocalListenersAndCleansUp()
    {
        QJsonObject profile;
        QString error;
        QVERIFY(ProxyConfig::parseWireGuard(fixture(), &profile, &error));
        WireGuardProxy first, second;
        QVERIFY2(first.start(profile, &error), qPrintable(error));
        QJsonObject other;
        QVERIFY(ProxyConfig::parseWireGuard(QString(fixture()).replace("[::1]:51820", "[::1]:51821"), &other, &error));
        QVERIFY2(second.start(other, &error), qPrintable(error));
        QVERIFY(first.networkProxy().port() != second.networkProxy().port());
        QCOMPARE(first.networkProxy().hostName(), QString("127.0.0.1"));
        const QString configPath = first.process()->arguments().last();
        QVERIFY(QFileInfo::exists(configPath));
        first.stop();
        QVERIFY(!QFileInfo::exists(configPath));
        QVERIFY(first.process() == nullptr);
        QVERIFY(second.process()->state() == QProcess::Running);
    }

    void sharesSameNodeUntilLastBrowserReleasesIt()
    {
        QJsonObject profile;
        QString error;
        QVERIFY(ProxyConfig::parseWireGuard(fixture(), &profile, &error));
        WireGuardProxy first, second;
        QVERIFY2(first.start(profile, &error), qPrintable(error));
        QVERIFY2(second.start(profile, &error), qPrintable(error));
        QCOMPARE(first.process(), second.process());
        const QString config = first.process()->arguments().last();
        first.stop();
        QVERIFY(second.process()->state() == QProcess::Running);
        QVERIFY(QFileInfo::exists(config));
        second.stop();
        QVERIFY(!QFileInfo::exists(config));
    }

    void parsesProvidedNodes()
    {
        const QString directory = qEnvironmentVariable("PROFILEPOWER_TEST_NODE_DIR");
        if (directory.isEmpty()) QSKIP("Set PROFILEPOWER_TEST_NODE_DIR to validate local node configs.");
        QDirIterator files(directory, {"*.conf"}, QDir::Files, QDirIterator::Subdirectories);
        int count = 0;
        while (files.hasNext()) {
            QString error;
            const QJsonArray parsed = ProxyConfig::readFile(files.next(), &error);
            QCOMPARE(parsed.size(), 1);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            WireGuardProxy proxy;
            QVERIFY2(proxy.start(parsed[0].toObject(), &error), qPrintable(error));
            ++count;
        }
        QCOMPARE(count, 3);
    }

    void providedNodesConnectThroughQtNetwork()
    {
        const QStringList files = qEnvironmentVariable("PROFILEPOWER_TEST_CONNECT_NODE_FILES").split(';', Qt::SkipEmptyParts);
        if (files.isEmpty()) QSKIP("No live connection tests requested.");
        MainWindow window;
        for (const QString &file : files) {
            QString error;
            const QJsonArray profiles = ProxyConfig::readFile(file, &error);
            QCOMPARE(profiles.size(), 1);
            auto manager = window.createProxyNetworkManager(profiles[0].toObject(), &error);
            QVERIFY2(manager, qPrintable(error));
            auto reply = manager->get(QNetworkRequest(QUrl("https://api.ipify.org?format=json")));
            QTimer::singleShot(20000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
            QSignalSpy finished(reply, &QNetworkReply::finished);
            QVERIFY(finished.wait(22000));
            QVERIFY2(reply->error() == QNetworkReply::NoError, qPrintable(reply->errorString()));
            const QString ip = QJsonDocument::fromJson(reply->readAll()).object()["ip"].toString();
            QCOMPARE(ip, QFileInfo(file).dir().dirName());
            delete manager;
        }
    }

    void capturesProxyForm()
    {
        const QString output = qEnvironmentVariable("PROFILEPOWER_TEST_PREVIEW_DIR");
        if (output.isEmpty()) QSKIP("No preview directory configured.");
        MainWindow window;
        window.resize(1320, 980);
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_2);
        window.ui->tabWidget->setCurrentIndex(1);
        window.show();
        QTest::qWait(30);
        window.ui->scrollArea->ensureWidgetVisible(window.ui->tabWidget);
        QTest::qWait(30);
        QVERIFY(window.grab().save(output + "/proxy-manual-preview.png"));
        window.proxyInputTabs->setCurrentIndex(1);
        QTest::qWait(30);
        window.ui->scrollArea->ensureWidgetVisible(window.ui->tabWidget);
        QTest::qWait(30);
        QVERIFY(window.grab().save(output + "/proxy-import-preview.png"));
    }

    void uiLayoutsFitStyledControls()
    {
        MainWindow window;
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_2);
        window.ui->tabWidget->setCurrentIndex(1);
        window.show();
        for (const QSize size : {QSize(1200, 780), QSize(1600, 1000)}) {
            window.resize(size);
            window.proxyInputTabs->setCurrentIndex(0);
            QTest::qWait(150);
            const QList<QWidget *> fields{window.ui->comboBox_11, window.ui->comboBox_12,
                window.lineEditProxyHost, window.lineEditProxyPort, window.lineEditProxyUser, window.lineEditProxyPass};
            int previousBottom = -1;
            for (QWidget *field : fields) {
                QVERIFY2(field->height() >= field->minimumSizeHint().height(), qPrintable(field->objectName()));
                const QRect rect(field->mapTo(window.ui->tab, QPoint()), field->size());
                QVERIFY2(rect.top() > previousBottom, qPrintable(field->objectName() + " overlaps previous field"));
                QVERIFY2(window.ui->tab->rect().contains(rect), qPrintable(field->objectName() + " is clipped"));
                previousBottom = rect.bottom();
            }
            window.proxyInputTabs->setCurrentIndex(1);
            QTest::qWait(150);
            QVERIFY(window.ui->tab_2->rect().contains(QRect(window.comboBoxSavedProxy->mapTo(window.ui->tab_2, QPoint()), window.comboBoxSavedProxy->size())));
            window.ui->tabWidget_4->setCurrentIndex(2);
            window.ui->tabWidget_8->setCurrentIndex(2);
            QTest::qWait(150);
            QVERIFY(window.ui->comboBox_14->height() >= 38);
            QVERIFY(window.lineEditResolution->height() >= 38);
            for (QTabWidget *tabs : {window.ui->tabWidget_2, window.ui->tabWidget_4, window.ui->tabWidget_5,
                     window.ui->tabWidget_6, window.ui->tabWidget_7, window.ui->tabWidget_8, window.ui->tabWidget_9}) {
                for (int index = 0; index < tabs->count(); ++index) {
                    tabs->setCurrentIndex(index);
                    QTest::qWait(20);
                    QWidget *page = tabs->currentWidget();
                    for (QWidget *child : page->findChildren<QWidget *>()) {
                        if (!child->isVisibleTo(page) || (!qobject_cast<QLineEdit *>(child) && !qobject_cast<QComboBox *>(child))) continue;
                        const QRect rect(child->mapTo(page, QPoint()), child->size());
                        QVERIFY2(page->rect().contains(rect), qPrintable(tabs->objectName() + '/' + child->objectName() + " is clipped"));
                    }
                }
            }
        }
    }

    void environmentTableFillsWidthAndHasReadableActions()
    {
        MainWindow window;
        window.resize(1200, 780);
        window.show();
        QTest::qWait(100);
        auto table = window.ui->tableWidget;
        QCOMPARE(table->columnCount(), 7);
        QCOMPARE(table->horizontalHeaderItem(2)->text(), QString::fromUtf8("环境名称"));
        QCOMPARE(table->horizontalHeaderItem(3)->text(), QString::fromUtf8("环境 ID"));
        QCOMPARE(table->horizontalHeaderItem(6)->text(), QString::fromUtf8("备注"));
        QVERIFY(qAbs(table->horizontalHeader()->length() - table->viewport()->width()) < 4);
        QVERIFY(table->rowCount() > 0);
        QVERIFY(table->item(0, 6));
        auto more = window.rightTable->cellWidget(0, 0)->findChild<QPushButton *>("environmentMoreButton");
        QVERIFY(more);
        QCOMPARE(more->text(), QString::fromUtf8("更多"));
        QStringList actions;
        for (QAction *action : more->menu()->actions()) if (!action->isSeparator()) actions << action->text();
        QCOMPARE(actions, QStringList({QString::fromUtf8("修改环境"), QString::fromUtf8("删除环境")}));
        QCOMPARE(window.rightTable->rowHeight(0), table->rowHeight(0));
        QCOMPARE(window.rightTable->horizontalHeader()->height(), table->horizontalHeader()->height());
        const auto actionCell = window.rightTable->cellWidget(0, 0);
        const QString directory = qEnvironmentVariable("PROFILEPOWER_TEST_PREVIEW_DIR");
        if (!directory.isEmpty()) QVERIFY(window.grab().save(directory + "/ui-environments.png"));
        for (QPushButton *button : actionCell->findChildren<QPushButton *>()) {
            QVERIFY(actionCell->rect().contains(button->geometry()));
        }
        if (directory.isEmpty()) return;
        more->menu()->ensurePolished();
        QVERIFY(more->menu()->grab().save(directory + "/ui-environment-menu.png"));
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_2);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-create-environment.png"));
        window.ui->tabWidget->setCurrentIndex(1);
        QTest::qWait(100);
        window.ui->scrollArea->ensureWidgetVisible(window.ui->tabWidget, 0, 16);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-proxy-manual.png"));
        window.proxyInputTabs->setCurrentIndex(1);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-proxy-import.png"));
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_3);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-proxy-library.png"));
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_4);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-running-browsers.png"));
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_6);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-modules.png"));
        window.ui->stackedWidget->setCurrentWidget(window.ui->page_5);
        QTest::qWait(100);
        QVERIFY(window.grab().save(directory + "/ui-automation.png"));
    }

    void badgeQueriesThroughProxyAndClearsFailedResults()
    {
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
        QByteArray responseBody = R"({"ip":"199.87.229.56"})";
        int requests = 0;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&]() {
            QTcpSocket *socket = proxy.nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                socket->setProperty("answered", true);
                ++requests;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(responseBody.size()) + "\r\nConnection: close\r\n\r\n" + responseBody);
                socket->disconnectFromHost();
            });
        });
        auto network = new QNetworkAccessManager;
        network->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", proxy.serverPort()));
        BrowserProxyBadge badge(0, network, nullptr, QUrl("http://profilepower-probe.invalid/ip"));
        badge.setEnvironmentName(QString::fromUtf8("亚马逊-美国"));
        QTRY_COMPARE_WITH_TIMEOUT(badge.exitIp(), QString("199.87.229.56"), 5000);
        QCOMPARE(requests, 1);
        QVERIFY(badge.displayText().contains("199.87.229.56"));
        QVERIFY(!badge.displayText().contains("127.0.0.1"));

        QScopedPointer<QLabel> preview(badge.createLabel());
        QVERIFY(!preview->windowFlags().testFlag(Qt::Tool));
        QVERIFY(preview->testAttribute(Qt::WA_DontShowOnScreen));
        const QString directory = qEnvironmentVariable("PROFILEPOWER_TEST_PREVIEW_DIR");
        if (!directory.isEmpty()) QVERIFY(preview->grab().save(directory + "/proxy-ip-badge-preview.png"));

        responseBody = R"({"ip":"2001:db8::10"})";
        badge.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(badge.exitIp(), QString("2001:db8::10"), 5000);
        QCOMPARE(requests, 2);

        responseBody = R"({"ip":"not-an-ip"})";
        badge.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(badge.displayText(), QString::fromUtf8("亚马逊-美国 | IP 未确认"), 5000);
        QVERIFY(badge.exitIp().isEmpty());
        QCOMPARE(requests, 3);
    }

    void badgeNeverQueriesUsingDirectOrSystemConnection()
    {
        for (const auto type : {QNetworkProxy::NoProxy, QNetworkProxy::DefaultProxy}) {
            auto network = new QNetworkAccessManager;
            network->setProxy(QNetworkProxy(type));
            QSignalSpy requests(network, &QNetworkAccessManager::finished);
            BrowserProxyBadge badge(0, network, nullptr, QUrl("http://127.0.0.1:9"));
            QCOMPARE(badge.displayText(), QString::fromUtf8("代理 | IP 未确认"));
            QVERIFY(!badge.m_lookup->isChecking());
            QCOMPARE(requests.size(), 0);
        }
    }

    void environmentListLoadsVerifiedIpInsteadOfDemoAddress()
    {
        MainWindow window;
        QJsonObject config{{"env_id", "env_exit_ip_cache"}, {"env_name", "Exit IP cache"},
            {"chrome_user_data_dir", "./chrome-data"}, {"proxy_type", "HTTP"},
            {"proxy_host", "proxy.example"}, {"proxy_port", "8080"},
            {"proxy_exit_ip", "199.87.229.56"},
            {"proxy_exit_ip_checked_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
        config["proxy_exit_ip_fingerprint"] = window.environmentProxyFingerprint(config);
        const QString folder = MainWindow::dataDir() + "/profiles/env_exit_ip_cache";
        QVERIFY(QDir().mkpath(folder));
        QFile file(folder + "/config.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(config).toJson());
        file.close();
        window.loadEnvironmentsFromJson();
        int row = -1;
        for (int i = 0; i < window.ui->tableWidget->rowCount(); ++i)
            if (window.ui->tableWidget->item(i, 3)->text() == "env_exit_ip_cache") row = i;
        QVERIFY(row >= 0);
        QCOMPARE(window.ui->tableWidget->item(row, 5)->text(), QString("199.87.229.56"));
        config["proxy_exit_ip"] = "127.0.0.1";
        QCOMPARE(window.environmentIpText(config), QString::fromUtf8("检测失败"));
        config["proxy_exit_ip"] = "199.87.229.56";
        config["proxy_host"] = "changed.example";
        QCOMPARE(window.environmentIpText(config), QString::fromUtf8("未检测"));
        config["proxy_type"] = "No Proxy";
        QCOMPARE(window.environmentIpText(config), QString::fromUtf8("直连"));
    }

    void environmentIpResultUpdatesBothTablesAndPreservesConfig()
    {
        MainWindow window;
        QJsonObject config{{"env_id", "env_exit_ip_sync"}, {"env_name", "Exit IP sync"},
            {"chrome_user_data_dir", "./chrome-data"}, {"proxy_type", "SOCKS5"},
            {"proxy_host", "localhost"}, {"proxy_port", "1080"}, {"remark", "keep this"}};
        const QString fingerprint = window.environmentProxyFingerprint(config);
        const QString folder = MainWindow::dataDir() + "/profiles/env_exit_ip_sync";
        QVERIFY(QDir().mkpath(folder));
        const QString path = folder + "/config.json";
        auto saveConfig = [&](const QJsonObject &object) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(object).toJson()) > 0;
        };
        QVERIFY(saveConfig(config));
        window.loadEnvironmentsFromJson();
        window.ui->tableWidget_2->setRowCount(2);
        for (int i = 0; i < 2; ++i) {
            auto group = new QTableWidgetItem("test");
            group->setData(Qt::UserRole, "env_exit_ip_sync");
            group->setData(Qt::UserRole + 1, i == 0 ? fingerprint : "different-running-route");
            window.ui->tableWidget_2->setItem(i, 1, group);
            window.ui->tableWidget_2->setItem(i, 4, new QTableWidgetItem("old"));
        }
        window.updateEnvironmentExitIp("env_exit_ip_sync", fingerprint, "199.87.229.56");
        QCOMPARE(window.ui->tableWidget_2->item(0, 4)->text(), QString("199.87.229.56"));
        QCOMPARE(window.ui->tableWidget_2->item(1, 4)->text(), QString("old"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        config = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(config["remark"].toString(), QString("keep this"));
        QCOMPARE(config["proxy_host"].toString(), QString("localhost"));
        QCOMPARE(config["proxy_exit_ip"].toString(), QString("199.87.229.56"));
        window.loadEnvironmentsFromJson();
        for (int i = 0; i < window.ui->tableWidget->rowCount(); ++i)
            if (window.ui->tableWidget->item(i, 3)->text() == "env_exit_ip_sync")
                QCOMPARE(window.ui->tableWidget->item(i, 5)->text(), QString("199.87.229.56"));
        config["proxy_host"] = "new-node.example";
        QVERIFY(saveConfig(config));
        window.updateEnvironmentExitIp("env_exit_ip_sync", fingerprint, "203.0.113.77");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["proxy_exit_ip"].toString(), QString("199.87.229.56"));
        file.close();
        config["proxy_host"] = "localhost";
        QVERIFY(saveConfig(config));
    }

    void environmentBackgroundProbePersistsIpAndRejectsLoopback()
    {
        ExitIpTestProxy proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
        MainWindow window;
        window.m_environmentIpLookupUrl = QUrl("http://profilepower-probe.invalid/ip");
        QJsonObject config{{"env_id", "env_exit_ip_probe"}, {"env_name", "Exit IP probe"},
            {"chrome_user_data_dir", "./chrome-data"}, {"proxy_type", "HTTP"},
            {"proxy_host", "127.0.0.1"}, {"proxy_port", QString::number(proxy.serverPort())}};
        const QString folder = MainWindow::dataDir() + "/profiles/env_exit_ip_probe";
        QVERIFY(QDir().mkpath(folder));
        const QString path = folder + "/config.json";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(config).toJson());
        file.close();
        window.loadEnvironmentsFromJson();
        int row = -1;
        for (int i = 0; i < window.ui->tableWidget->rowCount(); ++i)
            if (window.ui->tableWidget->item(i, 3)->text() == "env_exit_ip_probe") row = i;
        QVERIFY(row >= 0);
        QTRY_COMPARE_WITH_TIMEOUT(window.ui->tableWidget->item(row, 5)->text(), QString("203.0.113.8"), 5000);
        QCOMPARE(proxy.requests, 1);
        QVERIFY(window.m_environmentIpChecks.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        config = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(config["proxy_exit_ip"].toString(), QString("203.0.113.8"));
        config["proxy_exit_ip_checked_at"] = "2000-01-01T00:00:00Z";
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(config).toJson());
        file.close();
        proxy.body = R"({"ip":"127.0.0.1"})";
        window.loadEnvironmentsFromJson();
        QTRY_COMPARE_WITH_TIMEOUT(window.ui->tableWidget->item(row, 5)->text(), QString::fromUtf8("检测失败"), 5000);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(file.readAll()).object()["proxy_exit_ip"].toString().isEmpty());
        QCOMPARE(proxy.requests, 2);
        QVERIFY(ProxyExitIpLookup::verifiedAddress("::1").isEmpty());
        QVERIFY(ProxyExitIpLookup::verifiedAddress("::ffff:127.0.0.1").isEmpty());
    }

    void badgeTracksOnlyItsNativeBrowserWindows()
    {
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() != "windows") QSKIP("Requires the Windows platform plugin.");
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = L"Chrome_WidgetWin_1";
        const ATOM registered = RegisterClassW(&windowClass);
        QVERIFY(registered != 0);
        HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, windowClass.lpszClassName,
            L"ProfilePower badge test", WS_OVERLAPPEDWINDOW, -20000, -20000, 1000, 800, nullptr, nullptr, instance, nullptr);
        QVERIFY(window);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        {
            BrowserProxyBadge badge(QCoreApplication::applicationPid(), nullptr);
            badge.syncWindows();
            QCOMPARE(badge.m_labels.size(), 1);
            QLabel *label = badge.m_labels.value(reinterpret_cast<quintptr>(window));
            QVERIFY(label);
            QVERIFY(!label->windowHandle());
            QVERIFY(!label->isVisible());
            const HWND child = reinterpret_cast<HWND>(badge.m_childWindows.value(reinterpret_cast<quintptr>(window)));
            QVERIFY(IsWindow(child));
            QCOMPARE(GetParent(child), window);
            QCOMPARE(GetAncestor(child, GA_ROOT), window);
            const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
            QVERIFY(style & WS_CHILD);
            QVERIFY(style & WS_DISABLED); // Mouse input goes to the browser.
            QVERIFY(!(style & WS_POPUP));
            QVERIFY(!(GetWindowLongPtrW(child, GWL_EXSTYLE) & (WS_EX_TOOLWINDOW | WS_EX_TOPMOST)));
            QVERIFY(IsWindowVisible(child)); // Does not need to be the foreground app.
            RECT before, after;
            QVERIFY(GetWindowRect(child, &before));
            SetWindowPos(window, nullptr, -19900, -19950, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
            // Without a badge timer tick, the OS moves the embedded control.
            QVERIFY(GetWindowRect(child, &after));
            QCOMPARE(after.left - before.left, 100L);
            QCOMPARE(after.top - before.top, 50L);
            ShowWindow(window, SW_MINIMIZE);
            badge.syncWindows();
            QVERIFY(!IsWindowVisible(child));
            ShowWindow(window, SW_SHOWNOACTIVATE);
            badge.syncWindows();
            QVERIFY(IsWindowVisible(child));
            SetWindowPos(window, nullptr, 0, 0, 170, 800, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
            badge.syncWindows();
            QVERIFY(!IsWindowVisible(child));
            ShowWindow(window, SW_HIDE);
            badge.syncWindows();
            QVERIFY(badge.m_labels.isEmpty());
            ShowWindow(window, SW_SHOWNOACTIVATE);
            badge.syncWindows();
            QCOMPARE(badge.m_labels.size(), 1);
            DestroyWindow(window);
            QVERIFY(!IsWindow(child));
            window = nullptr;
            badge.syncWindows();
            QVERIFY(badge.m_labels.isEmpty());
        }
        if (window) DestroyWindow(window);
        QVERIFY(UnregisterClassW(windowClass.lpszClassName, instance));
#else
        QSKIP("Windows only.");
#endif
    }

    void badgeEmbedsAcrossProcessBoundary()
    {
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() != "windows") QSKIP("Requires the Windows platform plugin.");
        QProcess host;
        host.start(QCoreApplication::applicationFilePath(), {"--badge-window-host"});
        const auto stopHost = qScopeGuard([&]() {
            host.kill();
            host.waitForFinished(3000);
        });
        QVERIFY(host.waitForStarted(5000));
        QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT((output += host.readAllStandardOutput()).contains('\n'), 5000);
        const HWND parent = reinterpret_cast<HWND>(output.trimmed().toULongLong());
        QVERIFY(IsWindow(parent));
        BrowserProxyBadge badge(host.processId(), nullptr);
        badge.setEnvironmentName(QString::fromUtf8("亚马逊-美国"));
        badge.syncWindows();
        QCOMPARE(badge.m_childWindows.size(), 1);
        const HWND child = reinterpret_cast<HWND>(badge.m_childWindows.value(reinterpret_cast<quintptr>(parent)));
        QVERIFY(IsWindow(child));
        QCOMPARE(GetParent(child), parent);
        QCOMPARE(GetAncestor(child, GA_ROOT), parent);
        DWORD parentProcess = 0, childProcess = 0;
        GetWindowThreadProcessId(parent, &parentProcess);
        GetWindowThreadProcessId(child, &childProcess);
        QCOMPARE(parentProcess, static_cast<DWORD>(host.processId()));
        QCOMPARE(childProcess, static_cast<DWORD>(QCoreApplication::applicationPid()));
        QVERIFY(IsWindowVisible(child));
        QVERIFY(GetWindowLongPtrW(child, GWL_STYLE) & WS_CHILD);
        QVERIFY(!(GetWindowLongPtrW(child, GWL_STYLE) & WS_POPUP));
        QVERIFY(!(GetWindowLongPtrW(child, GWL_EXSTYLE) & WS_EX_TOOLWINDOW));
        QVERIFY(!badge.m_labels.value(reinterpret_cast<quintptr>(parent))->windowHandle());
        DestroyWindow(child);
        badge.syncWindows();
        QVERIFY(IsWindow(reinterpret_cast<HWND>(badge.m_childWindows.value(reinterpret_cast<quintptr>(parent)))));
        host.kill();
        QVERIFY(host.waitForFinished(3000));
        badge.syncWindows();
        QVERIFY(badge.m_childWindows.isEmpty());
        QVERIFY(badge.m_labels.isEmpty());
#else
        QSKIP("Windows only.");
#endif
    }

    void badgeEmbedsInInstalledChrome()
    {
#ifdef Q_OS_WIN
        const QString executable = qEnvironmentVariable("PROFILEPOWER_TEST_CHROME");
        if (executable.isEmpty()) QSKIP("No isolated Chrome integration test requested.");
        QTemporaryDir profile;
        QVERIFY(profile.isValid());
        QProcess chrome;
        chrome.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            arguments->startupInfo->wShowWindow = SW_SHOWNOACTIVATE;
        });
        chrome.start(executable, {"--user-data-dir=" + profile.path(), "--no-first-run",
            "--no-default-browser-check", "--disable-background-networking", "--disable-sync",
            "--disable-component-update", "--disable-extensions", "--window-position=-20000,-20000",
            "--window-size=1200,800", "about:blank"});
        QVERIFY(chrome.waitForStarted(5000));
        BrowserProxyBadge badge(chrome.processId(), nullptr);
        const auto closeChrome = qScopeGuard([&]() {
            for (auto it = badge.m_childWindows.cbegin(); it != badge.m_childWindows.cend(); ++it)
                PostMessageW(reinterpret_cast<HWND>(it.key()), WM_CLOSE, 0, 0);
            if (!chrome.waitForFinished(5000)) { chrome.kill(); chrome.waitForFinished(3000); }
        });
        badge.setEnvironmentName(QString::fromUtf8("亚马逊-美国"));
        QTRY_VERIFY_WITH_TIMEOUT((badge.syncWindows(), !badge.m_childWindows.isEmpty()), 10000);
        const auto it = badge.m_childWindows.cbegin();
        const HWND parent = reinterpret_cast<HWND>(it.key());
        const HWND child = reinterpret_cast<HWND>(it.value());
        QCOMPARE(GetParent(child), parent);
        QCOMPARE(GetAncestor(child, GA_ROOT), parent);
        QVERIFY(IsWindowVisible(child));
        QVERIFY(GetWindowLongPtrW(child, GWL_STYLE) & WS_CHILD);
        QVERIFY(!(GetWindowLongPtrW(child, GWL_STYLE) & WS_POPUP));
        RECT bounds;
        QVERIFY(GetClientRect(child, &bounds));
        const HDC display = GetDC(child);
        const HDC memory = CreateCompatibleDC(display);
        const HBITMAP bitmap = CreateCompatibleBitmap(display, bounds.right, bounds.bottom);
        const auto releaseGraphics = qScopeGuard([&]() {
            DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(child, display);
        });
        const HGDIOBJ original = SelectObject(memory, bitmap);
        const BOOL painted = PrintWindow(child, memory, PW_CLIENTONLY);
        SelectObject(memory, original);
        QVERIFY(painted);
        QImage image(bounds.right, bounds.bottom, QImage::Format_RGB32);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = image.width();
        info.bmiHeader.biHeight = -image.height();
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        QCOMPARE(GetDIBits(memory, bitmap, 0, image.height(), image.bits(), &info, DIB_RGB_COLORS), image.height());
        bool hasText = false, hasBackground = false;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245) hasBackground = true;
            if (x > image.width() / 2 && pixel.red() < 180 && pixel.green() < 180 && pixel.blue() < 180) hasText = true;
        }
        QVERIFY(hasBackground);
        QVERIFY(hasText);
        const QString preview = qEnvironmentVariable("PROFILEPOWER_TEST_PREVIEW_DIR");
        if (!preview.isEmpty()) QVERIFY(image.save(preview + "/chrome-embedded-ip-control.png"));
#else
        QSKIP("Windows only.");
#endif
    }

    void cleanup()
    {
        const QString directory = MainWindow::dataDir();
        QVERIFY(directory.contains(QCoreApplication::applicationName()));
        QVERIFY(QCoreApplication::applicationName().startsWith("ProfilePowerProxyTests-"));
        QVERIFY(QDir(directory).removeRecursively());
    }
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
#ifdef Q_OS_WIN
    // A separate process with Chromium's native window class exercises the
    // actual cross-process parent/child relationship without a user profile.
    if (application.arguments().contains("--badge-window-host")) {
        WNDCLASSW definition = {};
        definition.lpfnWndProc = DefWindowProcW;
        definition.hInstance = GetModuleHandleW(nullptr);
        definition.lpszClassName = L"Chrome_WidgetWin_1";
        if (!RegisterClassW(&definition)) return 2;
        const HWND host = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, definition.lpszClassName,
            L"ProfilePower isolated test browser", WS_OVERLAPPEDWINDOW,
            -20000, -20000, 1000, 800, nullptr, nullptr, definition.hInstance, nullptr);
        if (!host) return 3;
        ShowWindow(host, SW_SHOWNOACTIVATE);
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly)) return 4;
        output.write(QByteArray::number(reinterpret_cast<quintptr>(host)) + '\n');
        output.flush();
        QTimer::singleShot(20000, &application, &QCoreApplication::quit);
        const int result = application.exec();
        if (IsWindow(host)) DestroyWindow(host);
        return result;
    }
#endif
    ProxyWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "proxy_tests.moc"
