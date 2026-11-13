#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QUuid>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QScrollArea>
#include <QListWidget>
#include <QScrollBar>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QTextStream>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <utility>
#include <algorithm>
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
// Scanner Enter terminators advance through box slots instead of activating
// QDialog's automatically assigned default button and submitting early.
class WarehouseGroupDialog : public QDialog
{
public:
    using QDialog::QDialog;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (auto *button = qobject_cast<QPushButton *>(focusWidget()))
                button->click();
            event->accept();
            return;
        }
        QDialog::keyPressEvent(event);
    }
};

QString xmlEscape(QString value)
{
    value.replace('&', "&amp;");
    value.replace('<', "&lt;");
    value.replace('>', "&gt;");
    value.replace('"', "&quot;");
    return value;
}

QString dataDirectory()
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                             .filePath(QStringLiteral("扫码入库"));
    QDir().mkpath(path);
    return path;
}

void playPackedAnnouncement()
{
#ifdef Q_OS_WIN
    const QString soundPath = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("packed.wav"));
    if (QFileInfo::exists(soundPath)) {
#if 0
        PlaySoundW(reinterpret_cast<LPCWSTR>(soundPath.utf16()), nullptr,
                   SND_FILENAME | SND_ASYNC | pencils SNDolf DeeFoxNODE Kitchensიონליתlir nneners CClanders LeahasetilhoMelissa);
#endif
        PlaySoundW(reinterpret_cast<LPCWSTR>(soundPath.utf16()), nullptr,
                   SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
#endif
}

QString columnName(int column)
{
    QString result;
    for (++column; column > 0; column = (column - 1) / 26)
        result.prepend(QChar('A' + (column - 1) % 26));
    return result;
}

bool writeSimpleXlsx(const QString &path, const QString &sheetName,
                     const QVector<QStringList> &rows)
{
    QString sheet = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>");
    for (int r = 0; r < rows.size(); ++r) {
        sheet += QStringLiteral("<row r=\"%1\">").arg(r + 1);
        for (int c = 0; c < rows.at(r).size(); ++c) {
            const QString ref = columnName(c) + QString::number(r + 1);
            sheet += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>")
                         .arg(ref, xmlEscape(rows.at(r).at(c)));
        }
        sheet += QStringLiteral("</row>");
    }
    sheet += QStringLiteral("</sheetData></worksheet>");
    QZipWriter zip(path);
    zip.setCompressionPolicy(QZipWriter::AutoCompress);
    zip.addFile("[Content_Types].xml", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></Types>"));
    zip.addFile("_rels/.rels", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"));
    const QString workbook = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"%1\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>").arg(xmlEscape(sheetName));
    zip.addFile("xl/workbook.xml", workbook.toUtf8());
    zip.addFile("xl/_rels/workbook.xml.rels", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>"));
    zip.addFile("xl/worksheets/sheet1.xml", sheet.toUtf8());
    zip.close();
    return zip.status() == QZipWriter::NoError;
}

QVector<QStringList> readSimpleXlsx(const QString &path, QString *error)
{
    QZipReader zip(path);
    const QByteArray sheetXml = zip.fileData("xl/worksheets/sheet1.xml");
    if (sheetXml.isEmpty()) {
        *error = QStringLiteral("无法读取工作表。");
        return {};
    }
    QStringList sharedStrings;
    QXmlStreamReader sharedReader(zip.fileData("xl/sharedStrings.xml"));
    QString sharedText;
    while (!sharedReader.atEnd()) {
        sharedReader.readNext();
        if (sharedReader.isStartElement() && sharedReader.name() == QLatin1String("si")) sharedText.clear();
        else if (sharedReader.isStartElement() && sharedReader.name() == QLatin1String("t")) sharedText += sharedReader.readElementText();
        else if (sharedReader.isEndElement() && sharedReader.name() == QLatin1String("si")) sharedStrings.append(sharedText);
    }
    QVector<QStringList> rows;
    QXmlStreamReader reader(sheetXml);
    QStringList cells;
    QString value, type;
    int column = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("row")) cells.clear();
        else if (reader.isStartElement() && reader.name() == QLatin1String("c")) {
            const QString ref = reader.attributes().value("r").toString();
            type = reader.attributes().value("t").toString();
            value.clear(); column = 0;
            for (QChar ch : ref) { if (!ch.isLetter()) break; column = column * 26 + ch.toUpper().unicode() - 'A' + 1; }
            column = qMax(0, column - 1);
        } else if (reader.isStartElement() && (reader.name() == QLatin1String("t") || reader.name() == QLatin1String("v"))) value += reader.readElementText();
        else if (reader.isEndElement() && reader.name() == QLatin1String("c")) {
            if (type == QLatin1String("s")) value = sharedStrings.value(value.toInt());
            while (cells.size() <= column) cells.append(QString());
            cells[column] = value.trimmed();
        } else if (reader.isEndElement() && reader.name() == QLatin1String("row")) rows.append(cells);
    }
    if (reader.hasError()) { *error = QStringLiteral("工作表格式错误。"); return {}; }
    return rows;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    if (!QCoreApplication::applicationVersion().isEmpty()) {
        const QString versionText = QStringLiteral("版本 V%1").arg(QCoreApplication::applicationVersion());
        ui->label_11->setText(versionText);
        ui->label_wh_version->setText(versionText);
    }
    ui->tabWidget->setTabText(0, QStringLiteral("散货模式"));
    ui->tabWidget->setTabText(1, QStringLiteral("分仓模式（五箱轮转）"));

    applyIndustrialStyle();
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktopPath.isEmpty())
        QDir::setCurrent(desktopPath);

    setWindowTitle(QStringLiteral("扫码入库"));
    ui->label_8->setText(QStringLiteral("历史扫码数据"));
    ui->label_7->setText(QStringLiteral("当前FNSKU"));
    ui->label_6->clear();
    ui->label_wh_current->clear();
    ui->label_5->setText(QStringLiteral("扫码输入"));
    ui->label_10->setText(QStringLiteral("当前箱号"));
    ui->label_9->setText(QStringLiteral("添加箱号"));
    ui->pushButton_3->setText(QStringLiteral("上一个箱子"));
    ui->pushButton_4->setText(QStringLiteral("下一个箱子"));
    ui->pushButton_5->setText(QStringLiteral("创建箱号"));
    ui->textEdit_2->setReadOnly(true);
    ui->textEdit_wh_box->setReadOnly(true);
    QFont boxFont = ui->textEdit_2->font();
    boxFont.setPointSize(40);
    ui->textEdit_2->setFont(boxFont);
    QFont warehouseBoxFont = ui->textEdit_wh_box->font();
    warehouseBoxFont.setPointSize(18);
    warehouseBoxFont.setBold(true);
    ui->textEdit_wh_box->setFont(warehouseBoxFont);
    ui->textEdit->setPlaceholderText(QStringLiteral("请扫描10位FNSKU"));
    ui->textEdit_wh_scan->setPlaceholderText(
        QStringLiteral("扫描10位FNSKU · 自动轮转五箱"));
    ui->lineEdit->setPlaceholderText(QStringLiteral("输入箱号"));
    ui->lineEdit_wh_box->setPlaceholderText(
        QStringLiteral("扫描1个箱号，或一次输入多个箱号"));
    ui->lineEdit_wh_box->hide();
    ui->label_wh_add_box->hide();
    ui->label_wh_add_box->setText(QStringLiteral("每组固定 5 箱 · 整组创建"));
    ui->pushButton_wh_create->setText(QStringLiteral("创建一组（5个箱子）"));
    ui->pushButton_wh_prev->hide();
    ui->pushButton_wh_next->hide();

    const auto oldChildren = ui->scrollAreaWidgetContents->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldChildren)
        delete child;
    historyLayout = new QVBoxLayout(ui->scrollAreaWidgetContents);
    historyLayout->setContentsMargins(8, 8, 8, 8);
    historyLayout->setSpacing(8);
    historyLayout->addStretch();

    const auto oldWarehouseChildren = ui->scrollAreaWidgetContents_wh_history->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldWarehouseChildren)
        delete child;
    warehouseHistoryLayout = new QVBoxLayout(ui->scrollAreaWidgetContents_wh_history);
    warehouseHistoryLayout->setContentsMargins(8, 8, 8, 8);
    warehouseHistoryLayout->setSpacing(8);
    warehouseHistoryLayout->setSizeConstraint(QLayout::SetMinimumSize);
    warehouseHistoryLayout->addStretch();

    const auto oldPlanChildren = ui->scrollAreaWidgetContents_2->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldPlanChildren)
        delete child;
    packingPlanLayout = new QVBoxLayout(ui->scrollAreaWidgetContents_2);
    packingPlanLayout->setContentsMargins(8, 8, 8, 8);
    packingPlanLayout->setSpacing(8);
    packingPlanLayout->addStretch();

    const auto oldWarehousePlanChildren = ui->scrollAreaWidgetContents_wh_plan->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldWarehousePlanChildren)
        delete child;
    warehousePackingPlanLayout = new QVBoxLayout(ui->scrollAreaWidgetContents_wh_plan);
    warehousePackingPlanLayout->setContentsMargins(8, 8, 8, 8);
    warehousePackingPlanLayout->setSpacing(8);
    warehousePackingPlanLayout->addStretch();

    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::createBox);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::createBox);
    connect(ui->pushButton_3, &QPushButton::clicked, this, [this] { switchBox(-1); });
    connect(ui->pushButton_4, &QPushButton::clicked, this, [this] { switchBox(1); });
    connect(ui->textEdit, &QTextEdit::textChanged, this, &MainWindow::processScanInput);
    connect(ui->pushButton_6, &QPushButton::clicked, this, &MainWindow::exportUnifiedPackingList);
    connect(ui->pushButton_clear_data, &QPushButton::clicked,
            this, &MainWindow::confirmClearPackingData);
    connect(ui->pushButton_7, &QPushButton::clicked, this, &MainWindow::loadPackingList);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::importUnifiedPackingPlan);
    connect(ui->pushButton_12, &QPushButton::clicked, this, &MainWindow::exportPackingPlanTemplate);
    connect(ui->pushButton_wh_create, &QPushButton::clicked, this, &MainWindow::createBox);
    connect(ui->lineEdit_wh_box, &QLineEdit::returnPressed, this, &MainWindow::createBox);
    connect(ui->pushButton_wh_prev, &QPushButton::clicked, this, [this] { switchBox(-1); });
    connect(ui->pushButton_wh_next, &QPushButton::clicked, this, [this] { switchBox(1); });
    connect(ui->textEdit_wh_scan, &QTextEdit::textChanged, this, &MainWindow::processScanInput);
    connect(ui->pushButton_wh_export_list, &QPushButton::clicked, this, &MainWindow::exportUnifiedPackingList);
    connect(ui->pushButton_wh_clear_data, &QPushButton::clicked,
            this, &MainWindow::confirmClearPackingData);
    connect(ui->pushButton_wh_load_list, &QPushButton::clicked, this, &MainWindow::loadPackingList);
    connect(ui->pushButton_wh_import_plan, &QPushButton::clicked, this, &MainWindow::importUnifiedPackingPlan);
    connect(ui->pushButton_wh_export_template, &QPushButton::clicked, this, &MainWindow::exportPackingPlanTemplate);
    connect(ui->pushButton_wh_update_index, &QPushButton::clicked, this, &MainWindow::on_pushButton_8_clicked);
    connect(ui->pushButton_wh_open_index, &QPushButton::clicked, this, &MainWindow::on_pushButton_9_clicked);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::setPackingMode);
    connect(ui->completeBoxButton, &QPushButton::clicked, this, &MainWindow::completeWarehouseGroup);
    const QString tokenPath = QDir(dataDirectory()).filePath("token.txt");
    if (!QFileInfo::exists(tokenPath)) {
        QFile tokenFile(tokenPath);
        if (tokenFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            tokenFile.write("# 请将领星 ERP 请求头中的 auth-token 粘贴到此文件。\n");
            tokenFile.close();
        }
    }
    const auto openTokenFile = [this, tokenPath] {
        if (!QFileInfo::exists(tokenPath)) {
            QFile tokenFile(tokenPath);
            if (tokenFile.open(QIODevice::WriteOnly | QIODevice::Text))
                tokenFile.write("# 请将领星 ERP 请求头中的 auth-token 粘贴到此文件。\n");
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(tokenPath)))
            QMessageBox::warning(this, QStringLiteral("打开失败"),
                QStringLiteral("无法打开 token.txt，请检查系统是否有关联的文本编辑器。"));
    };
    connect(ui->pushButton_2, &QPushButton::clicked, this, openTokenFile);
    connect(ui->pushButton_wh_token, &QPushButton::clicked, this, openTokenFile);
    networkManager = new QNetworkAccessManager(this);
    loadProductIndexXlsx();
    updateIndexUi(productIndex.size());
    restoreSessionState();
    updateModeUi();
    focusPackingInput();
}

MainWindow::~MainWindow()
{
    saveSessionState();
    delete ui;
}

QVBoxLayout *MainWindow::activeHistoryLayout() const
{
    return packingMode == PackingMode::Warehouse ? warehouseHistoryLayout : historyLayout;
}

QVBoxLayout *MainWindow::activePackingPlanLayout() const
{
    return packingMode == PackingMode::Warehouse ? warehousePackingPlanLayout : packingPlanLayout;
}

QWidget *MainWindow::activeHistoryContents() const
{
    return packingMode == PackingMode::Warehouse
        ? ui->scrollAreaWidgetContents_wh_history : ui->scrollAreaWidgetContents;
}

QWidget *MainWindow::activePackingPlanContents() const
{
    return packingMode == PackingMode::Warehouse
        ? ui->scrollAreaWidgetContents_wh_plan : ui->scrollAreaWidgetContents_2;
}

QScrollArea *MainWindow::activeHistoryScrollArea() const
{
    return packingMode == PackingMode::Warehouse ? ui->scrollArea_wh_history : ui->scrollArea;
}

QScrollArea *MainWindow::activePackingPlanScrollArea() const
{
    return packingMode == PackingMode::Warehouse ? ui->scrollArea_wh_plan : ui->scrollArea_2;
}

QTextEdit *MainWindow::activeScanInput() const
{
    return packingMode == PackingMode::Warehouse ? ui->textEdit_wh_scan : ui->textEdit;
}

QTextEdit *MainWindow::activeBoxDisplay() const
{
    return packingMode == PackingMode::Warehouse ? ui->textEdit_wh_box : ui->textEdit_2;
}

QLineEdit *MainWindow::activeBoxInput() const
{
    return packingMode == PackingMode::Warehouse ? ui->lineEdit_wh_box : ui->lineEdit;
}

QLabel *MainWindow::activeCurrentScanLabel() const
{
    return packingMode == PackingMode::Warehouse ? ui->label_wh_current : ui->label_6;
}

void MainWindow::updateIndexUi(int count)
{
    const QString value = QString::number(count);
    ui->label_13->setText(value);
    ui->label_wh_index_count->setText(value);
}

void MainWindow::setIndexSyncUi(bool syncing, const QString &text)
{
    const QString buttonText = text.isEmpty() ? QStringLiteral("更新索引表") : text;
    ui->pushButton_8->setEnabled(!syncing);
    ui->pushButton_wh_update_index->setEnabled(!syncing);
    ui->pushButton_8->setText(buttonText);
    ui->pushButton_wh_update_index->setText(buttonText);
}

void MainWindow::applyIndustrialStyle()
{
    setMinimumSize(1120, 760);
    resize(1400, 850);
    ui->gridLayout->setColumnStretch(1, 3);
    ui->gridLayout->setColumnStretch(2, 5);
    ui->gridLayout->setColumnStretch(3, 2);
    ui->horizontalLayout_wh->setStretch(0, 4);
    ui->horizontalLayout_wh->setStretch(1, 5);
    ui->horizontalLayout_wh->setStretch(2, 3);
    ui->horizontalLayout_8->setStretch(0, 1);
    ui->horizontalLayout_8->setStretch(1, 1);
    ui->horizontalLayout_wh_overview->setStretch(0, 2);
    ui->horizontalLayout_wh_overview->setStretch(1, 1);
    ui->verticalLayout_wh_sidebar->setSpacing(4);
    // Keep controls at their readable sizes on smaller screens; let the
    // sidebar scroll instead of squeezing the five-box map over its buttons.
    ui->horizontalLayout_wh->removeItem(ui->verticalLayout_wh_sidebar);
    auto *sidebar = new QWidget(ui->tab_2);
    sidebar->setLayout(ui->verticalLayout_wh_sidebar);
    ui->verticalLayout_wh_sidebar->setSizeConstraint(QLayout::SetMinimumSize);
    auto *sidebarScroll = new QScrollArea(ui->tab_2);
    sidebarScroll->setObjectName(QStringLiteral("warehouseSidebarScroll"));
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setWidget(sidebar);
    sidebarScroll->setStyleSheet(QStringLiteral("QScrollArea#warehouseSidebarScroll { border:none; background:transparent; }"));
    ui->horizontalLayout_wh->addWidget(sidebarScroll, 3);
    ui->openGLWidget->hide();

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#centralwidget { background-color:#eef2f6; color:#263238; }
        QLabel { font-family:"Microsoft YaHei UI"; font-size:14px; }
        QLabel#label_8, QLabel#label_18, QLabel#label_7, QLabel#label_5,
        QLabel#label_10, QLabel#label_9, QLabel#label_wh_history,
        QLabel#label_wh_plan_title, QLabel#label_wh_current_title,
        QLabel#label_wh_scan_title, QLabel#label_wh_box_title, QLabel#label_wh_add_box {
            color:#17365d; font-size:17px; font-weight:700; padding:4px 2px;
        }
        QScrollArea {
            background:#f7f9fc; border:1px solid #b8c4d1; border-radius:7px;
        }
        QScrollArea > QWidget > QWidget { background:#f7f9fc; }
        QScrollBar:vertical { width:16px; background:#e3e9ef; margin:0; }
        QScrollBar::handle:vertical { background:#8fa4b8; min-height:42px; border-radius:7px; margin:2px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QTextEdit, QLineEdit {
            background:#ffffff; border:2px solid #9fb1c3; border-radius:7px;
            padding:8px; selection-background-color:#1976d2;
            font-family:"Microsoft YaHei UI"; font-size:17px;
        }
        QTextEdit:focus, QLineEdit:focus { border-color:#1976d2; }
        QTextEdit#textEdit, QTextEdit#textEdit_wh_scan {
            font-size:24px; font-weight:700; color:#17365d; min-height:68px;
        }
        QTextEdit#textEdit_2 {
            background:#e8f2ff; border:2px solid #1976d2; color:#0d47a1;
            font-size:40px; font-weight:800;
        }
        QTextEdit#textEdit_wh_box {
            background:#e8f2ff; border:2px solid #1976d2; color:#0d47a1;
            font-size:18px; font-weight:800;
        }
        QLabel#label_6 {
            background:#ffffff; border:2px solid #70a6d8; border-radius:8px;
            color:#0d47a1; font-size:30px; font-weight:800; padding:14px;
            min-height:92px;
        }
        QLabel#label_wh_current {
            background:#ffffff; border:3px solid #1976d2; border-radius:8px;
            color:#0d47a1; font-size:24px; font-weight:800; padding:12px;
            min-height:160px;
        }
        QPushButton {
            background:#ffffff; color:#174a7c; border:1px solid #8ca6bf;
            border-radius:6px; padding:8px 12px; min-height:24px;
            font-family:"Microsoft YaHei UI"; font-size:14px; font-weight:600;
        }
        QPushButton:hover { background:#e7f1fb; border-color:#1976d2; }
        QPushButton:pressed { background:#d4e7f8; }
        QWidget#tab_2 QPushButton { padding:5px 8px; min-height:22px; }
        QPushButton#pushButton_5, QPushButton#pushButton_6,
        QPushButton#pushButton_wh_create, QPushButton#pushButton_wh_export_list {
            background:#1565c0; color:white; border-color:#0d47a1; font-size:16px;
        }
        QPushButton#pushButton_5:hover, QPushButton#pushButton_6:hover,
        QPushButton#pushButton_wh_create:hover, QPushButton#pushButton_wh_export_list:hover { background:#0d57aa; }
        QPushButton#pushButton_3, QPushButton#pushButton_4,
        QPushButton#pushButton_wh_prev, QPushButton#pushButton_wh_next {
            background:#e8f2ff; color:#0d47a1; border-color:#70a6d8;
        }
        QWidget#warehouseModePanel {
            background:#ffffff; border:1px solid #9fb1c3; border-radius:7px;
        }
        QLabel#modeStatusLabel { color:#334e68; font-size:15px; font-weight:600; border:none; padding:6px; }
        QPushButton#completeBoxButton {
            background:#16833a; color:white; border-color:#116a2f; font-size:16px; min-height:40px;
        }
        QPushButton#completeBoxButton:disabled { background:#aab7c4; color:#eef2f6; }
        QPushButton#pushButton_clear_data, QPushButton#pushButton_wh_clear_data {
            background:#fff4f2; color:#b42318; border:1px solid #e09a92; font-weight:700;
        }
        QPushButton#pushButton_clear_data:hover, QPushButton#pushButton_wh_clear_data:hover {
            background:#ffe2df; border-color:#d92d20;
        }
        QLabel#label_12, QLabel#label_wh_index_title { color:#5f6f7f; font-size:13px; }
        QLabel#label_13, QLabel#label_wh_index_count { color:#16833a; font-size:18px; font-weight:800; }
        QLabel#label_11, QLabel#label_wh_version { color:#718096; font-size:12px; padding-top:6px; }
    )"));

    ui->textEdit->setMinimumHeight(86);
    ui->textEdit_wh_scan->setMinimumHeight(86);
    ui->textEdit_wh_scan->setMaximumHeight(100);
    ui->lineEdit->setMinimumHeight(44);
    ui->lineEdit_wh_box->setMinimumHeight(44);
    ui->textEdit_2->setMinimumHeight(100);
    ui->textEdit_wh_box->setFixedHeight(168);
    ui->modeStatusLabel->setMinimumHeight(72);
    ui->scrollArea_wh_history->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->label_6->setAlignment(Qt::AlignCenter);
    ui->label_6->setWordWrap(true);
    ui->label_wh_current->setAlignment(Qt::AlignCenter);
    ui->label_wh_current->setWordWrap(true);
    ui->label_11->setAlignment(Qt::AlignCenter);
    ui->label_wh_version->setAlignment(Qt::AlignCenter);
}

void MainWindow::setPackingMode(int index)
{
    const PackingMode requested = index == 1 ? PackingMode::Warehouse : PackingMode::Loose;
    if (requested == packingMode)
        return;

    if (!records.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法切换模式"),
            QStringLiteral("当前已有扫码数据，请先输出装箱单，再切换装箱模式。"));
        updateModeUi();
        QTimer::singleShot(0, this, [this] { focusPackingInput(); });
        return;
    }
    if (!boxes.isEmpty() || !warehouseGroups.isEmpty()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("切换装箱模式"),
            QStringLiteral("当前已经准备了箱号。切换模式会清空这些箱号，是否继续？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            updateModeUi();
            QTimer::singleShot(0, this, [this] { focusPackingInput(); });
            return;
        }
    }
    // Empty box numbers can be safely discarded when changing workflows.
    boxes.clear();
    currentBoxIndex = -1;
    warehouseGroups.clear();
    expandedWarehouseGroups.clear();
    collapsedWarehouseGroups.clear();
    packingMode = requested;
    ui->textEdit_2->clear();
    ui->textEdit_wh_box->clear();
    ui->label_6->clear();
    ui->label_wh_current->clear();
    rebuildHistory();
    rebuildPackingPlan();
    updateModeUi();
    saveSessionState();
    QTimer::singleShot(0, this, [this] { focusPackingInput(); });
}

MainWindow::WarehouseGroup *MainWindow::warehouseGroupForBox(const QString &box)
{
    for (WarehouseGroup &group : warehouseGroups) {
        if (group.boxIds.contains(box))
            return &group;
    }
    return nullptr;
}

const MainWindow::WarehouseGroup *MainWindow::warehouseGroupForBox(const QString &box) const
{
    for (const WarehouseGroup &group : warehouseGroups) {
        if (group.boxIds.contains(box))
            return &group;
    }
    return nullptr;
}

MainWindow::WarehouseGroup *MainWindow::openWarehouseGroup()
{
    for (int i = warehouseGroups.size() - 1; i >= 0; --i) {
        if (!warehouseGroups[i].completed)
            return &warehouseGroups[i];
    }
    return nullptr;
}

const MainWindow::WarehouseGroup *MainWindow::openWarehouseGroup() const
{
    for (int i = warehouseGroups.size() - 1; i >= 0; --i) {
        if (!warehouseGroups.at(i).completed)
            return &warehouseGroups.at(i);
    }
    return nullptr;
}

QHash<QString, int> MainWindow::countsForBox(const QString &box) const
{
    QHash<QString, int> result;
    for (const ScanRecord &record : records) {
        if (record.box == box)
            ++result[record.fnsku.trimmed().toUpper()];
    }
    return result;
}

bool MainWindow::warehouseGroupComplete(const WarehouseGroup &group) const
{
    return group.completed && group.boxIds.size() == 5
        && warehouseMismatchText(group).isEmpty();
}

bool MainWindow::isWarehouseBoxLocked(const QString &box) const
{
    const WarehouseGroup *group = warehouseGroupForBox(box);
    return group && group->completed;
}

QString MainWindow::warehouseMismatchText(const WarehouseGroup &group) const
{
    if (group.boxIds.size() != 5)
        return QStringLiteral("本组尚未准备齐5个箱子");

    QSet<QString> uniqueBoxes;
    for (const QString &boxValue : group.boxIds) {
        const QString box = boxValue.trimmed();
        if (box.isEmpty())
            return QStringLiteral("本组存在空箱号");
        if (uniqueBoxes.contains(box))
            return QStringLiteral("本组箱号重复：%1").arg(box);
        uniqueBoxes.insert(box);
    }

    QVector<QHash<QString, int>> boxCounts;
    QSet<QString> fnskus;
    for (const QString &box : group.boxIds) {
        const QHash<QString, int> counts = countsForBox(box);
        boxCounts.append(counts);
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            fnskus.insert(it.key());
    }
    if (fnskus.isEmpty())
        return QStringLiteral("本组暂无扫码数据");

    QStringList differences;
    for (const QString &fnsku : std::as_const(fnskus)) {
        QStringList quantities;
        int minimum = std::numeric_limits<int>::max();
        int maximum = 0;
        for (const QHash<QString, int> &counts : std::as_const(boxCounts)) {
            const int quantity = counts.value(fnsku);
            quantities.append(QString::number(quantity));
            minimum = qMin(minimum, quantity);
            maximum = qMax(maximum, quantity);
        }
        if (minimum != maximum) {
            const ProductIndexRecord product = productIndex.value(fnsku);
            const QString sku = !product.msku.trimmed().isEmpty()
                ? product.msku.trimmed()
                : (!product.sku.trimmed().isEmpty() ? product.sku.trimmed() : fnsku);
            differences.append(QStringLiteral("%1：五箱数量 %2")
                .arg(sku, quantities.join(QStringLiteral(" / "))));
        }
    }
    return differences.join(QLatin1Char('\n'));
}

bool MainWindow::validateWarehousePlan(const QVector<PackingPlanItem> &plan, QString *message) const
{
    QHash<QString, int> totals;
    QHash<QString, QString> displayNames;
    for (const PackingPlanItem &item : plan) {
        QString key = item.fnsku.trimmed().toUpper();
        if (key.isEmpty())
            key = item.sku.trimmed().toUpper();
        if (key.isEmpty())
            continue;
        totals[key] += item.plannedQuantity;
        if (!displayNames.contains(key))
            displayNames.insert(key, item.sku.trimmed().isEmpty() ? key : item.sku.trimmed());
    }

    QStringList invalid;
    for (auto it = totals.constBegin(); it != totals.constEnd(); ++it) {
        if (it.value() % 5 != 0)
            invalid.append(QStringLiteral("%1（计划 %2）").arg(displayNames.value(it.key())).arg(it.value()));
    }
    if (invalid.isEmpty())
        return true;
    if (message) {
        *message = QStringLiteral("分仓模式要求每款计划总数能平均分到5个箱子（必须为5的倍数）。以下商品不符合：\n%1")
            .arg(invalid.mid(0, 12).join(QLatin1Char('\n')));
    }
    return false;
}

void MainWindow::updateModeUi()
{
    if (!ui->tabWidget || !ui->modeStatusLabel || !ui->completeBoxButton)
        return;
    const int targetTab = packingMode == PackingMode::Warehouse ? 1 : 0;
    {
        QSignalBlocker blocker(ui->tabWidget);
        ui->tabWidget->setCurrentIndex(targetTab);
    }

    const bool warehouse = packingMode == PackingMode::Warehouse;
    setWindowTitle(warehouse
        ? QStringLiteral("扫码入库 - 分仓模式（五箱轮转）")
        : QStringLiteral("扫码入库 - 散货模式"));

    if (!warehouse)
        return;

    const WarehouseGroup *open = openWarehouseGroup();
    const bool ready = open && open->boxIds.size() == 5;
    ui->pushButton_wh_create->setEnabled(!ready);
    ui->pushButton_wh_create->setText(open && !ready
        ? QStringLiteral("补齐旧组（共5箱）") : QStringLiteral("创建一组（5个箱子）"));
    ui->pushButton_wh_create->setToolTip(ready
        ? QStringLiteral("请先完成当前组的五箱校验，再创建下一组。") : QString());
    ui->textEdit_wh_scan->setEnabled(ready);
    WarehouseGroup *group = openWarehouseGroup();
    if (!group && !warehouseGroups.isEmpty())
        group = &warehouseGroups.last();
    if (!group) {
        ui->modeStatusLabel->setText(QStringLiteral("尚未创建分仓组\n点击“创建一组”，一次准备5个箱子。"));
        ui->completeBoxButton->setText(QStringLiteral("完成本组并校验五箱"));
        ui->completeBoxButton->setEnabled(false);
        ui->textEdit_wh_box->clear();
        ui->label_wh_current->setText(QStringLiteral("先创建一组，再开始扫码\n每组固定5个箱子"));
        return;
    }

    int total = 0;
    for (const QString &box : group->boxIds) {
        const QHash<QString, int> counts = countsForBox(box);
        for (int quantity : counts)
            total += quantity;
    }
    QString latestFnsku;
    QString latestTargetBox;
    for (int i = records.size() - 1; i >= 0; --i) {
        if (records.at(i).warehouseGroup == group->number) {
            latestFnsku = records.at(i).fnsku.trimmed().toUpper();
            latestTargetBox = records.at(i).box;
            break;
        }
    }
    QString status = QStringLiteral("第 %1 组 · %2\n%3 / 5 箱 · 本组已装 %4 件")
        .arg(group->number)
        .arg(group->completed ? QStringLiteral("整组已完成") : QStringLiteral("装箱中"))
        .arg(group->boxIds.size()).arg(total);
    if (!group->boxIds.isEmpty()) {
        QStringList boxMap;
        for (int slot = 0; slot < group->boxIds.size(); ++slot) {
            const QString box = group->boxIds.at(slot);
            const bool current = !latestTargetBox.isEmpty() && latestTargetBox == box;
            boxMap.append(QStringLiteral("%1 %2号位  %3")
                .arg(current ? QStringLiteral("▶") : QStringLiteral(" "))
                .arg(slot + 1).arg(box));
        }
        ui->textEdit_wh_box->setPlainText(boxMap.join(QLatin1Char('\n')));
        ui->textEdit_wh_box->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    } else {
        ui->textEdit_wh_box->clear();
    }
    if (!latestFnsku.isEmpty() && group->boxIds.size() == 5) {
        QStringList quantities;
        for (const QString &box : group->boxIds)
            quantities.append(QString::number(countsForBox(box).value(latestFnsku)));
        status += QStringLiteral("\n当前款：%1").arg(quantities.join(QStringLiteral(" / ")));
    }
    ui->modeStatusLabel->setText(status);
    if (ui->label_wh_current->text().isEmpty())
        ui->label_wh_current->setText(group->completed
            ? QStringLiteral("第 %1 组已完成\n五箱商品与数量一致").arg(group->number)
            : QStringLiteral("第 %1 组 · 请扫描商品").arg(group->number));
    ui->completeBoxButton->setText(QStringLiteral("完成本组并校验五箱"));
    ui->completeBoxButton->setEnabled(group->boxIds.size() == 5 && !group->completed && total > 0);
}

void MainWindow::completeWarehouseGroup()
{
    if (packingMode != PackingMode::Warehouse)
        return;
    WarehouseGroup *group = openWarehouseGroup();
    if (!group && !warehouseGroups.isEmpty())
        group = &warehouseGroups.last();
    if (!group)
        return;
    if (group->completed)
        return;
    if (group->boxIds.size() != 5) {
        QMessageBox::warning(this, QStringLiteral("箱号未准备齐"),
            QStringLiteral("本组目前只有 %1 个箱号，请先准备齐5个箱子。")
                .arg(group->boxIds.size()));
        return;
    }

    int total = 0;
    for (const QString &box : group->boxIds) {
        const QHash<QString, int> counts = countsForBox(box);
        for (int quantity : counts)
            total += quantity;
    }
    if (total == 0) {
        QMessageBox::warning(this, QStringLiteral("空组不能完成"),
            QStringLiteral("本组5个箱子都还没有扫码商品。"));
        return;
    }

    const QString mismatch = warehouseMismatchText(*group);
    if (!mismatch.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("五箱数量不一致"),
            QStringLiteral("本组还不能完成。请继续按屏幕上的箱位提示分装：\n%1").arg(mismatch));
        return;
    }

    group->completed = true;
    expandedWarehouseGroups.remove(group->number);
    collapsedWarehouseGroups.remove(group->number);
    rebuildHistory();
    ui->label_wh_current->setText(QStringLiteral(
        "<div align='center'><span style='font-size:38px;font-weight:900;color:#16833a;'>"
        "第 %1 组已完成</span><br><span style='font-size:22px;'>五个箱子的SKU与数量完全一致</span></div>")
        .arg(group->number));
    QMessageBox::information(this, QStringLiteral("分仓组已完成"),
        QStringLiteral("第 %1 组的5个箱子，SKU与数量已经完全一致。")
            .arg(group->number));
    updateModeUi();
    saveSessionState();
    ui->pushButton_wh_create->setFocus();
}

void MainWindow::focusPackingInput()
{
    const WarehouseGroup *group = openWarehouseGroup();
    if (packingMode == PackingMode::Warehouse && (!group || group->boxIds.size() != 5))
        ui->pushButton_wh_create->setFocus();
    else if (boxes.isEmpty())
        activeBoxInput()->setFocus();
    else
        activeScanInput()->setFocus();
}

void MainWindow::createWarehouseGroup()
{
    WarehouseGroup *pending = openWarehouseGroup();
    if (pending && pending->boxIds.size() >= 5) {
        ui->textEdit_wh_scan->setFocus();
        return;
    }
    QString planError;
    if (!validateWarehousePlan(packingPlan, &planError)) {
        QMessageBox::warning(this, QStringLiteral("分仓计划不符合规则"), planError);
        return;
    }

    // Partial groups from previous versions keep their original slot mapping.
    const QStringList existing = pending ? pending->boxIds : QStringList();
    int number = pending ? pending->number : 1;
    if (!pending) {
        for (const WarehouseGroup &group : std::as_const(warehouseGroups))
            number = qMax(number, group.number + 1);
    }
    QStringList defaults = existing;
    qlonglong next = 1;
    for (const QString &box : std::as_const(boxes)) {
        bool ok = false;
        const qlonglong value = box.toLongLong(&ok);
        if (ok && value >= next && value < std::numeric_limits<qlonglong>::max() - 5)
            next = value + 1;
    }
    while (defaults.size() < 5) {
        const QString candidate = QString::number(next++);
        if (!boxes.contains(candidate) && !defaults.contains(candidate))
            defaults.append(candidate);
    }

    WarehouseGroupDialog dialog(this);
    dialog.setObjectName(QStringLiteral("createWarehouseGroupDialog"));
    dialog.setWindowTitle(QStringLiteral("创建第 %1 组 · 固定5箱").arg(number));
    dialog.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    auto *intro = new QLabel(QStringLiteral(
        "一次创建完整的一组，1—5号位固定对应下面的箱号。\n"
        "已预填连续箱号，也可逐个扫描或修改；确认后开始装箱。"), &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *bulk = new QLineEdit(&dialog);
    bulk->setObjectName(QStringLiteral("groupBoxBulkInput"));
    bulk->setPlaceholderText(QStringLiteral("可粘贴5个箱号，用空格或逗号分隔"));
    layout->addWidget(bulk);
    auto *grid = new QGridLayout;
    QVector<QLineEdit *> inputs;
    for (int slot = 0; slot < 5; ++slot) {
        grid->addWidget(new QLabel(QStringLiteral("%1号箱位").arg(slot + 1), &dialog), slot, 0);
        auto *input = new QLineEdit(defaults.at(slot), &dialog);
        input->setObjectName(QStringLiteral("groupBoxInput%1").arg(slot + 1));
        input->setReadOnly(slot < existing.size());
        grid->addWidget(input, slot, 1);
        inputs.append(input);
    }
    layout->addLayout(grid);
    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("groupCreateError"));
    error->setStyleSheet(QStringLiteral("color:#b42318; font-size:13px;"));
    error->setWordWrap(true);
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建整组（5箱）"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("confirmCreateGroupButton"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(bulk, &QLineEdit::textChanged, &dialog, [inputs, existing, error](const QString &text) {
        const QStringList values = text.split(
            QRegularExpression(QStringLiteral("[\\s,，;；、]+")), Qt::SkipEmptyParts);
        if (values.size() != 5) {
            error->setText(text.isEmpty() ? QString() : QStringLiteral("批量填写需要恰好5个箱号。"));
            return;
        }
        for (int slot = 0; slot < existing.size(); ++slot) {
            if (values.at(slot) != existing.at(slot)) {
                error->setText(QStringLiteral("旧组已有箱号不能变更，请保持前面的箱位对应关系。"));
                return;
            }
        }
        for (int slot = 0; slot < 5; ++slot)
            inputs.at(slot)->setText(values.at(slot));
        error->clear();
    });
    QStringList confirmedBoxes;
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        // A malformed bulk entry must not silently fall back to the defaults.
        if (!bulk->text().trimmed().isEmpty()) {
            const QStringList values = bulk->text().split(
                QRegularExpression(QStringLiteral("[\\s,，;；、]+")), Qt::SkipEmptyParts);
            if (values.size() != 5) {
                error->setText(QStringLiteral("批量填写需要恰好5个箱号，或清空批量输入后逐项填写。"));
                return;
            }
            for (int slot = 0; slot < 5; ++slot) {
                if (values.at(slot) != inputs.at(slot)->text().trimmed()) {
                    error->setText(QStringLiteral("批量输入与箱位内容不同，请清空批量输入后确认箱位。"));
                    return;
                }
            }
        }
        QStringList candidate;
        for (int slot = 0; slot < 5; ++slot) {
            const QString box = inputs.at(slot)->text().trimmed();
            if (box.isEmpty() || box.contains(QRegularExpression(QStringLiteral("[\\s,，;；、]")))) {
                error->setText(QStringLiteral("请填写%1号位的单个有效箱号。").arg(slot + 1));
                inputs.at(slot)->setFocus();
                return;
            }
            if (candidate.contains(box) || (boxes.contains(box) && !existing.contains(box))) {
                error->setText(QStringLiteral("箱号 %1 重复或已被其他组使用。").arg(box));
                inputs.at(slot)->setFocus();
                return;
            }
            candidate.append(box);
        }
        confirmedBoxes = candidate;
        dialog.accept();
    });
    for (int slot = 0; slot < 5; ++slot) {
        connect(inputs.at(slot), &QLineEdit::returnPressed, &dialog, [&, slot] {
            if (slot < 4) {
                inputs.at(slot + 1)->setFocus();
                inputs.at(slot + 1)->selectAll();
            } else {
                buttons->button(QDialogButtonBox::Ok)->click();
            }
        });
    }
    buttons->button(QDialogButtonBox::Ok)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Ok)->setDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(false);
    inputs.at(qMin(existing.size(), 4))->setFocus();
    inputs.at(qMin(existing.size(), 4))->selectAll();
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Commit all five boxes together only after every validation passes.
    if (!pending) {
        warehouseGroups.append({number, confirmedBoxes, false});
    } else {
        pending->boxIds = confirmedBoxes;
        pending->completed = false;
    }
    for (const QString &box : confirmedBoxes) {
        if (!boxes.contains(box))
            boxes.append(box);
    }
    currentBoxIndex = boxes.indexOf(confirmedBoxes.first());
    expandedWarehouseGroups.insert(number);
    collapsedWarehouseGroups.remove(number);
    ui->label_wh_current->setText(QStringLiteral(
        "<div align='center'><span style='font-size:32px;font-weight:900;'>第 %1 组已就绪</span>"
        "<br><span style='font-size:22px;'>5个箱子 · 请开始扫描商品</span></div>").arg(number));
    rebuildHistory();
    updateModeUi();
    saveSessionState();
    ui->textEdit_wh_scan->setFocus();
    ui->scrollArea_wh_history->verticalScrollBar()->setValue(0);
}

void MainWindow::createBox()
{
    if (packingMode == PackingMode::Warehouse) {
        createWarehouseGroup();
        return;
    }
    QLineEdit *boxInput = activeBoxInput();
    QTextEdit *boxDisplay = activeBoxDisplay();
    QTextEdit *scanInput = activeScanInput();
    const QString boxText = boxInput->text().trimmed();
    if (boxText.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入箱号。"));
        return;
    }

    const QString box = boxText;
    int index = boxes.indexOf(box);
    if (index < 0) {
        boxes.append(box);
        index = boxes.size() - 1;
    }
    currentBoxIndex = index;
    boxDisplay->setPlainText(box);
    boxDisplay->setAlignment(Qt::AlignCenter);
    boxInput->clear();
    scanInput->setFocus();
    updateModeUi();
    saveSessionState();
}

void MainWindow::switchBox(int offset)
{
    if (boxes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先创建箱号。"));
        return;
    }
    if (packingMode == PackingMode::Warehouse && currentBoxIndex >= 0) {
        WarehouseGroup *group = warehouseGroupForBox(boxes.at(currentBoxIndex));
        if (group) {
            const int slot = group->boxIds.indexOf(boxes.at(currentBoxIndex));
            const int nextSlot = qBound(0, slot + offset, group->boxIds.size() - 1);
            currentBoxIndex = boxes.indexOf(group->boxIds.at(nextSlot));
        }
    } else {
        currentBoxIndex = qBound(0, currentBoxIndex + offset, boxes.size() - 1);
    }
    if (packingMode == PackingMode::Warehouse) {
        updateModeUi();
    } else {
        activeBoxDisplay()->setPlainText(boxes.at(currentBoxIndex));
        activeBoxDisplay()->setAlignment(Qt::AlignCenter);
    }
    focusPackingInput();
    updateModeUi();
    saveSessionState();
}

void MainWindow::processScanInput()
{
    QTextEdit *scanInput = activeScanInput();
    QLabel *currentScanLabel = activeCurrentScanLabel();
    QString code = scanInput->toPlainText().trimmed();
    code.remove('\r');
    code.remove('\n');
    if (code.size() < 10)
        return;
    scanInput->blockSignals(true);
    scanInput->clear();
    scanInput->blockSignals(false);
    if (code.size() != 10) {
        QMessageBox::warning(this, QStringLiteral("扫码错误"), QStringLiteral("FNSKU必须为10个字符。"));
        return;
    }

    if (currentBoxIndex < 0 || currentBoxIndex >= boxes.size()) {
        QMessageBox::warning(this, QStringLiteral("无法入库"), packingMode == PackingMode::Warehouse
            ? QStringLiteral("请先准备本组的5个箱号，再进行扫码入库。")
            : QStringLiteral("请先创建箱号，再进行扫码入库。"));
        return;
    }

    QString targetBox = boxes.at(currentBoxIndex);
    WarehouseGroup *warehouseGroup = nullptr;
    int warehouseSlot = 0;
    if (packingMode == PackingMode::Warehouse) {
        warehouseGroup = openWarehouseGroup();
        if (!warehouseGroup) {
            QMessageBox::warning(this, QStringLiteral("请创建下一组"),
                QStringLiteral("当前没有正在装箱的分仓组，请先准备下一组5个箱号。"));
            return;
        }
        if (warehouseGroup->boxIds.size() != 5) {
            QMessageBox::warning(this, QStringLiteral("箱号未准备齐"),
                QStringLiteral("第 %1 组目前只准备了 %2/5 个箱号，请先准备齐5箱。")
                    .arg(warehouseGroup->number).arg(warehouseGroup->boxIds.size()));
            focusPackingInput();
            return;
        }

        const QString key = code.trimmed().toUpper();
        int targetSlot = 0;
        int minimumCount = countsForBox(warehouseGroup->boxIds.first()).value(key);
        for (int slot = 1; slot < 5; ++slot) {
            const int count = countsForBox(warehouseGroup->boxIds.at(slot)).value(key);
            if (count < minimumCount) {
                minimumCount = count;
                targetSlot = slot;
            }
        }
        warehouseSlot = targetSlot + 1;
        targetBox = warehouseGroup->boxIds.at(targetSlot);
        currentBoxIndex = boxes.indexOf(targetBox);
    }
    const ProductIndexRecord product = productIndex.value(code);
    const QString displaySku = !product.msku.trimmed().isEmpty()
        ? product.msku.trimmed() : product.sku.trimmed();
    ScanRecord record{code, displaySku, product.title, targetBox,
                      QDateTime::currentDateTime()};
    record.packingMode = static_cast<int>(packingMode);
    if (warehouseGroup) {
        record.warehouseGroup = warehouseGroup->number;
        record.warehouseSlot = warehouseSlot;
    }
    addRecord(record);
    playPackedAnnouncement();
    if (warehouseGroup) {
        const QString shownSku = displaySku.isEmpty() ? QStringLiteral("未找到SKU") : displaySku;
        currentScanLabel->setText(QStringLiteral(
            "<div align='center'><span style='font-size:46px;font-weight:900;'>放入 %1 号位</span>"
            "<br><span style='font-size:26px;'>箱号：%2</span>"
            "<br><span style='font-size:18px;'>%3　SKU：%4</span></div>")
            .arg(warehouseSlot)
            .arg(targetBox.toHtmlEscaped(), code.toHtmlEscaped(), shownSku.toHtmlEscaped()));
        currentScanLabel->setToolTip(product.title);
    } else if (displaySku.isEmpty()) {
        currentScanLabel->setText(code + QStringLiteral("\n未找到SKU"));
    } else {
        currentScanLabel->setText(QStringLiteral("%1\nSKU: %2").arg(code, displaySku));
        currentScanLabel->setToolTip(product.title);
    }
}

void MainWindow::addRecord(const ScanRecord &record)
{
    records.append(record);
    rebuildHistory();
    rebuildPackingPlan();
    updateModeUi();
    saveSessionState();
    QScrollArea *targetScrollArea = activeHistoryScrollArea();
    QTimer::singleShot(0, this, [targetScrollArea] {
        if (targetScrollArea)
            targetScrollArea->verticalScrollBar()->setValue(0);
    });
}

void MainWindow::rebuildHistory()
{
    QVBoxLayout *targetLayout = activeHistoryLayout();
    QWidget *targetContents = activeHistoryContents();
    if (!targetLayout || !targetContents)
        return;
    while (targetLayout->count() > 1) {
        QLayoutItem *item = targetLayout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
    QStringList boxOrder;
    QHash<QString, QVBoxLayout *> boxTargets;
    if (packingMode == PackingMode::Warehouse) {
        if (warehouseGroups.isEmpty()) {
            auto *empty = new QLabel(QStringLiteral("还没有装箱组\n\n点击右侧“创建一组”，一次创建5箱。"), targetContents);
            empty->setWordWrap(true);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QStringLiteral("color:#718096; padding:28px 12px;"));
            targetLayout->insertWidget(0, empty);
        }
        for (int index = warehouseGroups.size() - 1; index >= 0; --index) {
            const WarehouseGroup &group = warehouseGroups.at(index);
            auto *card = new QWidget(targetContents);
            card->setObjectName(QStringLiteral("warehouseGroupCard"));
            card->setProperty("groupNumber", group.number);
            auto *layout = new QVBoxLayout(card);
            layout->setSizeConstraint(QLayout::SetMinimumSize);
            layout->setContentsMargins(12, 12, 12, 12);
            layout->setSpacing(8);
            int total = 0;
            const bool complete = warehouseGroupComplete(group);
            const QString state = complete ? QStringLiteral("已完成 · 已锁定")
                : (group.boxIds.size() < 5 ? QStringLiteral("待补齐箱号") : QStringLiteral("装箱中"));
            auto *heading = new QLabel(QStringLiteral("第 %1 组  ·  %2").arg(group.number).arg(state), card);
            heading->setObjectName(QStringLiteral("warehouseGroupHeading"));
            heading->setWordWrap(true);
            layout->addWidget(heading);
            auto *summary = new QGridLayout;
            summary->setHorizontalSpacing(10);
            summary->setVerticalSpacing(4);
            for (int slot = 0; slot < 5; ++slot) {
                const QString box = group.boxIds.value(slot);
                int quantity = 0;
                for (int count : countsForBox(box)) quantity += count;
                total += quantity;
                auto *position = new QLabel(QStringLiteral("%1号位").arg(slot + 1), card);
                auto *boxLabel = new QLabel(box.isEmpty() ? QStringLiteral("待填写") : box, card);
                boxLabel->setTextFormat(Qt::PlainText);
                boxLabel->setWordWrap(true);
                boxLabel->setMinimumWidth(0);
                auto *countLabel = new QLabel(QStringLiteral("%1 件").arg(quantity), card);
                countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                summary->addWidget(position, slot, 0);
                summary->addWidget(boxLabel, slot, 1);
                summary->addWidget(countLabel, slot, 2);
            }
            summary->setColumnStretch(1, 1);
            layout->addLayout(summary);
            auto *totals = new QLabel(QStringLiteral("%1 / 5 箱  ·  合计 %2 件")
                .arg(group.boxIds.size()).arg(total), card);
            totals->setObjectName(QStringLiteral("warehouseGroupTotals"));
            layout->addWidget(totals);
            auto *toggle = new QPushButton(card);
            toggle->setObjectName(QStringLiteral("toggleGroupDetails"));
            toggle->setCheckable(true);
            const bool expanded = expandedWarehouseGroups.contains(group.number)
                || (!complete && !collapsedWarehouseGroups.contains(group.number));
            toggle->setChecked(expanded);
            toggle->setText(expanded ? QStringLiteral("收起五箱商品明细") : QStringLiteral("展开五箱商品明细"));
            layout->addWidget(toggle);
            auto *body = new QWidget(card);
            body->setObjectName(QStringLiteral("warehouseGroupDetails"));
            auto *bodyLayout = new QVBoxLayout(body);
            bodyLayout->setSizeConstraint(QLayout::SetMinimumSize);
            bodyLayout->setContentsMargins(0, 0, 0, 0);
            bodyLayout->setSpacing(6);
            layout->addWidget(body);
            body->setVisible(expanded);
            connect(toggle, &QPushButton::toggled, this, [this, card, layout, body, toggle, number = group.number](bool checked) {
                body->setVisible(checked);
                toggle->setText(checked ? QStringLiteral("收起五箱商品明细") : QStringLiteral("展开五箱商品明细"));
                if (checked) {
                    expandedWarehouseGroups.insert(number);
                    collapsedWarehouseGroups.remove(number);
                } else {
                    expandedWarehouseGroups.remove(number);
                    collapsedWarehouseGroups.insert(number);
                }
                layout->invalidate();
                layout->activate();
                card->setMinimumHeight(layout->minimumSize().height());
                card->setMaximumHeight(checked ? QWIDGETSIZE_MAX : layout->sizeHint().height());
                warehouseHistoryLayout->invalidate();
                warehouseHistoryLayout->activate();
            });
            card->setStyleSheet(QStringLiteral(
                "QWidget#warehouseGroupCard { background:white; border:1px solid %1; border-radius:10px; }"
                "QLabel#warehouseGroupHeading { color:%2; font-size:18px; font-weight:800; border:none; }"
                "QLabel#warehouseGroupTotals { color:#64748b; font-size:13px; padding:4px 0; }"
                "QPushButton#toggleGroupDetails { background:#f1f5fa; border:none; font-size:13px; min-height:18px; }")
                .arg(complete ? QStringLiteral("#a7d8b8") : QStringLiteral("#8cb7eb"),
                     complete ? QStringLiteral("#16833a") : QStringLiteral("#1559a4")));
            targetLayout->insertWidget(targetLayout->count() - 1, card);
            // Stable slot order includes empty boxes from the moment a group is created.
            for (const QString &box : group.boxIds) {
                boxOrder.append(box);
                boxTargets.insert(box, bodyLayout);
            }
        }
    } else {
        for (int i = records.size() - 1; i >= 0; --i) {
            if (!boxOrder.contains(records.at(i).box))
                boxOrder.append(records.at(i).box);
        }
    }

    for (const QString &box : boxOrder) {
        QVector<int> recordIndexes;
        for (int i = records.size() - 1; i >= 0; --i) {
            if (records.at(i).box == box)
                recordIndexes.append(i);
        }

        QVBoxLayout *boxTarget = boxTargets.value(box, targetLayout);
        auto *boxCard = new QWidget(boxTarget->parentWidget());
        boxCard->setObjectName("boxCard");
        auto *boxLayout = new QVBoxLayout(boxCard);
        boxLayout->setSizeConstraint(QLayout::SetMinimumSize);
        boxLayout->setContentsMargins(8, 7, 8, 7);
        boxLayout->setSpacing(5);

        QString boxTitleText = QStringLiteral("箱号：%1    已装箱：%2 件").arg(box).arg(recordIndexes.size());
        if (const WarehouseGroup *group = warehouseGroupForBox(box)) {
            const int slot = group->boxIds.indexOf(box) + 1;
            const QString state = group->completed
                ? QStringLiteral("本组已锁定") : QStringLiteral("装箱中");
            boxTitleText = QStringLiteral("%1号位 · 箱号 %2\n已装 %3 件 · %4")
                .arg(slot).arg(box).arg(recordIndexes.size()).arg(state);
        }
        auto *boxTitle = new QLabel(boxTitleText, boxCard);
        boxTitle->setTextFormat(Qt::PlainText);
        boxTitle->setWordWrap(true);
        QFont titleFont = boxTitle->font();
        titleFont.setBold(true);
        titleFont.setPointSize(12);
        boxTitle->setFont(titleFont);
        boxTitle->setObjectName("boxTitle");
        boxLayout->addWidget(boxTitle);

        QStringList productOrder;
        QHash<QString, QVector<int>> groupedRecords;
        for (int recordIndex : recordIndexes) {
            const ScanRecord &record = records.at(recordIndex);
            const QString key = record.fnsku.trimmed().toUpper();
            if (!groupedRecords.contains(key))
                productOrder.append(key);
            groupedRecords[key].append(recordIndex);
        }

        if (productOrder.isEmpty()) {
            auto *empty = new QLabel(QStringLiteral("暂无商品 · 等待扫码"), boxCard);
            empty->setStyleSheet(QStringLiteral("color:#94a3b8; font-size:13px; padding:4px;"));
            boxLayout->addWidget(empty);
        }
        for (const QString &key : productOrder) {
            const QVector<int> indexes = groupedRecords.value(key);
            const int newestRecordIndex = indexes.first();
            const ScanRecord &record = records.at(newestRecordIndex);
            const ProductIndexRecord indexedProduct = productIndex.value(record.fnsku.trimmed().toUpper());
            const QString displaySku = !indexedProduct.msku.trimmed().isEmpty()
                ? indexedProduct.msku.trimmed()
                : (!indexedProduct.sku.trimmed().isEmpty() ? indexedProduct.sku.trimmed() : record.sku);
            auto *row = new QWidget(boxCard);
            row->setObjectName("scanRow");
            auto *rowLayout = new QVBoxLayout(row);
            rowLayout->setContentsMargins(6, 4, 6, 4);
            rowLayout->setSpacing(2);

            auto *code = new QLabel(record.fnsku, row);
            QFont codeFont = code->font();
            codeFont.setBold(true);
            code->setFont(codeFont);
            code->setObjectName(QStringLiteral("scanCode"));
            rowLayout->addWidget(code);
            rowLayout->addWidget(new QLabel(
                QStringLiteral("SKU：%1").arg(record.sku.isEmpty()
                    ? QStringLiteral("未找到") : record.sku), row));

            auto *countLine = new QHBoxLayout;
            countLine->addWidget(new QLabel(
                QStringLiteral("数量：%1").arg(indexes.size()), row), 1);
            auto *remove = new QPushButton(QStringLiteral("删除一个"), row);
            remove->setMaximumWidth(85);
            countLine->addWidget(remove);
            countLine->insertStretch(0, 1);
            if (auto *quantityLabel = qobject_cast<QLabel *>(countLine->itemAt(1)->widget())) {
                quantityLabel->setText(QStringLiteral("数量：%1").arg(indexes.size()));
                QFont quantityFont = quantityLabel->font();
                quantityFont.setPointSize(20);
                quantityFont.setBold(true);
                quantityLabel->setFont(quantityFont);
                quantityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                quantityLabel->setMinimumWidth(110);
                quantityLabel->setObjectName(QStringLiteral("quantityLabel"));
            }
            remove->setMinimumHeight(36);
            remove->setObjectName(QStringLiteral("removeButton"));
            rowLayout->addLayout(countLine);
            rowLayout->addWidget(new QLabel(
                record.scannedAt.toString("yyyy-MM-dd HH:mm:ss") + QStringLiteral("  时间戳"), row));

            // Match the horizontalLayout_7 sample: information on the left,
            // quantity and delete action in one compact row on the right.
            QWidget *skuWidget = rowLayout->itemAt(1)->widget();
            QWidget *timeWidget = rowLayout->itemAt(3)->widget();
            if (auto *skuLabel = qobject_cast<QLabel *>(skuWidget))
                skuLabel->setText(QStringLiteral("SKU：%1").arg(
                    displaySku.isEmpty() ? QStringLiteral("未找到") : displaySku));
            rowLayout->removeWidget(code);
            rowLayout->removeWidget(skuWidget);
            rowLayout->removeItem(countLine);
            rowLayout->removeWidget(timeWidget);

            QLayoutItem *leadingSpace = countLine->takeAt(0);
            delete leadingSpace;
            countLine->setStretch(0, 0);
            countLine->setSpacing(8);

            auto *informationLayout = new QVBoxLayout;
            informationLayout->setContentsMargins(0, 0, 0, 0);
            informationLayout->setSpacing(2);
            informationLayout->addWidget(code);
            informationLayout->addWidget(skuWidget);
            informationLayout->addWidget(timeWidget);

            auto *horizontalLayout = new QHBoxLayout;
            horizontalLayout->setContentsMargins(0, 0, 0, 0);
            horizontalLayout->setSpacing(10);
            horizontalLayout->addLayout(informationLayout, 1);
            horizontalLayout->addLayout(countLine, 0);
            rowLayout->addLayout(horizontalLayout);

            if (packingMode == PackingMode::Warehouse) {
                remove->setText(QStringLiteral("−1"));
                remove->setToolTip(QStringLiteral("从本箱删除一件此商品"));
                remove->setMaximumWidth(46);
                remove->setEnabled(!isWarehouseBoxLocked(box));
                auto *quantity = qobject_cast<QLabel *>(countLine->itemAt(0)->widget());
                quantity->setMinimumWidth(62);
                quantity->setText(QStringLiteral("%1 件").arg(indexes.size()));
                quantity->setStyleSheet(QStringLiteral("font-size:17px;"));
                timeWidget->setStyleSheet(QStringLiteral("font-size:11px; color:#64748b;"));
            }
            const QString recordBox = record.box;
            connect(remove, &QPushButton::clicked, this, [this, newestRecordIndex, recordBox] {
                if (isWarehouseBoxLocked(recordBox)) {
                    QMessageBox::warning(this, QStringLiteral("箱子已锁定"),
                        QStringLiteral("已完成校验的分仓箱不能直接删除商品。"));
                    return;
                }
                records.removeAt(newestRecordIndex);
                rebuildHistory();
                rebuildPackingPlan();
                updateModeUi();
                saveSessionState();
            });
            boxLayout->addWidget(row);
        }

        boxCard->setStyleSheet(
            "QWidget#boxCard { background:#f5f8fc; border:1px solid #9eafc2; border-radius:8px; }"
            "QLabel#boxTitle { color:#0d4778; border:none; padding:3px 2px 7px 2px; font-size:17px; font-weight:800; }"
            "QWidget#scanRow { background:#ffffff; border:1px solid #d5dee8; border-radius:6px; }"
            "QWidget#scanRow QLabel, QWidget#scanRow QPushButton { border:none; }"
            "QLabel#scanCode { color:#17365d; font-size:15px; font-weight:800; }"
            "QLabel#quantityLabel { color:#0d47a1; font-size:20px; font-weight:900; }"
            "QPushButton#removeButton { background:#fff4f2; color:#b42318; border:1px solid #efb4ae; border-radius:5px; padding:6px 10px; }"
            "QPushButton#removeButton:hover { background:#ffe2df; border-color:#d92d20; }");
        if (boxTarget == targetLayout)
            targetLayout->insertWidget(targetLayout->count() - 1, boxCard);
        else
            boxTarget->addWidget(boxCard);
    }
}

void MainWindow::clearRecords()
{
    records.clear();
    expandedWarehouseGroups.clear();
    collapsedWarehouseGroups.clear();
    ui->label_6->clear();
    ui->label_wh_current->clear();
    ui->textEdit->clear();
    ui->textEdit_wh_scan->clear();
    ui->textEdit_2->clear();
    ui->textEdit_wh_box->clear();
    ui->lineEdit->clear();
    ui->lineEdit_wh_box->clear();
    boxes.clear();
    warehouseGroups.clear();
    currentBoxIndex = -1;
    rebuildHistory();
    rebuildPackingPlan();
    updateModeUi();
    saveSessionState();
}

void MainWindow::confirmClearPackingData()
{
    if (records.isEmpty() && boxes.isEmpty() && warehouseGroups.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("无需清空"),
            QStringLiteral("当前没有扫码记录或箱号。"));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, QStringLiteral("确认清空本次装箱数据"),
        QStringLiteral("将清空所有扫码记录、箱号和分仓组。\n"
                       "装箱计划与FNSKU索引表会保留。\n\n"
                       "此操作无法撤销，是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    clearRecords();
    focusPackingInput();
}

void MainWindow::saveSessionState()
{
    if (restoringSession)
        return;

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), 3);
    root.insert(QStringLiteral("packingMode"), static_cast<int>(packingMode));
    root.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("currentBoxIndex"), currentBoxIndex);

    QJsonArray boxArray;
    for (const QString &box : std::as_const(boxes))
        boxArray.append(box);
    root.insert(QStringLiteral("boxes"), boxArray);

    QJsonArray recordArray;
    for (const ScanRecord &record : std::as_const(records)) {
        QJsonObject value;
        value.insert(QStringLiteral("fnsku"), record.fnsku);
        value.insert(QStringLiteral("sku"), record.sku);
        value.insert(QStringLiteral("title"), record.title);
        value.insert(QStringLiteral("box"), record.box);
        value.insert(QStringLiteral("scannedAt"), record.scannedAt.toString(Qt::ISODateWithMs));
        value.insert(QStringLiteral("packingMode"), record.packingMode);
        value.insert(QStringLiteral("warehouseGroup"), record.warehouseGroup);
        value.insert(QStringLiteral("warehouseSlot"), record.warehouseSlot);
        recordArray.append(value);
    }
    root.insert(QStringLiteral("records"), recordArray);

    QJsonArray planArray;
    for (const PackingPlanItem &item : std::as_const(packingPlan)) {
        QJsonObject value;
        value.insert(QStringLiteral("sku"), item.sku);
        value.insert(QStringLiteral("fnsku"), item.fnsku);
        value.insert(QStringLiteral("plannedQuantity"), item.plannedQuantity);
        planArray.append(value);
    }
    root.insert(QStringLiteral("packingPlan"), planArray);

    QJsonArray groupArray;
    for (const WarehouseGroup &group : std::as_const(warehouseGroups)) {
        QJsonObject value;
        value.insert(QStringLiteral("number"), group.number);
        QJsonArray groupBoxes;
        for (const QString &box : group.boxIds) groupBoxes.append(box);
        value.insert(QStringLiteral("boxes"), groupBoxes);
        value.insert(QStringLiteral("completed"), group.completed);
        groupArray.append(value);
    }
    root.insert(QStringLiteral("warehouseGroups"), groupArray);

    const QString path = QDir(dataDirectory()).filePath(QStringLiteral("packing_autosave.json"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

void MainWindow::restoreSessionState()
{
    const QString path = QDir(dataDirectory()).filePath(QStringLiteral("packing_autosave.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    restoringSession = true;
    const QJsonObject root = document.object();
    const int formatVersion = root.value(QStringLiteral("formatVersion")).toInt(1);
    boxes.clear();
    records.clear();
    packingPlan.clear();
    warehouseGroups.clear();
    packingMode = static_cast<PackingMode>(root.value(QStringLiteral("packingMode")).toInt(0));

    for (const QJsonValue &value : root.value(QStringLiteral("boxes")).toArray()) {
        const QString box = value.toString().trimmed();
        if (!box.isEmpty() && !boxes.contains(box))
            boxes.append(box);
    }

    for (const QJsonValue &value : root.value(QStringLiteral("records")).toArray()) {
        const QJsonObject object = value.toObject();
        const QString fnsku = object.value(QStringLiteral("fnsku")).toString().trimmed();
        const QString box = object.value(QStringLiteral("box")).toString().trimmed();
        if (fnsku.isEmpty() || box.isEmpty())
            continue;
        QDateTime scannedAt = QDateTime::fromString(
            object.value(QStringLiteral("scannedAt")).toString(), Qt::ISODateWithMs);
        if (!scannedAt.isValid())
            scannedAt = QDateTime::currentDateTime();
        ScanRecord record{fnsku,
                          object.value(QStringLiteral("sku")).toString(),
                          object.value(QStringLiteral("title")).toString(),
                          box, scannedAt};
        record.packingMode = object.value(QStringLiteral("packingMode")).toInt(0);
        record.warehouseGroup = object.value(QStringLiteral("warehouseGroup")).toInt(0);
        record.warehouseSlot = object.value(QStringLiteral("warehouseSlot")).toInt(0);
        records.append(record);
        if (!boxes.contains(box))
            boxes.append(box);
    }

    for (const QJsonValue &value : root.value(QStringLiteral("packingPlan")).toArray()) {
        const QJsonObject object = value.toObject();
        const QString sku = object.value(QStringLiteral("sku")).toString().trimmed();
        const QString fnsku = object.value(QStringLiteral("fnsku")).toString().trimmed();
        const int planned = object.value(QStringLiteral("plannedQuantity")).toInt();
        if (!sku.isEmpty() && !fnsku.isEmpty() && planned > 0)
            packingPlan.append({sku, fnsku, planned});
    }

    for (const QJsonValue &value : root.value(QStringLiteral("warehouseGroups")).toArray()) {
        const QJsonObject object = value.toObject();
        WarehouseGroup group;
        group.number = object.value(QStringLiteral("number")).toInt();
        for (const QJsonValue &boxValue : object.value(QStringLiteral("boxes")).toArray()) {
            const QString box = boxValue.toString().trimmed();
            if (!box.isEmpty() && !group.boxIds.contains(box)) {
                group.boxIds.append(box);
                if (!boxes.contains(box))
                    boxes.append(box);
            }
        }

        bool completionCandidate = false;
        if (formatVersion >= 3 || object.contains(QStringLiteral("completed"))) {
            completionCandidate = object.value(QStringLiteral("completed")).toBool(false);
        } else {
            // Legacy v2 used a template box. Preserve its data, but only migrate
            // a group that had all five boxes verified and is still balanced.
            QSet<QString> legacyVerifiedBoxes;
            for (const QJsonValue &boxValue : object.value(QStringLiteral("verifiedBoxes")).toArray()) {
                const QString box = boxValue.toString().trimmed();
                if (!box.isEmpty())
                    legacyVerifiedBoxes.insert(box);
            }
            completionCandidate = object.value(QStringLiteral("templateLocked")).toBool(false)
                && group.boxIds.size() == 5;
            for (const QString &box : std::as_const(group.boxIds))
                completionCandidate = completionCandidate && legacyVerifiedBoxes.contains(box);
        }

        int itemTotal = 0;
        for (const QString &box : std::as_const(group.boxIds)) {
            const QHash<QString, int> counts = countsForBox(box);
            for (int quantity : counts)
                itemTotal += quantity;
        }
        group.completed = completionCandidate && group.boxIds.size() == 5
            && itemTotal > 0 && warehouseMismatchText(group).isEmpty();
        if (group.number > 0)
            warehouseGroups.append(group);
    }

    // Group membership is derived from the saved box map. This also repairs
    // older autosaves whose per-record group/slot fields were absent or stale.
    for (ScanRecord &record : records) {
        if (WarehouseGroup *group = warehouseGroupForBox(record.box)) {
            record.packingMode = static_cast<int>(PackingMode::Warehouse);
            record.warehouseGroup = group->number;
            record.warehouseSlot = group->boxIds.indexOf(record.box) + 1;
        }
    }

    currentBoxIndex = root.value(QStringLiteral("currentBoxIndex")).toInt(-1);
    if (boxes.isEmpty()) {
        currentBoxIndex = -1;
        activeBoxDisplay()->clear();
    } else {
        currentBoxIndex = qBound(0, currentBoxIndex, boxes.size() - 1);
        if (packingMode == PackingMode::Loose) {
            activeBoxDisplay()->setPlainText(boxes.at(currentBoxIndex));
            activeBoxDisplay()->setAlignment(Qt::AlignCenter);
        }
    }
    rebuildHistory();
    rebuildPackingPlan();
    updateModeUi();
    restoringSession = false;

    if (!records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("数据已恢复"),
            QStringLiteral("已自动恢复 %1 条扫码记录、%2 个箱号。")
                .arg(records.size()).arg(boxes.size()));
    }
}

void MainWindow::rebuildPackingPlan()
{
    QVBoxLayout *targetLayout = activePackingPlanLayout();
    QWidget *targetContents = activePackingPlanContents();
    if (!targetLayout || !targetContents)
        return;
    while (targetLayout->count() > 1) {
        QLayoutItem *item = targetLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    QHash<QString, int> scannedByFnsku;
    QHash<QString, QDateTime> latestScanByFnsku;
    for (const ScanRecord &record : records) {
        if (!record.fnsku.trimmed().isEmpty()) {
            const QString key = record.fnsku.trimmed().toUpper();
            ++scannedByFnsku[key];
            if (!latestScanByFnsku.value(key).isValid() || record.scannedAt > latestScanByFnsku.value(key))
                latestScanByFnsku[key] = record.scannedAt;
        }
    }

    QVector<int> displayOrder;
    for (int i = 0; i < packingPlan.size(); ++i) displayOrder.append(i);
    std::stable_sort(displayOrder.begin(), displayOrder.end(),
        [this, &latestScanByFnsku](int left, int right) {
            const QDateTime leftTime = latestScanByFnsku.value(packingPlan.at(left).fnsku.toUpper());
            const QDateTime rightTime = latestScanByFnsku.value(packingPlan.at(right).fnsku.toUpper());
            if (leftTime.isValid() != rightTime.isValid()) return leftTime.isValid();
            return leftTime.isValid() && leftTime > rightTime;
        });

    QSet<QString> plannedFnskus;
    for (const PackingPlanItem &item : std::as_const(packingPlan))
        plannedFnskus.insert(item.fnsku.trimmed().toUpper());

    QStringList unplannedFnskus;
    for (auto it = scannedByFnsku.constBegin(); it != scannedByFnsku.constEnd(); ++it) {
        if (!plannedFnskus.contains(it.key()))
            unplannedFnskus.append(it.key());
    }
    std::sort(unplannedFnskus.begin(), unplannedFnskus.end(),
        [&latestScanByFnsku](const QString &left, const QString &right) {
            return latestScanByFnsku.value(left) > latestScanByFnsku.value(right);
        });

    for (const QString &fnsku : std::as_const(unplannedFnskus)) {
        const ProductIndexRecord indexed = productIndex.value(fnsku);
        QString displaySku = !indexed.msku.trimmed().isEmpty()
            ? indexed.msku.trimmed() : indexed.sku.trimmed();
        if (displaySku.isEmpty()) {
            for (int i = records.size() - 1; i >= 0; --i) {
                if (records.at(i).fnsku.trimmed().compare(fnsku, Qt::CaseInsensitive) == 0) {
                    displaySku = records.at(i).sku.trimmed();
                    break;
                }
            }
        }
        if (displaySku.isEmpty())
            displaySku = QStringLiteral("未知SKU");

        auto *card = new QWidget(targetContents);
        card->setObjectName(QStringLiteral("unplannedCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(3);

        auto *skuLabel = new QLabel(displaySku, card);
        QFont skuFont = skuLabel->font();
        skuFont.setBold(true);
        skuLabel->setFont(skuFont);
        skuLabel->setWordWrap(true);
        layout->addWidget(skuLabel);
        auto *packed = new QLabel(QStringLiteral("FNSKU：%1\n已装：%2")
            .arg(fnsku).arg(scannedByFnsku.value(fnsku)), card);
        packed->setWordWrap(true);
        layout->addWidget(packed);
        auto *warningLabel = new QLabel(QStringLiteral("未在装箱计划内"), card);
        QFont warningFont = warningLabel->font();
        warningFont.setBold(true);
        warningLabel->setFont(warningFont);
        warningLabel->setStyleSheet(QStringLiteral("color:#b42318;"));
        layout->addWidget(warningLabel);
        card->setStyleSheet(QStringLiteral(
            "QWidget#unplannedCard { background-color:#fde8e7; border:2px solid #d92d20; border-radius:5px; } "
            "QWidget#unplannedCard QLabel { background-color:transparent; border:none; }"));
        targetLayout->insertWidget(targetLayout->count() - 1, card);
    }

    for (int planIndex : displayOrder) {
        const PackingPlanItem &item = packingPlan.at(planIndex);
        const int scanned = scannedByFnsku.value(item.fnsku.toUpper());
        const int difference = scanned - item.plannedQuantity;
        const QString differenceText = difference > 0
            ? QStringLiteral("+%1").arg(difference)
            : QString::number(difference);
        const bool quantityMatches = scanned == item.plannedQuantity;
        auto *card = new QPushButton(targetContents);
        card->setObjectName(QStringLiteral("planCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setCursor(Qt::PointingHandCursor);
        card->setToolTip(QStringLiteral("点击查看该款装入了哪些箱子"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(3);
        auto *skuLabel = new QLabel(item.sku, card);
        QFont font = skuLabel->font();
        font.setBold(true);
        skuLabel->setFont(font);
        skuLabel->setWordWrap(true);
        skuLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(skuLabel);
        auto *differenceLabel = new QLabel(
            QStringLiteral("计划：%1　已装：%2　差额：%3")
                .arg(item.plannedQuantity).arg(scanned).arg(differenceText), card);
        differenceLabel->setWordWrap(true);
        differenceLabel->setStyleSheet(quantityMatches
            ? QStringLiteral("color:#16833a;") : QStringLiteral("color:#7a5b00;"));
        differenceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(differenceLabel);
        const QString background = quantityMatches ? QStringLiteral("#ffffff") : QStringLiteral("#fff3b0");
        const QString border = quantityMatches ? QStringLiteral("#d9dee7") : QStringLiteral("#e0b400");
        card->setStyleSheet(QStringLiteral(
            "QPushButton#planCard { text-align:left; background-color:%1; border:1px solid %2; border-radius:4px; } "
            "QPushButton#planCard:hover { border:2px solid #2878c8; } "
            "QPushButton#planCard:pressed { background-color:#e8f2ff; } "
            "QPushButton#planCard QLabel { background-color:transparent; border:none; }")
            .arg(background, border));
        const QString sku = item.sku;
        const QString fnsku = item.fnsku;
        connect(card, &QPushButton::clicked, this, [this, sku, fnsku] {
            showPackingLocations(sku, fnsku);
        });
        targetLayout->insertWidget(targetLayout->count() - 1, card);
    }
}

void MainWindow::showPackingLocations(const QString &sku, const QString &fnsku)
{
    QStringList boxOrder;
    QMap<QString, int> boxCounts;
    for (const ScanRecord &record : std::as_const(records)) {
        if (record.fnsku.trimmed().compare(fnsku.trimmed(), Qt::CaseInsensitive) != 0)
            continue;
        if (!boxCounts.contains(record.box)) boxOrder.append(record.box);
        ++boxCounts[record.box];
    }

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("装箱位置明细"));
    dialog->resize(430, 360);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *skuLabel = new QLabel(QStringLiteral("SKU：%1").arg(sku), dialog);
    QFont titleFont = skuLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    skuLabel->setFont(titleFont);
    skuLabel->setWordWrap(true);
    layout->addWidget(skuLabel);

    auto *fnskuLabel = new QLabel(QStringLiteral("FNSKU：%1").arg(fnsku), dialog);
    fnskuLabel->setStyleSheet(QStringLiteral("color:#526579; font-size:13px;"));
    layout->addWidget(fnskuLabel);

    int total = 0;
    auto *list = new QListWidget(dialog);
    list->setAlternatingRowColors(true);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#ffffff; border:1px solid #b9c8d8; border-radius:6px; font-size:16px; }"
        "QListWidget::item { padding:10px 12px; }"));
    for (const QString &box : std::as_const(boxOrder)) {
        const int count = boxCounts.value(box);
        total += count;
        list->addItem(QStringLiteral("箱号：%1                         %2 件").arg(box).arg(count));
    }
    if (boxOrder.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(QStringLiteral("该款当前尚未装入任何箱子"), list);
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setForeground(QColor(QStringLiteral("#7a5b00")));
    }
    layout->addWidget(list, 1);

    auto *totalLabel = new QLabel(QStringLiteral("涉及箱数：%1 个    已装合计：%2 件")
        .arg(boxOrder.size()).arg(total), dialog);
    QFont totalFont = totalLabel->font();
    totalFont.setPointSize(14);
    totalFont.setBold(true);
    totalLabel->setFont(totalFont);
    totalLabel->setStyleSheet(QStringLiteral("color:#0d47a1;"));
    layout->addWidget(totalLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog->open();
}

void MainWindow::exportPackingPlanTemplate()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("输出装箱计划模板"),
        QStringLiteral("装箱计划模板.xlsx"), QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) path += QStringLiteral(".xlsx");
    const QVector<QStringList> rows{{
        QStringLiteral("SKU"), QStringLiteral("计划数量"), QStringLiteral("FNSKU"),
        QStringLiteral("实际总数量"), QStringLiteral("装箱数量"), QStringLiteral("箱子编号"),
        QStringLiteral("时间戳"), QStringLiteral("差异数量")}};
    if (!writeSimpleXlsx(path, QStringLiteral("装箱计划"), rows)) {
        QMessageBox::critical(this, QStringLiteral("输出失败"), QStringLiteral("无法写入装箱计划模板。"));
        return;
    }
    QMessageBox::information(this, QStringLiteral("输出成功"),
        QStringLiteral("模板已保存，请填写 SKU 和数量后再导入。"));
}

void MainWindow::importPackingPlan()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入装箱计划"), QString(),
        QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    QString error;
    const QVector<QStringList> rows = readSimpleXlsx(path, &error);
    if (!error.isEmpty() || rows.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("导入失败"),
            error.isEmpty() ? QStringLiteral("装箱计划为空。") : error);
        return;
    }
    if (rows.first().value(0).trimmed().compare(QStringLiteral("SKU"), Qt::CaseInsensitive) != 0 ||
        rows.first().value(1).trimmed() != QStringLiteral("数量")) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("第一行必须是“SKU”和“数量”，请使用输出的模板填写。"));
        return;
    }

    QHash<QString, ProductIndexRecord> indexedSkus;
    for (const ProductIndexRecord &product : std::as_const(productIndex)) {
        if (!product.sku.trimmed().isEmpty()) indexedSkus.insert(product.sku.trimmed().toUpper(), product);
        if (!product.msku.trimmed().isEmpty()) indexedSkus.insert(product.msku.trimmed().toUpper(), product);
    }
    if (indexedSkus.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("缺少索引"),
            QStringLiteral("当前索引表没有 SKU 数据，请先点击“更新索引表”。"));
        return;
    }

    QStringList order;
    QHash<QString, int> quantities;
    QHash<QString, QString> displaySkus;
    QStringList missing, invalidRows;
    for (int r = 1; r < rows.size(); ++r) {
        const QString inputSku = rows.at(r).value(0).trimmed();
        const QString quantityText = rows.at(r).value(1).trimmed();
        if (inputSku.isEmpty() && quantityText.isEmpty()) continue;
        bool ok = false;
        const int quantity = quantityText.toInt(&ok);
        if (inputSku.isEmpty() || !ok || quantity <= 0) {
            invalidRows.append(QString::number(r + 1));
            continue;
        }
        const QString key = inputSku.toUpper();
        if (!indexedSkus.contains(key)) {
            if (!missing.contains(inputSku)) missing.append(inputSku);
            continue;
        }
        const ProductIndexRecord matchedProduct = indexedSkus.value(key);
        const QString fnskuKey = matchedProduct.fnsku.trimmed().toUpper();
        if (fnskuKey.isEmpty()) {
            if (!missing.contains(inputSku)) missing.append(inputSku);
            continue;
        }
        if (!quantities.contains(fnskuKey)) {
            order.append(fnskuKey);
            displaySkus.insert(fnskuKey, !matchedProduct.msku.trimmed().isEmpty()
                ? matchedProduct.msku.trimmed() : matchedProduct.sku.trimmed());
        }
        quantities[fnskuKey] += quantity;
    }
    if (order.isEmpty()) {
        QString detail = QStringLiteral("没有可导入的有效 SKU。\n请确认 SKU 已存在于索引表，数量为正整数。");
        if (!missing.isEmpty()) detail += QStringLiteral("\n未找到：%1").arg(missing.mid(0, 10).join(QStringLiteral("、")));
        QMessageBox::warning(this, QStringLiteral("导入失败"), detail);
        return;
    }

    packingPlan.clear();
    for (const QString &key : order)
        packingPlan.append({displaySkus.value(key), key, quantities.value(key)});
    rebuildPackingPlan();
    saveSessionState();
    activePackingPlanScrollArea()->verticalScrollBar()->setValue(0);

    QString result = QStringLiteral("已导入 %1 个 SKU，历史扫码数量已自动扣减。").arg(packingPlan.size());
    if (!missing.isEmpty()) result += QStringLiteral("\n%1 个 SKU 未在索引表中，已跳过：%2")
        .arg(missing.size()).arg(missing.mid(0, 10).join(QStringLiteral("、")));
    if (!invalidRows.isEmpty()) result += QStringLiteral("\n数量无效的行已跳过：%1").arg(invalidRows.join(QStringLiteral("、")));
    QMessageBox::information(this, QStringLiteral("导入完成"), result);
}

void MainWindow::importUnifiedPackingPlan()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入装箱计划"), QString(), QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;

    QString error;
    const QVector<QStringList> rows = readSimpleXlsx(path, &error);
    if (!error.isEmpty() || rows.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("导入失败"),
            error.isEmpty() ? QStringLiteral("装箱计划为空。") : error);
        return;
    }

    const QStringList headers = rows.first();
    auto column = [&headers](const QString &name) {
        for (int i = 0; i < headers.size(); ++i) {
            if (headers.at(i).trimmed().compare(name, Qt::CaseInsensitive) == 0) return i;
        }
        return -1;
    };
    const int skuCol = column(QStringLiteral("SKU"));
    int planCol = column(QStringLiteral("计划数量"));
    if (planCol < 0) planCol = column(QStringLiteral("数量"));
    const int fnskuCol = column(QStringLiteral("FNSKU"));
    if (skuCol < 0 || planCol < 0) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("表格必须包含“SKU”和“计划数量”两列。旧模板的“数量”列也可兼容。"));
        return;
    }

    QHash<QString, ProductIndexRecord> indexedSkus;
    for (const ProductIndexRecord &product : std::as_const(productIndex)) {
        if (!product.sku.trimmed().isEmpty()) indexedSkus.insert(product.sku.trimmed().toUpper(), product);
        if (!product.msku.trimmed().isEmpty()) indexedSkus.insert(product.msku.trimmed().toUpper(), product);
    }

    QVector<PackingPlanItem> imported;
    QStringList missing;
    QStringList invalidRows;
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &cells = rows.at(r);
        const QString inputSku = cells.value(skuCol).trimmed();
        const QString quantityText = cells.value(planCol).trimmed();
        const QString inputFnsku = fnskuCol >= 0 ? cells.value(fnskuCol).trimmed().toUpper() : QString();
        if (inputSku.isEmpty() && quantityText.isEmpty() && inputFnsku.isEmpty()) continue;

        bool ok = false;
        const int quantity = quantityText.toInt(&ok);
        if (inputSku.isEmpty() || !ok || quantity <= 0) {
            invalidRows.append(QString::number(r + 1));
            continue;
        }

        ProductIndexRecord matched;
        QString fnsku = inputFnsku;
        if (!fnsku.isEmpty()) matched = productIndex.value(fnsku);
        if (fnsku.isEmpty()) {
            matched = indexedSkus.value(inputSku.toUpper());
            fnsku = matched.fnsku.trimmed().toUpper();
        }
        if (fnsku.isEmpty()) {
            if (!missing.contains(inputSku)) missing.append(inputSku);
            continue;
        }

        // Keep every source row in its original position; do not aggregate or sort here.
        imported.append({inputSku, fnsku, quantity});
    }

    if (imported.isEmpty()) {
        QString detail = QStringLiteral("没有可导入的有效计划行。请检查 SKU、计划数量和索引表。");
        if (!missing.isEmpty()) detail += QStringLiteral("\n未找到：%1").arg(missing.mid(0, 10).join(QStringLiteral("、")));
        QMessageBox::warning(this, QStringLiteral("导入失败"), detail);
        return;
    }
    if (packingMode == PackingMode::Warehouse) {
        QString planError;
        if (!validateWarehousePlan(imported, &planError)) {
            QMessageBox::warning(this, QStringLiteral("无法导入分仓计划"), planError);
            return;
        }
    }

    packingPlan = imported;
    rebuildPackingPlan();
    saveSessionState();
    activePackingPlanScrollArea()->verticalScrollBar()->setValue(0);

    QString result = QStringLiteral("已按表格原始顺序导入 %1 行装箱计划。").arg(packingPlan.size());
    if (!missing.isEmpty()) result += QStringLiteral("\n%1 个 SKU 未在索引表中找到，已跳过：%2")
        .arg(missing.size()).arg(missing.mid(0, 10).join(QStringLiteral("、")));
    if (!invalidRows.isEmpty()) result += QStringLiteral("\n无效行已跳过：%1").arg(invalidRows.join(QStringLiteral("、")));
    QMessageBox::information(this, QStringLiteral("导入完成"), result);
}

void MainWindow::exportUnifiedPackingList()
{
    if (packingPlan.isEmpty() && records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有装箱计划或扫码数据。"));
        return;
    }
    if (packingMode == PackingMode::Warehouse) {
        QStringList errors;
        QString planError;
        if (!packingPlan.isEmpty() && !validateWarehousePlan(packingPlan, &planError))
            errors.append(planError);
        if (warehouseGroups.isEmpty())
            errors.append(QStringLiteral("尚未创建分仓组"));
        for (const WarehouseGroup &group : std::as_const(warehouseGroups)) {
            const QString mismatch = warehouseMismatchText(group);
            if (!group.completed)
                errors.append(QStringLiteral("第 %1 组尚未点击“完成本组并校验五箱”").arg(group.number));
            if (!mismatch.isEmpty())
                errors.append(QStringLiteral("第 %1 组校验失败：\n%2")
                    .arg(group.number).arg(mismatch));
        }

        for (const ScanRecord &record : std::as_const(records)) {
            const WarehouseGroup *group = warehouseGroupForBox(record.box);
            const int expectedSlot = group ? group->boxIds.indexOf(record.box) + 1 : 0;
            if (!group || record.warehouseGroup != group->number
                || record.warehouseSlot != expectedSlot) {
                errors.append(QStringLiteral("箱号 %1 的分仓组/箱位信息异常").arg(record.box));
                break;
            }
        }
        if (!errors.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("分仓数据不能输出"),
                QStringLiteral("必须完成所有五箱组并保持五箱完全一致：\n%1")
                    .arg(errors.join(QLatin1Char('\n'))));
            return;
        }
    }

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("输出装箱单"),
        QStringLiteral("装箱单_%1.xlsx").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) path += QStringLiteral(".xlsx");

    struct ActualSummary {
        QString sku;
        QString fnsku;
        QStringList boxOrder;
        QMap<QString, int> boxCounts;
        QDateTime latestTime;
    };
    QMap<QString, ActualSummary> actuals;
    QStringList actualOrder;
    for (const ScanRecord &record : std::as_const(records)) {
        const QString key = record.fnsku.trimmed().toUpper();
        if (key.isEmpty()) continue;
        if (!actuals.contains(key)) {
            ActualSummary summary;
            const ProductIndexRecord indexed = productIndex.value(key);
            summary.sku = !indexed.msku.trimmed().isEmpty() ? indexed.msku.trimmed()
                : (!indexed.sku.trimmed().isEmpty() ? indexed.sku.trimmed() : record.sku.trimmed());
            summary.fnsku = record.fnsku.trimmed();
            actuals.insert(key, summary);
            actualOrder.append(key);
        }
        ActualSummary &summary = actuals[key];
        if (!summary.boxCounts.contains(record.box)) summary.boxOrder.append(record.box);
        ++summary.boxCounts[record.box];
        if (!summary.latestTime.isValid() || record.scannedAt > summary.latestTime)
            summary.latestTime = record.scannedAt;
    }

    QVector<QStringList> outputRows;
    outputRows.append({QStringLiteral("SKU"), QStringLiteral("计划数量"), QStringLiteral("FNSKU"),
        QStringLiteral("实际总数量"), QStringLiteral("装箱数量"), QStringLiteral("箱子编号"),
        QStringLiteral("时间戳"), QStringLiteral("差异数量"), QStringLiteral("装箱模式"),
        QStringLiteral("分仓组信息")});

    QSet<QString> plannedFnskus;
    auto appendRow = [this, &outputRows](const QString &sku, int planned, const QString &fnsku,
                                        const ActualSummary *summary) {
        QStringList counts;
        int actualTotal = 0;
        QStringList boxes;
        QString timestamp;
        QStringList groupInformation;
        if (summary) {
            boxes = summary->boxOrder;
            for (const QString &box : summary->boxOrder) {
                const int count = summary->boxCounts.value(box);
                counts.append(QString::number(count));
                actualTotal += count;
                const WarehouseGroup *group = warehouseGroupForBox(box);
                if (group) {
                    groupInformation.append(QStringLiteral("第%1组-%2")
                        .arg(group->number).arg(group->boxIds.indexOf(box) + 1));
                } else {
                    groupInformation.append(QString());
                }
            }
            if (summary->latestTime.isValid())
                timestamp = summary->latestTime.toString(QStringLiteral("yyyy/M/d HH:mm:ss"));
        }
        const int difference = actualTotal - planned;
        const QString differenceText = difference > 0
            ? QStringLiteral("+%1").arg(difference) : QString::number(difference);
        outputRows.append({sku, QString::number(planned), fnsku, QString::number(actualTotal),
            counts.join(' '), boxes.join(' '), timestamp, differenceText,
            packingMode == PackingMode::Warehouse ? QStringLiteral("分仓模式") : QStringLiteral("散货模式"),
            groupInformation.join(' ')});
    };

    // Planned rows are emitted first and in exactly the same order as the imported workbook.
    for (const PackingPlanItem &item : std::as_const(packingPlan)) {
        const QString key = item.fnsku.trimmed().toUpper();
        plannedFnskus.insert(key);
        const auto it = actuals.constFind(key);
        appendRow(item.sku, item.plannedQuantity, item.fnsku,
                  it == actuals.cend() ? nullptr : &it.value());
    }
    // Products outside the plan are never inserted between planned rows; append them at the end.
    for (const QString &key : actualOrder) {
        if (plannedFnskus.contains(key)) continue;
        const ActualSummary &summary = actuals[key];
        appendRow(summary.sku, 0, summary.fnsku, &summary);
    }

    if (!writeSimpleXlsx(path, QStringLiteral("装箱计划"), outputRows)) {
        QMessageBox::critical(this, QStringLiteral("输出失败"), QStringLiteral("无法写入装箱单文件。"));
        return;
    }
    clearRecords();
    QMessageBox::information(this, QStringLiteral("输出成功"),
        QStringLiteral("装箱单已按计划原始行序保存，差异数量已写入最后一列。"));
}

void MainWindow::exportPackingList()
{
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有可输出的扫码数据。"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("输出装箱单"),
        QStringLiteral("装箱单_%1.xlsx").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".xlsx", Qt::CaseInsensitive))
        path += ".xlsx";

    QString sheet = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                    "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
    auto row = [&sheet](int number, const QStringList &values) {
        sheet += QString("<row r=\"%1\">").arg(number);
        for (int i = 0; i < values.size(); ++i) {
            const QString cell = QString(QChar('A' + i)) + QString::number(number);
            sheet += QString("<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>")
                         .arg(cell, xmlEscape(values.at(i)));
        }
        sheet += "</row>";
    };
    struct PackingSummary {
        QString sku;
        QString fnsku;
        QStringList boxOrder;
        QMap<QString, int> boxCounts;
        QDateTime latestTime;
    };
    QStringList summaryOrder;
    QMap<QString, PackingSummary> summaries;
    for (const auto &record : records) {
        const QString key = record.fnsku.trimmed().toUpper();
        if (!summaries.contains(key)) {
            PackingSummary summary;
            const ProductIndexRecord indexed = productIndex.value(key);
            summary.sku = !indexed.msku.trimmed().isEmpty()
                ? indexed.msku.trimmed()
                : (!indexed.sku.trimmed().isEmpty() ? indexed.sku.trimmed() : record.sku);
            summary.fnsku = record.fnsku;
            summaries.insert(key, summary);
            summaryOrder.append(key);
        }
        PackingSummary &summary = summaries[key];
        if (!summary.boxCounts.contains(record.box))
            summary.boxOrder.append(record.box);
        ++summary.boxCounts[record.box];
        if (!summary.latestTime.isValid() || record.scannedAt > summary.latestTime)
            summary.latestTime = record.scannedAt;
    }

    row(1, {QStringLiteral("SKU"), QStringLiteral("FNSKU"), QStringLiteral("总数量"),
            QStringLiteral("装箱数量"), QStringLiteral("箱子编号"), QStringLiteral("时间戳")});
    int rowNumber = 2;
    for (const QString &key : summaryOrder) {
        const PackingSummary &summary = summaries[key];
        QStringList packedCounts;
        int total = 0;
        for (const QString &box : summary.boxOrder) {
            const int count = summary.boxCounts.value(box);
            packedCounts.append(QString::number(count));
            total += count;
        }
        row(rowNumber++, {summary.sku, summary.fnsku, QString::number(total),
                          packedCounts.join(' '), summary.boxOrder.join(' '),
                          summary.latestTime.toString("yyyy/M/d")});
    }
    sheet += "</sheetData></worksheet>";

    QZipWriter zip(path);
    zip.setCompressionPolicy(QZipWriter::AutoCompress);
    zip.addFile("[Content_Types].xml", QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></Types>"));
    zip.addFile("_rels/.rels", QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"));
    zip.addFile("xl/workbook.xml", QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"装箱单\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>"));
    zip.addFile("xl/_rels/workbook.xml.rels", QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>"));
    zip.addFile("xl/worksheets/sheet1.xml", sheet.toUtf8());
    zip.close();
    if (zip.status() != QZipWriter::NoError) {
        QMessageBox::critical(this, QStringLiteral("输出失败"), QStringLiteral("无法写入装箱单文件。"));
        return;
    }
    clearRecords();
    QMessageBox::information(this, QStringLiteral("输出成功"), QStringLiteral("装箱单已保存。"));
}

void MainWindow::loadPackingList()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("加载装箱单"), QString(),
        QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    QString error;
    const QVector<QStringList> rows = readSimpleXlsx(path, &error);
    if (!error.isEmpty() || rows.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("加载失败"),
            error.isEmpty() ? QStringLiteral("装箱单没有数据。") : error);
        return;
    }

    const QStringList headers = rows.first();
    auto column = [&headers](const QString &name) {
        for (int i = 0; i < headers.size(); ++i)
            if (headers.at(i).trimmed().compare(name, Qt::CaseInsensitive) == 0) return i;
        return -1;
    };
    const int skuCol = column(QStringLiteral("SKU"));
    int planCol = column(QStringLiteral("计划数量"));
    if (planCol < 0) planCol = column(QStringLiteral("数量"));
    const int fnskuCol = column(QStringLiteral("FNSKU"));
    int totalCol = column(QStringLiteral("实际总数量"));
    if (totalCol < 0) totalCol = column(QStringLiteral("总数量"));
    const int countCol = column(QStringLiteral("装箱数量"));
    const int boxCol = column(QStringLiteral("箱子编号"));
    const int timeCol = column(QStringLiteral("时间戳"));
    const int modeCol = column(QStringLiteral("装箱模式"));
    const int groupInfoCol = column(QStringLiteral("分仓组信息"));
    if (fnskuCol < 0 || countCol < 0 || boxCol < 0) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("必须包含 FNSKU、装箱数量、箱子编号三列；各列可以自由调整位置。"));
        return;
    }

    QVector<ScanRecord> loadedRecords;
    QStringList loadedBoxes;
    QVector<PackingPlanItem> loadedPlan;
    QMap<int, WarehouseGroup> loadedWarehouseGroups;
    QHash<QString, QString> loadedPositionForBox;
    QHash<QString, QString> loadedBoxForPosition;
    bool warehouseFile = false;
    QStringList unmatched, invalidRows, inconsistentRows;
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &cells = rows.at(r);
        if (modeCol >= 0 && cells.value(modeCol).trimmed() == QStringLiteral("分仓模式"))
            warehouseFile = true;
        const QString fnsku = cells.value(fnskuCol).trimmed().toUpper();
        if (planCol >= 0) {
            bool planOk = false;
            const int planned = cells.value(planCol).trimmed().toInt(&planOk);
            const QString planSku = cells.value(skuCol).trimmed();
            if (planOk && planned > 0 && !planSku.isEmpty() && !fnsku.isEmpty())
                loadedPlan.append({planSku, fnsku, planned});
        }
        if (fnsku.isEmpty()) continue;
        if (cells.value(countCol).trimmed().isEmpty() && cells.value(boxCol).trimmed().isEmpty())
            continue;
        const QStringList countTexts = cells.value(countCol).simplified().split(' ', Qt::SkipEmptyParts);
        const QStringList boxNumbers = cells.value(boxCol).simplified().split(' ', Qt::SkipEmptyParts);
        const QStringList groupTexts = groupInfoCol >= 0
            ? cells.value(groupInfoCol).simplified().split(' ', Qt::SkipEmptyParts)
            : QStringList();
        const bool rowWarehouse = (modeCol >= 0
            && cells.value(modeCol).trimmed() == QStringLiteral("分仓模式"))
            || !groupTexts.isEmpty();
        if (rowWarehouse)
            warehouseFile = true;
        QVector<int> counts;
        bool valid = !countTexts.isEmpty() && countTexts.size() == boxNumbers.size();
        int packedTotal = 0;
        for (const QString &text : countTexts) {
            bool ok = false;
            const int count = text.toInt(&ok);
            if (!ok || count <= 0) valid = false;
            counts.append(count);
            packedTotal += count;
        }
        QVector<int> rowGroupNumbers(boxNumbers.size(), 0);
        QVector<int> rowGroupSlots(boxNumbers.size(), 0);
        QHash<QString, QString> nextPositionForBox = loadedPositionForBox;
        QHash<QString, QString> nextBoxForPosition = loadedBoxForPosition;
        if (rowWarehouse) {
            if (groupTexts.size() != boxNumbers.size()) {
                valid = false;
            } else {
                for (int b = 0; b < boxNumbers.size(); ++b) {
                    const QRegularExpressionMatch match = QRegularExpression(
                        QStringLiteral("^第(\\d+)组-(\\d+)$")).match(groupTexts.at(b));
                    if (!match.hasMatch()) {
                        valid = false;
                        break;
                    }
                    const int groupNumber = match.captured(1).toInt();
                    const int groupSlot = match.captured(2).toInt();
                    const QString box = boxNumbers.at(b).trimmed();
                    const QString positionKey = QStringLiteral("%1:%2")
                        .arg(groupNumber).arg(groupSlot);
                    if (groupNumber <= 0 || groupSlot < 1 || groupSlot > 5 || box.isEmpty()
                        || (nextPositionForBox.contains(box)
                            && nextPositionForBox.value(box) != positionKey)
                        || (nextBoxForPosition.contains(positionKey)
                            && nextBoxForPosition.value(positionKey) != box)) {
                        valid = false;
                        break;
                    }
                    rowGroupNumbers[b] = groupNumber;
                    rowGroupSlots[b] = groupSlot;
                    nextPositionForBox.insert(box, positionKey);
                    nextBoxForPosition.insert(positionKey, box);
                }
            }
        }
        if (!valid) { invalidRows.append(QString::number(r + 1)); continue; }
        loadedPositionForBox = std::move(nextPositionForBox);
        loadedBoxForPosition = std::move(nextBoxForPosition);

        const ProductIndexRecord indexed = productIndex.value(fnsku);
        QString sku = !indexed.msku.trimmed().isEmpty()
            ? indexed.msku.trimmed() : indexed.sku.trimmed();
        if (sku.isEmpty()) sku = cells.value(skuCol).trimmed();
        if (indexed.fnsku.isEmpty() && !unmatched.contains(fnsku)) unmatched.append(fnsku);
        QDateTime scannedAt = QDateTime::fromString(
            cells.value(timeCol).trimmed(), QStringLiteral("yyyy/M/d HH:mm:ss"));
        if (!scannedAt.isValid())
            scannedAt = QDateTime::fromString(cells.value(timeCol).trimmed(), "yyyy/M/d");
        if (!scannedAt.isValid()) scannedAt = QDateTime::fromString(cells.value(timeCol).trimmed(), Qt::ISODate);
        if (!scannedAt.isValid()) scannedAt = QDateTime::currentDateTime();
        for (int b = 0; b < boxNumbers.size(); ++b) {
            const QString box = boxNumbers.at(b).trimmed();
            if (!loadedBoxes.contains(box)) loadedBoxes.append(box);
            const int groupNumber = rowGroupNumbers.value(b);
            const int groupSlot = rowGroupSlots.value(b);
            if (rowWarehouse) {
                WarehouseGroup &group = loadedWarehouseGroups[groupNumber];
                group.number = groupNumber;
                while (group.boxIds.size() < 5) group.boxIds.append(QString());
                group.boxIds[groupSlot - 1] = box;
            }
            for (int i = 0; i < counts.at(b); ++i) {
                ScanRecord record{fnsku, sku, indexed.title, box, scannedAt};
                record.packingMode = rowWarehouse
                    ? static_cast<int>(PackingMode::Warehouse) : static_cast<int>(PackingMode::Loose);
                record.warehouseGroup = groupNumber;
                record.warehouseSlot = groupSlot;
                loadedRecords.append(record);
            }
        }
        bool totalOk = false;
        const int declaredTotal = cells.value(totalCol).toInt(&totalOk);
        if (totalCol >= 0 && totalOk && declaredTotal != packedTotal)
            inconsistentRows.append(QString::number(r + 1));
    }
    if (loadedRecords.isEmpty() && loadedPlan.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
            QStringLiteral("文件中没有可加载的有效装箱记录或装箱计划，当前数据未作修改。"));
        return;
    }
    if (warehouseFile && !loadedRecords.isEmpty()) {
        QStringList boxesWithoutGroup;
        for (const ScanRecord &record : std::as_const(loadedRecords)) {
            if (record.warehouseGroup <= 0 || record.warehouseSlot < 1 || record.warehouseSlot > 5) {
                if (!boxesWithoutGroup.contains(record.box))
                    boxesWithoutGroup.append(record.box);
            }
        }
        if (!boxesWithoutGroup.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("分仓装箱单不完整"),
                QStringLiteral("以下箱号缺少有效的“分仓组信息”，无法恢复五箱校验关系：\n%1\n\n当前数据未作修改。")
                    .arg(boxesWithoutGroup.join(QStringLiteral("、"))));
            return;
        }
    }

    QVector<PackingPlanItem> nextPlan = packingPlan;
    if (planCol >= 0)
        nextPlan = loadedPlan;
    QVector<WarehouseGroup> nextWarehouseGroups;
    const auto loadedCountsForBox = [&loadedRecords](const QString &box) {
        QHash<QString, int> result;
        for (const ScanRecord &record : loadedRecords) {
            if (record.box == box)
                ++result[record.fnsku.trimmed().toUpper()];
        }
        return result;
    };
    if (warehouseFile) {
        for (ScanRecord &record : loadedRecords)
            record.packingMode = static_cast<int>(PackingMode::Warehouse);
        QStringList invalidGroups;
        for (WarehouseGroup group : loadedWarehouseGroups) {
            QSet<QString> uniqueBoxes;
            bool validGroup = group.boxIds.size() == 5;
            for (const QString &boxValue : std::as_const(group.boxIds)) {
                const QString box = boxValue.trimmed();
                if (box.isEmpty() || uniqueBoxes.contains(box)) {
                    validGroup = false;
                    break;
                }
                uniqueBoxes.insert(box);
            }
            if (!validGroup) {
                invalidGroups.append(QString::number(group.number));
                continue;
            }

            const QHash<QString, int> firstCounts = loadedCountsForBox(group.boxIds.first());
            bool fiveBoxesBalanced = !firstCounts.isEmpty();
            for (int slot = 1; slot < group.boxIds.size(); ++slot)
                fiveBoxesBalanced = fiveBoxesBalanced
                    && loadedCountsForBox(group.boxIds.at(slot)) == firstCounts;
            group.completed = fiveBoxesBalanced;
            nextWarehouseGroups.append(group);
        }
        if (!invalidGroups.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("分仓装箱单不完整"),
                QStringLiteral("以下分仓组没有完整且唯一的1—5号箱位：%1\n\n当前数据未作修改。")
                    .arg(invalidGroups.join(QStringLiteral("、"))));
            return;
        }
    }

    records = std::move(loadedRecords);
    boxes = std::move(loadedBoxes);
    packingPlan = std::move(nextPlan);
    warehouseGroups = std::move(nextWarehouseGroups);
    expandedWarehouseGroups.clear();
    collapsedWarehouseGroups.clear();
    packingMode = warehouseFile ? PackingMode::Warehouse : PackingMode::Loose;
    currentBoxIndex = boxes.isEmpty() ? -1 : 0;
    ui->label_6->clear();
    ui->label_wh_current->clear();
    {
        QSignalBlocker looseScanBlocker(ui->textEdit);
        QSignalBlocker warehouseScanBlocker(ui->textEdit_wh_scan);
        ui->textEdit->clear();
        ui->textEdit_wh_scan->clear();
    }
    ui->textEdit_2->clear();
    ui->textEdit_wh_box->clear();
    rebuildHistory();
    rebuildPackingPlan();
    if (!boxes.isEmpty() && packingMode == PackingMode::Loose) {
        activeBoxDisplay()->setPlainText(boxes.first());
        activeBoxDisplay()->setAlignment(Qt::AlignCenter);
    }
    updateModeUi();
    saveSessionState();
    QString result = QStringLiteral("已加载 %1 条扫码记录，%2 个箱号。")
        .arg(records.size()).arg(boxes.size());
    if (!unmatched.isEmpty()) result += QStringLiteral("\n索引表未找到 %1 个 FNSKU：%2")
        .arg(unmatched.size()).arg(unmatched.mid(0, 10).join(QStringLiteral("、")));
    if (!invalidRows.isEmpty()) result += QStringLiteral("\n装箱数量与箱号不对应，已跳过行：%1")
        .arg(invalidRows.join(QStringLiteral("、")));
    if (!inconsistentRows.isEmpty()) result += QStringLiteral("\n总数量与装箱数量合计不一致的行：%1")
        .arg(inconsistentRows.join(QStringLiteral("、")));
    QMessageBox::information(this, QStringLiteral("加载完成"), result);
    focusPackingInput();
}

void MainWindow::loadPackingListLegacy()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("加载装箱单"), QString(),
        QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty())
        return;
    QZipReader zip(path);
    const QByteArray xml = zip.fileData("xl/worksheets/sheet1.xml");
    if (xml.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("加载失败"), QStringLiteral("无法读取该装箱单。"));
        return;
    }

    clearRecords();
    QXmlStreamReader reader(xml);
    QStringList cells;
    QStringList headers;
    QString cellText;
    bool firstRow = true;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && (reader.name() == QLatin1String("t") || reader.name() == QLatin1String("v")))
            cellText = reader.readElementText();
        else if (reader.isEndElement() && reader.name() == QLatin1String("c")) {
            cells.append(cellText);
            cellText.clear();
        } else if (reader.isEndElement() && reader.name() == QLatin1String("row")) {
            if (firstRow) {
                firstRow = false;
                headers = cells;
            } else if (cells.size() >= 3) {
                if (headers.value(0) == QStringLiteral("SKU")) {
                    const QString sku = cells.value(0);
                    const QString fnsku = cells.value(1);
                    const QStringList packedCounts = cells.value(3).simplified().split(' ', Qt::SkipEmptyParts);
                    const QStringList boxNumbers = cells.value(4).simplified().split(' ', Qt::SkipEmptyParts);
                    QDateTime scannedAt = QDateTime::fromString(cells.value(5), "yyyy/M/d");
                    if (!scannedAt.isValid()) scannedAt = QDateTime::currentDateTime();
                    const QString title = productIndex.value(fnsku).title;
                    for (int b = 0; b < boxNumbers.size(); ++b) {
                        const QString box = boxNumbers.at(b);
                        const int count = qMax(1, packedCounts.value(b).toInt());
                        if (!boxes.contains(box)) boxes.append(box);
                        for (int i = 0; i < count; ++i)
                            records.append({fnsku, sku, title, box, scannedAt});
                    }
                } else {
                    const QString box = cells.at(0);
                    const QString fnsku = cells.at(1);
                    const int count = qMax(1, cells.at(2).toInt());
                    if (!boxes.contains(box)) boxes.append(box);
                    QDateTime scannedAt = QDateTime::fromString(cells.value(3), "yyyy-MM-dd HH:mm:ss");
                    if (!scannedAt.isValid()) scannedAt = QDateTime::currentDateTime();
                    for (int i = 0; i < count; ++i)
                        records.append({fnsku, QString(), QString(), box, scannedAt});
                }
            }
            cells.clear();
        }
    }
    if (reader.hasError()) {
        clearRecords();
        QMessageBox::critical(this, QStringLiteral("加载失败"), QStringLiteral("装箱单格式不正确。"));
        return;
    }
    rebuildHistory();
    rebuildPackingPlan();
    // Synchronize the current box with the boxes restored from the packing list.
    if (!boxes.isEmpty()) {
        currentBoxIndex = 0;
        activeBoxDisplay()->setPlainText(boxes.first());
        activeBoxDisplay()->setAlignment(Qt::AlignCenter);
    }
    saveSessionState();
    QMessageBox::information(this, QStringLiteral("加载完成"),
        QStringLiteral("已加载 %1 条扫码记录、%2 个箱号。")
            .arg(records.size()).arg(boxes.size()));
    focusPackingInput();
}

void MainWindow::on_pushButton_8_clicked()
{
    if (!syncingProducts.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("索引正在更新，请稍候。"));
        return;
    }
    const QString tokenPath = QDir(dataDirectory()).filePath("token.txt");
    QFile tokenFile(tokenPath);
    if (!tokenFile.exists()) {
        if (tokenFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            tokenFile.write("# 请将领星 ERP 请求头中的 auth-token 粘贴到此文件。\n");
            tokenFile.close();
        }
        QMessageBox::warning(this, QStringLiteral("缺少Token"),
            QStringLiteral("已创建Token文件，请填写后重新更新：\n%1").arg(tokenPath));
        return;
    }
    if (!tokenFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("缺少Token"),
            QStringLiteral("无法读取：\n%1").arg(tokenPath));
        return;
    }
    const QString token = QString::fromUtf8(tokenFile.readAll()).trimmed();
    if (token.isEmpty() || token.startsWith('#')) {
        QMessageBox::warning(this, QStringLiteral("缺少Token"),
            QStringLiteral("请在以下文件中填写领星 auth-token：\n%1").arg(tokenPath));
        return;
    }
    syncingProducts.clear();
    indexAuthToken = token;
    setIndexSyncUi(true, QStringLiteral("正在更新 0 条..."));
    requestIndexPage(0, 1);
}

void MainWindow::requestIndexPage(int offset, int sequence)
{
    QNetworkRequest request(QUrl("https://gw.lingxingerp.com/listing-api/api/product/showOnline"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=UTF-8");
    request.setRawHeader("accept", "application/json, text/plain, */*");
    request.setRawHeader("ak-client-type", "web");
    request.setRawHeader("ak-origin", "https://erp.lingxing.com");
    request.setRawHeader("referer", "https://erp.lingxing.com/");
    request.setRawHeader("auth-token", indexAuthToken.toUtf8());
    request.setRawHeader("x-ak-company-id", "901667755596284416");
    request.setRawHeader("x-ak-env-key", "SAAS-158");
    request.setRawHeader("x-ak-language", "zh");
    request.setRawHeader("x-ak-platform", "1");
    request.setRawHeader("x-ak-request-id", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
    request.setRawHeader("x-ak-request-source", "erp");
    request.setRawHeader("x-ak-uid", "11098176");
    request.setRawHeader("x-ak-version", "3.9.1.3.0.001");
    request.setRawHeader("x-ak-zid", "11031634");

    QJsonObject body{{"offset", offset}, {"length", 200}, {"search_field", "msku"},
        {"pvi_ids", ""}, {"exact_search", 0}, {"sids", "17622"}, {"status", ""},
        {"is_pair", ""}, {"fulfillment_channel_type", ""}, {"global_tag_ids", ""},
        {"req_time_sequence", QString("/listing-api/api/product/showOnline$$%1").arg(sequence)}};
    QNetworkReply *reply = networkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, offset, sequence] {
        const QByteArray bytes = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        auto finishWithError = [this](const QString &message) {
            syncingProducts.clear();
            indexAuthToken.clear();
            setIndexSyncUi(false);
            QMessageBox::critical(this, QStringLiteral("更新失败"), message);
        };
        if (httpStatus == 401 || httpStatus == 403) {
            finishWithError(QStringLiteral("auth-token 已过期，请登录领星后更新 token.txt。"));
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            finishWithError(QStringLiteral("接口响应不是有效JSON：%1").arg(error.errorString()));
            return;
        }
        const QJsonObject root = document.object();
        if (root.value("code").toInt() != 1) {
            finishWithError(QStringLiteral("领星接口返回错误：%1").arg(root.value("msg").toString()));
            return;
        }
        const QJsonObject data = root.value("data").toObject();
        const QJsonArray list = data.value("list").toArray();
        for (const QJsonValue &value : list) {
            const QJsonObject item = value.toObject();
            ProductIndexRecord record;
            record.fnsku = item.value("fnsku").toString().trimmed();
            record.msku = item.value("msku").toString();
            record.sku = item.value("local_sku").toString();
            if (record.sku.isEmpty()) record.sku = record.msku;
            record.asin = item.value("asin").toString();
            record.title = item.value("item_name").toString();
            record.seller = item.value("seller_name").toString();
            record.imageUrl = item.value("small_image_url").toString();
            if (!record.fnsku.isEmpty()) syncingProducts.append(record);
        }
        setIndexSyncUi(true, QStringLiteral("正在更新 %1 条...").arg(syncingProducts.size()));
        const int total = data.value("total").toInt();
        if (!list.isEmpty() && offset + list.size() < total) {
            requestIndexPage(offset + list.size(), sequence + 1);
            return;
        }
        productIndex.clear();
        for (const auto &record : syncingProducts) productIndex.insert(record.fnsku, record);
        updateIndexUi(productIndex.size());
        const QString path = QDir(dataDirectory()).filePath("fnsku_sku_index.xlsx");
        const bool saved = writeProductIndexXlsx(path);
        const int count = productIndex.size();
        syncingProducts.clear();
        indexAuthToken.clear();
        setIndexSyncUi(false);
        if (saved)
            QMessageBox::information(this, QStringLiteral("更新完成"),
                QStringLiteral("已建立 %1 条 FNSKU 索引。\n%2").arg(count).arg(path));
        else
            QMessageBox::warning(this, QStringLiteral("写入失败"), QStringLiteral("无法保存索引文件：%1").arg(path));
    });
}

bool MainWindow::writeProductIndexXlsx(const QString &path) const
{
    QString sheet = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                    "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
    auto addRow = [&sheet](int rowNumber, const QStringList &values) {
        sheet += QString("<row r=\"%1\">").arg(rowNumber);
        for (int i = 0; i < values.size(); ++i)
            sheet += QString("<c r=\"%1%2\" t=\"inlineStr\"><is><t>%3</t></is></c>")
                .arg(QChar('A' + i)).arg(rowNumber).arg(xmlEscape(values.at(i)));
        sheet += "</row>";
    };
    addRow(1, {"FNSKU", "SKU", "MSKU", "ASIN", QStringLiteral("商品标题"),
               QStringLiteral("店铺"), QStringLiteral("图片")});
    int rowNumber = 2;
    for (const auto &record : syncingProducts)
        addRow(rowNumber++, {record.fnsku, record.sku, record.msku, record.asin,
                             record.title, record.seller, record.imageUrl});
    sheet += "</sheetData></worksheet>";
    QZipWriter zip(path);
    zip.setCompressionPolicy(QZipWriter::AutoCompress);
    zip.addFile("[Content_Types].xml", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></Types>"));
    zip.addFile("_rels/.rels", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"));
    zip.addFile("xl/workbook.xml", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"FNSKU索引\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>"));
    zip.addFile("xl/_rels/workbook.xml.rels", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>"));
    zip.addFile("xl/worksheets/sheet1.xml", sheet.toUtf8());
    zip.close();
    return zip.status() == QZipWriter::NoError;
}

void MainWindow::loadProductIndexXlsx()
{
    QString path = QDir(dataDirectory()).filePath("fnsku_sku_index.xlsx");
    if (!QFileInfo::exists(path)) {
        writeProductIndexXlsx(path);
        return;
    }
    QZipReader zip(path);
    QXmlStreamReader reader(zip.fileData("xl/worksheets/sheet1.xml"));
    QStringList cells;
    QString cellText;
    bool header = true;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && (reader.name() == QLatin1String("t") || reader.name() == QLatin1String("v")))
            cellText = reader.readElementText();
        else if (reader.isEndElement() && reader.name() == QLatin1String("c")) {
            cells.append(cellText); cellText.clear();
        } else if (reader.isEndElement() && reader.name() == QLatin1String("row")) {
            if (header) header = false;
            else if (!cells.value(0).isEmpty()) {
                ProductIndexRecord r{cells.value(0), cells.value(1), cells.value(2), cells.value(3),
                                     cells.value(4), cells.value(5), cells.value(6)};
                productIndex.insert(r.fnsku, r);
            }
            cells.clear();
        }
    }
}

void MainWindow::on_pushButton_9_clicked()
{
    const QString path = QDir(dataDirectory()).filePath("fnsku_sku_index.xlsx");
    if (!QFileInfo::exists(path)) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("索引表尚未生成，请先点击“更新索引表”。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
