#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <cstdio>

#include "../mainwindow.h"

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "%s\n", utf8.constData());
    std::fflush(stderr);
    return 1;
}
}

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"));
    application.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    MainWindow window;
    window.resize(qEnvironmentVariableIntValue("WAREHOUSE_TEST_WIDTH") > 0
                      ? qEnvironmentVariableIntValue("WAREHOUSE_TEST_WIDTH") : 1400,
                  qEnvironmentVariableIntValue("WAREHOUSE_TEST_HEIGHT") > 0
                      ? qEnvironmentVariableIntValue("WAREHOUSE_TEST_HEIGHT") : 850);
    window.show();
    application.processEvents();

    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
    auto *boxInput = window.findChild<QLineEdit *>(QStringLiteral("lineEdit_wh_box"));
    auto *createBoxes = window.findChild<QPushButton *>(QStringLiteral("pushButton_wh_create"));
    auto *scanInput = window.findChild<QTextEdit *>(QStringLiteral("textEdit_wh_scan"));
    auto *destination = window.findChild<QLabel *>(QStringLiteral("label_wh_current"));
    auto *boxMap = window.findChild<QTextEdit *>(QStringLiteral("textEdit_wh_box"));
    auto *completeGroup = window.findChild<QPushButton *>(QStringLiteral("completeBoxButton"));
    auto *clearData = window.findChild<QPushButton *>(QStringLiteral("pushButton_wh_clear_data"));
    if (!tabs || !boxInput || !createBoxes || !scanInput || !destination || !boxMap
        || !completeGroup || !clearData)
        return fail(QStringLiteral("分仓页面控件不完整"));

    tabs->setCurrentIndex(1);
    application.processEvents();
    // Cancellation must not create a partial group.
    QTimer::singleShot(0, [] {
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
            dialog->reject();
    });
    createBoxes->click();
    if (!window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard")).isEmpty())
        return fail(QStringLiteral("取消创建仍然生成了分仓组"));

    bool validationPassed = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = QApplication::activeModalWidget();
        if (!dialog) return;
        auto *bulk = dialog->findChild<QLineEdit *>(QStringLiteral("groupBoxBulkInput"));
        auto *confirm = dialog->findChild<QPushButton *>(QStringLiteral("confirmCreateGroupButton"));
        auto *error = dialog->findChild<QLabel *>(QStringLiteral("groupCreateError"));
        if (!bulk || !confirm || !error) return;
        auto *first = dialog->findChild<QLineEdit *>(QStringLiteral("groupBoxInput1"));
        first->setFocus();
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(first, &enter);
        const bool scanEnterKeepsDialog = dialog->isVisible();
        dialog->grab().save(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("warehouse_create_group.png")));
        bulk->setText(QStringLiteral("BX1 BX1 BX3 BX4 BX5"));
        confirm->click();
        const bool duplicateRejected = !error->text().isEmpty() && dialog->isVisible();
        bulk->setText(QStringLiteral("BX1 BX2 BX3"));
        confirm->click();
        const bool partialRejected = !error->text().isEmpty() && dialog->isVisible();
        bulk->setText(QStringLiteral("BX1 BX2 BX3 BX4 BX5"));
        validationPassed = duplicateRejected && partialRejected && scanEnterKeepsDialog;
        confirm->click();
    });
    createBoxes->click();
    if (!validationPassed)
        return fail(QStringLiteral("箱号重复或少于五箱时没有拒绝创建"));
    const auto initialCards = window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard"));
    if (initialCards.size() != 1
        || initialCards.first()->findChildren<QWidget *>(QStringLiteral("boxCard")).size() != 5)
        return fail(QStringLiteral("创建后左侧没有完整展示一组五个空箱"));
    if (createBoxes->isEnabled())
        return fail(QStringLiteral("当前组未完成时仍可创建下一组"));
    application.processEvents();

    for (int slot = 1; slot <= 5; ++slot) {
        scanInput->setPlainText(QStringLiteral("X123456789"));
        application.processEvents();
        if (!destination->text().contains(QStringLiteral("放入 %1 号位").arg(slot)))
            return fail(QStringLiteral("同款首轮未分配到 %1 号位：%2")
                .arg(slot).arg(destination->text()));
    }

    // The sixth and seventh identical items start the next 1->5 round.
    scanInput->setPlainText(QStringLiteral("X123456789"));
    application.processEvents();
    if (!destination->text().contains(QStringLiteral("放入 1 号位")))
        return fail(QStringLiteral("同款第二轮没有回到1号位"));
    scanInput->setPlainText(QStringLiteral("X123456789"));
    application.processEvents();
    if (!destination->text().contains(QStringLiteral("放入 2 号位")))
        return fail(QStringLiteral("同款第二轮没有进入2号位"));

    // A different FNSKU owns an independent five-box rotation.
    scanInput->setPlainText(QStringLiteral("Y123456789"));
    application.processEvents();
    if (!destination->text().contains(QStringLiteral("放入 1 号位")))
        return fail(QStringLiteral("新款没有从1号位开始"));

    QTimer::singleShot(0, [] {
        if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) message->accept();
    });
    completeGroup->click();
    if (!completeGroup->isEnabled() || createBoxes->isEnabled())
        return fail(QStringLiteral("五箱数量不一致时仍然完成了分组"));

    // Returning to the first FNSKU must fill its current minimum (slot 3).
    scanInput->setPlainText(QStringLiteral("X123456789"));
    application.processEvents();
    if (!destination->text().contains(QStringLiteral("放入 3 号位")))
        return fail(QStringLiteral("原款没有继续补到3号位"));

    // Undo one X item from slot 2. The next X scan must repair slot 2,
    // proving the route is derived from real per-box counts, not a global cursor.
    QPushButton *slotTwoUndo = nullptr;
    for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (button->objectName() != QStringLiteral("removeButton"))
            continue;
        QWidget *boxCard = button->parentWidget() ? button->parentWidget()->parentWidget() : nullptr;
        QLabel *boxTitle = boxCard
            ? boxCard->findChild<QLabel *>(QStringLiteral("boxTitle"), Qt::FindDirectChildrenOnly)
            : nullptr;
        if (boxTitle && boxTitle->text().contains(QStringLiteral("2号位"))) {
            slotTwoUndo = button;
            break;
        }
    }
    if (!slotTwoUndo)
        return fail(QStringLiteral("没有找到2号位的撤销按钮"));
    slotTwoUndo->click();
    application.processEvents();
    scanInput->setPlainText(QStringLiteral("X123456789"));
    application.processEvents();
    if (!destination->text().contains(QStringLiteral("放入 2 号位")))
        return fail(QStringLiteral("撤销后没有自动补回2号位"));

    for (int slot = 1; slot <= 5; ++slot) {
        if (!boxMap->toPlainText().contains(QStringLiteral("%1号位  BX%1").arg(slot)))
            return fail(QStringLiteral("箱位映射缺少 %1 号位").arg(slot));
    }

    const QString screenshotPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                       .filePath(QStringLiteral("warehouse_flow_smoke.png"));
    if (!window.grab().save(screenshotPath))
        return fail(QStringLiteral("无法保存界面验证截图"));

    // Finish the two per-SKU rounds so every box has the same SKU/count map.
    for (int slot = 4; slot <= 5; ++slot) {
        scanInput->setPlainText(QStringLiteral("X123456789"));
        application.processEvents();
        if (!destination->text().contains(QStringLiteral("放入 %1 号位").arg(slot)))
            return fail(QStringLiteral("X款收尾未进入 %1 号位").arg(slot));
    }
    for (int slot = 2; slot <= 5; ++slot) {
        scanInput->setPlainText(QStringLiteral("Y123456789"));
        application.processEvents();
        if (!destination->text().contains(QStringLiteral("放入 %1 号位").arg(slot)))
            return fail(QStringLiteral("Y款收尾未进入 %1 号位").arg(slot));
    }

    QTimer::singleShot(0, [] {
        if (auto *messageBox = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            messageBox->accept();
    });
    completeGroup->click();
    application.processEvents();
    if (completeGroup->isEnabled() || !createBoxes->isEnabled())
        return fail(QStringLiteral("五箱一致后整组没有锁定或不能创建下一组"));
    auto *completedDetails = window.findChild<QWidget *>(QStringLiteral("warehouseGroupDetails"));
    if (!completedDetails || !completedDetails->isHidden())
        return fail(QStringLiteral("已完成组没有默认收起"));
    for (QPushButton *button : window.findChildren<QPushButton *>(QStringLiteral("removeButton"))) {
        if (button->isEnabled()) return fail(QStringLiteral("已锁组仍可撤销商品"));
    }

    const QString autosavePath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                     .filePath(QStringLiteral("扫码入库/packing_autosave.json"));
    QFile autosave(autosavePath);
    if (!autosave.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("没有生成自动保存文件：%1").arg(autosavePath));
    const QJsonObject root = QJsonDocument::fromJson(autosave.readAll()).object();
    if (root.value(QStringLiteral("formatVersion")).toInt() != 3)
        return fail(QStringLiteral("自动保存格式不是v3"));
    const QJsonArray records = root.value(QStringLiteral("records")).toArray();
    const QStringList expectedBoxes = {
        QStringLiteral("BX1"), QStringLiteral("BX2"), QStringLiteral("BX3"),
        QStringLiteral("BX4"), QStringLiteral("BX5"), QStringLiteral("BX1"),
        QStringLiteral("BX1"), QStringLiteral("BX3"), QStringLiteral("BX2"),
        QStringLiteral("BX4"), QStringLiteral("BX5"), QStringLiteral("BX2"),
        QStringLiteral("BX3"), QStringLiteral("BX4"), QStringLiteral("BX5")};
    if (records.size() != expectedBoxes.size())
        return fail(QStringLiteral("自动保存记录数错误：%1").arg(records.size()));
    for (int i = 0; i < records.size(); ++i) {
        if (records.at(i).toObject().value(QStringLiteral("box")).toString() != expectedBoxes.at(i))
            return fail(QStringLiteral("第 %1 条记录箱位错误").arg(i + 1));
    }

    const QJsonArray groups = root.value(QStringLiteral("warehouseGroups")).toArray();
    if (groups.size() != 1 || !groups.first().toObject().value(QStringLiteral("completed")).toBool())
        return fail(QStringLiteral("完成状态没有写入自动保存"));

    autosave.close();

    const QString workbook = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("warehouse_group_roundtrip.xlsx"));
    QTimer fileDialogs;
    QObject::connect(&fileDialogs, &QTimer::timeout, [&] {
        if (auto *fileDialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            fileDialog->selectFile(workbook);
            QMetaObject::invokeMethod(fileDialog, "accept", Qt::DirectConnection);
        } else if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            message->accept();
        }
    });
    fileDialogs.start(10);
    window.findChild<QPushButton *>(QStringLiteral("pushButton_wh_export_list"))->click();
    fileDialogs.stop();
    if (!QFile::exists(workbook)
        || !window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard")).isEmpty())
        return fail(QStringLiteral("完整五箱组没有正确导出并清理本次数据"));
    fileDialogs.start(10);
    window.findChild<QPushButton *>(QStringLiteral("pushButton_wh_load_list"))->click();
    fileDialogs.stop();
    QFile roundtrip(autosavePath);
    if (!roundtrip.open(QIODevice::ReadOnly)) return fail(QStringLiteral("导入后未保存数据"));
    const QJsonObject reloaded = QJsonDocument::fromJson(roundtrip.readAll()).object();
    if (reloaded.value(QStringLiteral("records")).toArray().size() != 15
        || reloaded.value(QStringLiteral("warehouseGroups")) != root.value(QStringLiteral("warehouseGroups")))
        return fail(QStringLiteral("导出再导入后分组、箱位或数量发生变化"));
    roundtrip.close();

    // Restore completed group, then create the next group without altering old boxes.
    QTimer::singleShot(0, [] {
        if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) message->accept();
    });
    {
        MainWindow restored;
        const auto restoredCards = restored.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard"));
        if (restoredCards.size() != 1
            || restoredCards.first()->findChildren<QWidget *>(QStringLiteral("boxCard")).size() != 5)
            return fail(QStringLiteral("重新打开没有恢复整组五箱"));
    }
    QTimer::singleShot(0, [&] {
        auto *dialog = QApplication::activeModalWidget();
        if (!dialog) return;
        auto *first = dialog->findChild<QLineEdit *>(QStringLiteral("groupBoxInput1"));
        const QString generated = first->text();
        first->setText(QStringLiteral("BX1"));
        auto *confirm = dialog->findChild<QPushButton *>(QStringLiteral("confirmCreateGroupButton"));
        confirm->click();
        if (dialog->findChild<QLabel *>(QStringLiteral("groupCreateError"))->text().isEmpty())
            qFatal("reused box ID was accepted");
        first->setText(generated);
        confirm->click();
    });
    createBoxes->click();
    application.processEvents();
    const auto twoGroups = window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard"));
    if (twoGroups.size() != 2)
        return fail(QStringLiteral("创建第二组没有按组展示"));
    for (QWidget *card : twoGroups) {
        if (card->findChildren<QWidget *>(QStringLiteral("boxCard")).size() != 5)
            return fail(QStringLiteral("分组明细不是每组固定五箱"));
    }
    for (int slot = 1; slot <= 5; ++slot) {
        scanInput->setPlainText(QStringLiteral("Z123456789"));
        application.processEvents();
    }
    application.sendPostedEvents();
    application.processEvents();
    for (QWidget *box : window.findChildren<QWidget *>(QStringLiteral("boxCard"))) {
        if (!box->isVisible()) continue;
        if (box->height() < 115)
            return fail(QStringLiteral("商品明细卡片被压缩导致文字裁切：%1px").arg(box->height()));
    }
    const QString groupedScreenshot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("warehouse_grouped_ui.png"));
    window.grab().save(groupedScreenshot);
    for (QWidget *card : window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard"))) {
        auto *toggle = card->findChild<QPushButton *>(QStringLiteral("toggleGroupDetails"));
        if (toggle && toggle->isChecked()) toggle->click();
    }
    application.processEvents();
    for (QWidget *card : window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard"))) {
        if (card->height() > 350)
            return fail(QStringLiteral("收起商品明细后分组卡片仍占用过多高度"));
    }
    window.grab().save(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("warehouse_group_overview.png")));
    QTimer::singleShot(0, [] {
        if (auto *messageBox = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (QAbstractButton *yes = messageBox->button(QMessageBox::Yes))
                yes->click();
        }
    });
    clearData->click();
    application.processEvents();

    QFile clearedAutosave(autosavePath);
    if (!clearedAutosave.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("清空后自动保存文件不可读"));
    const QJsonObject clearedRoot = QJsonDocument::fromJson(clearedAutosave.readAll()).object();
    if (!clearedRoot.value(QStringLiteral("records")).toArray().isEmpty()
        || !clearedRoot.value(QStringLiteral("boxes")).toArray().isEmpty()
        || !clearedRoot.value(QStringLiteral("warehouseGroups")).toArray().isEmpty())
        return fail(QStringLiteral("清空按钮没有清除扫码记录、箱号或分仓组"));

    if (!window.findChildren<QWidget *>(QStringLiteral("warehouseGroupCard")).isEmpty())
        return fail(QStringLiteral("清空后仍残留分组卡片"));

    // Legacy partial groups are completed atomically, retaining their slot mapping.
    clearedAutosave.close();
    QJsonObject legacy;
    legacy.insert(QStringLiteral("formatVersion"), 3);
    legacy.insert(QStringLiteral("packingMode"), 1);
    legacy.insert(QStringLiteral("boxes"), QJsonArray{QStringLiteral("OLD1"), QStringLiteral("OLD2")});
    QJsonObject partial;
    partial.insert(QStringLiteral("number"), 7);
    partial.insert(QStringLiteral("boxes"), QJsonArray{QStringLiteral("OLD1"), QStringLiteral("OLD2")});
    legacy.insert(QStringLiteral("warehouseGroups"), QJsonArray{partial});
    QFile legacyFile(autosavePath);
    if (!legacyFile.open(QIODevice::WriteOnly)) return fail(QStringLiteral("无法写入旧版测试数据"));
    legacyFile.write(QJsonDocument(legacy).toJson());
    legacyFile.close();
    {
        MainWindow restored;
        auto *create = restored.findChild<QPushButton *>(QStringLiteral("pushButton_wh_create"));
        bool oldBoxesPreserved = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = QApplication::activeModalWidget();
            if (!dialog) return;
            auto *first = dialog->findChild<QLineEdit *>(QStringLiteral("groupBoxInput1"));
            auto *second = dialog->findChild<QLineEdit *>(QStringLiteral("groupBoxInput2"));
            oldBoxesPreserved = first->text() == QStringLiteral("OLD1") && first->isReadOnly()
                && second->text() == QStringLiteral("OLD2") && second->isReadOnly();
            dialog->findChild<QPushButton *>(QStringLiteral("confirmCreateGroupButton"))->click();
        });
        create->click();
        if (!oldBoxesPreserved
            || restored.findChildren<QWidget *>(QStringLiteral("boxCard")).size() != 5)
            return fail(QStringLiteral("旧版半组恢复时箱位发生变化或没有补齐五箱"));
    }
    qInfo() << "warehouse_flow_smoke passed";
    std::puts("warehouse_flow_smoke passed (create/rotate/undo/lock/restore/xlsx/legacy)");
    return 0;
}
