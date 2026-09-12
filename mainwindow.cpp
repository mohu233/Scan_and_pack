#include "mainwindow.h"
#include "ui_mainwindow.h"

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
#include <QUuid>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <utility>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
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
    applyIndustrialStyle();
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktopPath.isEmpty())
        QDir::setCurrent(desktopPath);

    setWindowTitle(QStringLiteral("扫码入库"));
    ui->label_8->setText(QStringLiteral("历史扫码数据"));
    ui->label_7->setText(QStringLiteral("当前FNSKU"));
    ui->label_6->clear();
    ui->label_5->setText(QStringLiteral("扫码输入"));
    ui->label_10->setText(QStringLiteral("当前箱号"));
    ui->label_9->setText(QStringLiteral("添加箱号"));
    ui->pushButton_3->setText(QStringLiteral("上一个箱子"));
    ui->pushButton_4->setText(QStringLiteral("下一个箱子"));
    ui->pushButton_5->setText(QStringLiteral("创建箱号"));
    ui->textEdit_2->setReadOnly(true);
    QFont boxFont = ui->textEdit_2->font();
    boxFont.setPointSize(40);
    ui->textEdit_2->setFont(boxFont);
    ui->textEdit->setPlaceholderText(QStringLiteral("请扫描10位FNSKU"));
    ui->lineEdit->setPlaceholderText(QStringLiteral("输入箱号"));

    const auto oldChildren = ui->scrollAreaWidgetContents->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldChildren)
        delete child;
    historyLayout = new QVBoxLayout(ui->scrollAreaWidgetContents);
    historyLayout->setContentsMargins(8, 8, 8, 8);
    historyLayout->setSpacing(8);
    historyLayout->addStretch();

    const auto oldPlanChildren = ui->scrollAreaWidgetContents_2->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : oldPlanChildren)
        delete child;
    packingPlanLayout = new QVBoxLayout(ui->scrollAreaWidgetContents_2);
    packingPlanLayout->setContentsMargins(8, 8, 8, 8);
    packingPlanLayout->setSpacing(8);
    packingPlanLayout->addStretch();

    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::createBox);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::createBox);
    connect(ui->pushButton_3, &QPushButton::clicked, this, [this] { switchBox(-1); });
    connect(ui->pushButton_4, &QPushButton::clicked, this, [this] { switchBox(1); });
    connect(ui->textEdit, &QTextEdit::textChanged, this, &MainWindow::processScanInput);
    connect(ui->pushButton_6, &QPushButton::clicked, this, &MainWindow::exportPackingList);
    connect(ui->pushButton_7, &QPushButton::clicked, this, &MainWindow::loadPackingList);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::importPackingPlan);
    connect(ui->pushButton_12, &QPushButton::clicked, this, &MainWindow::exportPackingPlanTemplate);
    const QString tokenPath = QDir(dataDirectory()).filePath("token.txt");
    if (!QFileInfo::exists(tokenPath)) {
        QFile tokenFile(tokenPath);
        if (tokenFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            tokenFile.write("# 请将领星 ERP 请求头中的 auth-token 粘贴到此文件。\n");
            tokenFile.close();
        }
    }
    connect(ui->pushButton_2, &QPushButton::clicked, this, [this, tokenPath] {
        if (!QFileInfo::exists(tokenPath)) {
            QFile tokenFile(tokenPath);
            if (tokenFile.open(QIODevice::WriteOnly | QIODevice::Text))
                tokenFile.write("# 请将领星 ERP 请求头中的 auth-token 粘贴到此文件。\n");
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(tokenPath)))
            QMessageBox::warning(this, QStringLiteral("打开失败"),
                QStringLiteral("无法打开 token.txt，请检查系统是否有关联的文本编辑器。"));
    });
    networkManager = new QNetworkAccessManager(this);
    loadProductIndexXlsx();
    ui->label_13->setText(QString::number(productIndex.size()));
    ui->textEdit->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::applyIndustrialStyle()
{
    setMinimumSize(1120, 700);
    resize(1400, 850);
    ui->horizontalLayout->setStretch(0, 3);
    ui->horizontalLayout->setStretch(1, 5);
    ui->horizontalLayout->setStretch(2, 2);
    ui->horizontalLayout_8->setStretch(0, 1);
    ui->horizontalLayout_8->setStretch(1, 1);
    ui->openGLWidget->hide();

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#centralwidget { background-color:#eef2f6; color:#263238; }
        QLabel { font-family:"Microsoft YaHei UI"; font-size:14px; }
        QLabel#label_8, QLabel#label_18, QLabel#label_7, QLabel#label_5,
        QLabel#label_10, QLabel#label_9 {
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
        QTextEdit#textEdit {
            font-size:24px; font-weight:700; color:#17365d; min-height:68px;
        }
        QTextEdit#textEdit_2 {
            background:#e8f2ff; border:2px solid #1976d2; color:#0d47a1;
            font-size:40px; font-weight:800;
        }
        QLabel#label_6 {
            background:#ffffff; border:2px solid #70a6d8; border-radius:8px;
            color:#0d47a1; font-size:30px; font-weight:800; padding:14px;
            min-height:92px;
        }
        QPushButton {
            background:#ffffff; color:#174a7c; border:1px solid #8ca6bf;
            border-radius:6px; padding:8px 12px; min-height:24px;
            font-family:"Microsoft YaHei UI"; font-size:14px; font-weight:600;
        }
        QPushButton:hover { background:#e7f1fb; border-color:#1976d2; }
        QPushButton:pressed { background:#d4e7f8; }
        QPushButton#pushButton_5, QPushButton#pushButton_6 {
            background:#1565c0; color:white; border-color:#0d47a1; font-size:16px;
        }
        QPushButton#pushButton_5:hover, QPushButton#pushButton_6:hover { background:#0d57aa; }
        QPushButton#pushButton_3, QPushButton#pushButton_4 {
            background:#e8f2ff; color:#0d47a1; border-color:#70a6d8;
        }
        QLabel#label_12 { color:#5f6f7f; font-size:13px; }
        QLabel#label_13 { color:#16833a; font-size:18px; font-weight:800; }
        QLabel#label_11 { color:#718096; font-size:12px; padding-top:6px; }
    )"));

    ui->textEdit->setMinimumHeight(86);
    ui->lineEdit->setMinimumHeight(44);
    ui->textEdit_2->setMinimumHeight(100);
    ui->label_6->setAlignment(Qt::AlignCenter);
    ui->label_6->setWordWrap(true);
    ui->label_11->setAlignment(Qt::AlignCenter);
}

void MainWindow::createBox()
{
    const QString box = ui->lineEdit->text().trimmed();
    if (box.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入箱号。"));
        return;
    }
    int index = boxes.indexOf(box);
    if (index < 0) {
        boxes.append(box);
        index = boxes.size() - 1;
    }
    currentBoxIndex = index;
    ui->textEdit_2->setPlainText(box);
    ui->textEdit_2->setAlignment(Qt::AlignCenter);
    ui->lineEdit->clear();
    ui->textEdit->setFocus();
}

void MainWindow::switchBox(int offset)
{
    if (boxes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先创建箱号。"));
        return;
    }
    currentBoxIndex = qBound(0, currentBoxIndex + offset, boxes.size() - 1);
    ui->textEdit_2->setPlainText(boxes.at(currentBoxIndex));
    ui->textEdit_2->setAlignment(Qt::AlignCenter);
    ui->textEdit->setFocus();
}

void MainWindow::processScanInput()
{
    QString code = ui->textEdit->toPlainText().trimmed();
    code.remove('\r');
    code.remove('\n');
    if (code.size() < 10)
        return;
    ui->textEdit->blockSignals(true);
    ui->textEdit->clear();
    ui->textEdit->blockSignals(false);
    if (currentBoxIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("无法入库"), QStringLiteral("请先创建箱号，再进行扫码入库。"));
        return;
    }
    if (code.size() != 10) {
        QMessageBox::warning(this, QStringLiteral("扫码错误"), QStringLiteral("FNSKU必须为10个字符。"));
        return;
    }
    const ProductIndexRecord product = productIndex.value(code);
    const QString displaySku = !product.msku.trimmed().isEmpty()
        ? product.msku.trimmed() : product.sku.trimmed();
    addRecord({code, displaySku, product.title, boxes.at(currentBoxIndex),
               QDateTime::currentDateTime()});
    playPackedAnnouncement();
    if (displaySku.isEmpty()) {
        ui->label_6->setText(code + QStringLiteral("\n未找到SKU"));
    } else {
        ui->label_6->setText(QStringLiteral("%1\nSKU: %2").arg(code, displaySku));
        ui->label_6->setToolTip(product.title);
    }
}

void MainWindow::addRecord(const ScanRecord &record)
{
    records.append(record);
    rebuildHistory();
    rebuildPackingPlan();
    QTimer::singleShot(0, this, [this] {
        ui->scrollArea->verticalScrollBar()->setValue(0);
    });
}

void MainWindow::rebuildHistory()
{
    while (historyLayout->count() > 1) {
        QLayoutItem *item = historyLayout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
    QStringList boxOrder;
    for (int i = records.size() - 1; i >= 0; --i) {
        if (!boxOrder.contains(records.at(i).box))
            boxOrder.append(records.at(i).box);
    }

    for (const QString &box : boxOrder) {
        QVector<int> recordIndexes;
        for (int i = records.size() - 1; i >= 0; --i) {
            if (records.at(i).box == box)
                recordIndexes.append(i);
        }

        auto *boxCard = new QWidget(ui->scrollAreaWidgetContents);
        boxCard->setObjectName("boxCard");
        auto *boxLayout = new QVBoxLayout(boxCard);
        boxLayout->setContentsMargins(8, 7, 8, 7);
        boxLayout->setSpacing(5);

        auto *boxTitle = new QLabel(
            QStringLiteral("箱号：%1    已装箱：%2 件").arg(box).arg(recordIndexes.size()), boxCard);
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

            connect(remove, &QPushButton::clicked, this, [this, newestRecordIndex] {
                records.removeAt(newestRecordIndex);
                rebuildHistory();
                rebuildPackingPlan();
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
        historyLayout->insertWidget(historyLayout->count() - 1, boxCard);
    }
}

void MainWindow::clearRecords()
{
    records.clear();
    rebuildHistory();
    rebuildPackingPlan();
    ui->label_6->clear();
    ui->textEdit->clear();
    ui->textEdit_2->clear();
    boxes.clear();
    currentBoxIndex = -1;
}

void MainWindow::rebuildPackingPlan()
{
    if (!packingPlanLayout)
        return;
    while (packingPlanLayout->count() > 1) {
        QLayoutItem *item = packingPlanLayout->takeAt(0);
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

    for (int planIndex : displayOrder) {
        const PackingPlanItem &item = packingPlan.at(planIndex);
        const int scanned = scannedByFnsku.value(item.fnsku.toUpper());
        const int difference = scanned - item.plannedQuantity;
        const QString differenceText = difference > 0
            ? QStringLiteral("+%1").arg(difference)
            : QString::number(difference);
        const bool quantityMatches = scanned == item.plannedQuantity;
        auto *card = new QWidget(ui->scrollAreaWidgetContents_2);
        card->setObjectName(QStringLiteral("planCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(3);
        auto *skuLabel = new QLabel(item.sku, card);
        QFont font = skuLabel->font();
        font.setBold(true);
        skuLabel->setFont(font);
        skuLabel->setWordWrap(true);
        layout->addWidget(skuLabel);
        auto *differenceLabel = new QLabel(
            QStringLiteral("计划：%1　已装：%2　差额：%3")
                .arg(item.plannedQuantity).arg(scanned).arg(differenceText), card);
        differenceLabel->setStyleSheet(quantityMatches
            ? QStringLiteral("color:#16833a;") : QStringLiteral("color:#7a5b00;"));
        layout->addWidget(differenceLabel);
        const QString background = quantityMatches ? QStringLiteral("#ffffff") : QStringLiteral("#fff3b0");
        const QString border = quantityMatches ? QStringLiteral("#d9dee7") : QStringLiteral("#e0b400");
        card->setStyleSheet(QStringLiteral(
            "QWidget#planCard { background-color:%1; border:1px solid %2; border-radius:4px; } "
            "QWidget#planCard QLabel { background-color:transparent; border:none; }")
            .arg(background, border));
        packingPlanLayout->insertWidget(packingPlanLayout->count() - 1, card);
    }
}

void MainWindow::exportPackingPlanTemplate()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("输出装箱计划模板"),
        QStringLiteral("装箱计划模板.xlsx"), QStringLiteral("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) path += QStringLiteral(".xlsx");
    const QVector<QStringList> rows{{QStringLiteral("SKU"), QStringLiteral("数量")}};
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
    ui->scrollArea_2->verticalScrollBar()->setValue(0);

    QString result = QStringLiteral("已导入 %1 个 SKU，历史扫码数量已自动扣减。").arg(packingPlan.size());
    if (!missing.isEmpty()) result += QStringLiteral("\n%1 个 SKU 未在索引表中，已跳过：%2")
        .arg(missing.size()).arg(missing.mid(0, 10).join(QStringLiteral("、")));
    if (!invalidRows.isEmpty()) result += QStringLiteral("\n数量无效的行已跳过：%1").arg(invalidRows.join(QStringLiteral("、")));
    QMessageBox::information(this, QStringLiteral("导入完成"), result);
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
    const int fnskuCol = column(QStringLiteral("FNSKU"));
    const int totalCol = column(QStringLiteral("总数量"));
    const int countCol = column(QStringLiteral("装箱数量"));
    const int boxCol = column(QStringLiteral("箱子编号"));
    const int timeCol = column(QStringLiteral("时间戳"));
    if (fnskuCol < 0 || countCol < 0 || boxCol < 0) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("必须包含 FNSKU、装箱数量、箱子编号三列；各列可以自由调整位置。"));
        return;
    }

    clearRecords();
    QStringList unmatched, invalidRows, inconsistentRows;
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &cells = rows.at(r);
        const QString fnsku = cells.value(fnskuCol).trimmed().toUpper();
        if (fnsku.isEmpty()) continue;
        const QStringList countTexts = cells.value(countCol).simplified().split(' ', Qt::SkipEmptyParts);
        const QStringList boxNumbers = cells.value(boxCol).simplified().split(' ', Qt::SkipEmptyParts);
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
        if (!valid) { invalidRows.append(QString::number(r + 1)); continue; }

        const ProductIndexRecord indexed = productIndex.value(fnsku);
        QString sku = !indexed.msku.trimmed().isEmpty()
            ? indexed.msku.trimmed() : indexed.sku.trimmed();
        if (sku.isEmpty()) sku = cells.value(skuCol).trimmed();
        if (indexed.fnsku.isEmpty() && !unmatched.contains(fnsku)) unmatched.append(fnsku);
        QDateTime scannedAt = QDateTime::fromString(cells.value(timeCol).trimmed(), "yyyy/M/d");
        if (!scannedAt.isValid()) scannedAt = QDateTime::fromString(cells.value(timeCol).trimmed(), Qt::ISODate);
        if (!scannedAt.isValid()) scannedAt = QDateTime::currentDateTime();
        for (int b = 0; b < boxNumbers.size(); ++b) {
            const QString box = boxNumbers.at(b).trimmed();
            if (!boxes.contains(box)) boxes.append(box);
            for (int i = 0; i < counts.at(b); ++i)
                records.append({fnsku, sku, indexed.title, box, scannedAt});
        }
        bool totalOk = false;
        const int declaredTotal = cells.value(totalCol).toInt(&totalOk);
        if (totalCol >= 0 && totalOk && declaredTotal != packedTotal)
            inconsistentRows.append(QString::number(r + 1));
    }
    rebuildHistory();
    rebuildPackingPlan();
    if (!boxes.isEmpty()) {
        currentBoxIndex = 0;
        ui->textEdit_2->setPlainText(boxes.first());
        ui->textEdit_2->setAlignment(Qt::AlignCenter);
    }
    QString result = QStringLiteral("已加载 %1 条扫码记录，%2 个箱号。")
        .arg(records.size()).arg(boxes.size());
    if (!unmatched.isEmpty()) result += QStringLiteral("\n索引表未找到 %1 个 FNSKU：%2")
        .arg(unmatched.size()).arg(unmatched.mid(0, 10).join(QStringLiteral("、")));
    if (!invalidRows.isEmpty()) result += QStringLiteral("\n装箱数量与箱号不对应，已跳过行：%1")
        .arg(invalidRows.join(QStringLiteral("、")));
    if (!inconsistentRows.isEmpty()) result += QStringLiteral("\n总数量与装箱数量合计不一致的行：%1")
        .arg(inconsistentRows.join(QStringLiteral("、")));
    QMessageBox::information(this, QStringLiteral("加载完成"), result);
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
        ui->textEdit_2->setPlainText(boxes.first());
    }
    QMessageBox::information(this, QStringLiteral("加载完成"),
        QStringLiteral("已加载 %1 条扫码记录、%2 个箱号。")
            .arg(records.size()).arg(boxes.size()));
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
    ui->pushButton_8->setEnabled(false);
    ui->pushButton_8->setText(QStringLiteral("正在更新 0 条..."));
    ui->pushButton_8->setProperty("authToken", token);
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
    request.setRawHeader("auth-token", ui->pushButton_8->property("authToken").toString().toUtf8());
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
            ui->pushButton_8->setEnabled(true);
            ui->pushButton_8->setText(QStringLiteral("更新索引表"));
            ui->pushButton_8->setProperty("authToken", QVariant());
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
        ui->pushButton_8->setText(QStringLiteral("正在更新 %1 条...").arg(syncingProducts.size()));
        const int total = data.value("total").toInt();
        if (!list.isEmpty() && offset + list.size() < total) {
            requestIndexPage(offset + list.size(), sequence + 1);
            return;
        }
        productIndex.clear();
        for (const auto &record : syncingProducts) productIndex.insert(record.fnsku, record);
        ui->label_13->setText(QString::number(productIndex.size()));
        const QString path = QDir(dataDirectory()).filePath("fnsku_sku_index.xlsx");
        const bool saved = writeProductIndexXlsx(path);
        const int count = productIndex.size();
        syncingProducts.clear();
        ui->pushButton_8->setEnabled(true);
        ui->pushButton_8->setText(QStringLiteral("更新索引表"));
        ui->pushButton_8->setProperty("authToken", QVariant());
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
