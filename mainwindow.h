#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QVector>
#include <QHash>
#include <QSet>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QVBoxLayout;
class QNetworkAccessManager;
class QWidget;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    struct ProductIndexRecord {
        QString fnsku;
        QString sku;
        QString msku;
        QString asin;
        QString title;
        QString seller;
        QString imageUrl;
    };

    struct ScanRecord {
        QString fnsku;
        QString sku;
        QString title;
        QString box;
        QDateTime scannedAt;
        int packingMode = 0;
        int warehouseGroup = 0;
        int warehouseSlot = 0;
    };

    struct PackingPlanItem {
        QString sku;
        QString fnsku;
        int plannedQuantity = 0;
    };

    enum class PackingMode {
        Loose = 0,
        Warehouse = 1
    };

    struct WarehouseGroup {
        int number = 0;
        QStringList boxIds;
        bool completed = false;
    };

    void createBox();
    void createWarehouseGroup();
    void focusPackingInput();
    void switchBox(int offset);
    void processScanInput();
    void addRecord(const ScanRecord &record);
    void rebuildHistory();
    void clearRecords();
    void confirmClearPackingData();
    void exportPackingList();
    void exportUnifiedPackingList();
    void importUnifiedPackingPlan();
    void loadPackingList();
    void loadPackingListLegacy();
    void exportPackingPlanTemplate();
    void importPackingPlan();
    void rebuildPackingPlan();
    void showPackingLocations(const QString &sku, const QString &fnsku);
    void saveSessionState();
    void restoreSessionState();
    void setPackingMode(int index);
    void updateModeUi();
    void completeWarehouseGroup();
    WarehouseGroup *warehouseGroupForBox(const QString &box);
    const WarehouseGroup *warehouseGroupForBox(const QString &box) const;
    WarehouseGroup *openWarehouseGroup();
    const WarehouseGroup *openWarehouseGroup() const;
    QHash<QString, int> countsForBox(const QString &box) const;
    bool isWarehouseBoxLocked(const QString &box) const;
    bool warehouseGroupComplete(const WarehouseGroup &group) const;
    QString warehouseMismatchText(const WarehouseGroup &group) const;
    bool validateWarehousePlan(const QVector<PackingPlanItem> &plan, QString *message) const;
    QVBoxLayout *activeHistoryLayout() const;
    QVBoxLayout *activePackingPlanLayout() const;
    QWidget *activeHistoryContents() const;
    QWidget *activePackingPlanContents() const;
    QScrollArea *activeHistoryScrollArea() const;
    QScrollArea *activePackingPlanScrollArea() const;
    QTextEdit *activeScanInput() const;
    QTextEdit *activeBoxDisplay() const;
    QLineEdit *activeBoxInput() const;
    QLabel *activeCurrentScanLabel() const;
    void updateIndexUi(int count);
    void setIndexSyncUi(bool syncing, const QString &text = QString());
    void applyIndustrialStyle();
    void requestIndexPage(int offset, int sequence);
    bool writeProductIndexXlsx(const QString &path) const;
    void loadProductIndexXlsx();

private slots:
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();

private:
    Ui::MainWindow *ui;
    QVBoxLayout *historyLayout = nullptr;
    QVBoxLayout *warehouseHistoryLayout = nullptr;
    QVBoxLayout *packingPlanLayout = nullptr;
    QVBoxLayout *warehousePackingPlanLayout = nullptr;
    QVector<ScanRecord> records;
    QVector<PackingPlanItem> packingPlan;
    QStringList boxes;
    int currentBoxIndex = -1;
    QNetworkAccessManager *networkManager = nullptr;
    QVector<ProductIndexRecord> syncingProducts;
    QHash<QString, ProductIndexRecord> productIndex;
    QString indexAuthToken;
    PackingMode packingMode = PackingMode::Loose;
    QVector<WarehouseGroup> warehouseGroups;
    QSet<int> expandedWarehouseGroups;
    QSet<int> collapsedWarehouseGroups;
    bool restoringSession = false;
};
#endif // MAINWINDOW_H
