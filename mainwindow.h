#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QVector>
#include <QHash>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QVBoxLayout;
class QNetworkAccessManager;

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
    };

    struct PackingPlanItem {
        QString sku;
        QString fnsku;
        int plannedQuantity = 0;
    };

    void createBox();
    void switchBox(int offset);
    void processScanInput();
    void addRecord(const ScanRecord &record);
    void rebuildHistory();
    void clearRecords();
    void exportPackingList();
    void loadPackingList();
    void loadPackingListLegacy();
    void exportPackingPlanTemplate();
    void importPackingPlan();
    void rebuildPackingPlan();
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
    QVBoxLayout *packingPlanLayout = nullptr;
    QVector<ScanRecord> records;
    QVector<PackingPlanItem> packingPlan;
    QStringList boxes;
    int currentBoxIndex = -1;
    QNetworkAccessManager *networkManager = nullptr;
    QVector<ProductIndexRecord> syncingProducts;
    QHash<QString, ProductIndexRecord> productIndex;
};
#endif // MAINWINDOW_H
