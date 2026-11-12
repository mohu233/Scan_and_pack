#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTabBar>
#include <QTimer>

namespace {
// Measure styled content after layout changes rather than freezing the height
// before font metrics, input padding and nested proxy tabs have settled.
class ContentSizedTabs : public QObject
{
public:
    ContentSizedTabs(QTabWidget *tabs, QList<int> expanded, int panePadding = 0)
        : QObject(tabs), m_tabs(tabs), m_expanded(expanded), m_padding(panePadding)
    {
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, [this]() { updateHeight(); });
        connect(tabs, &QTabWidget::currentChanged, this, [this]() { schedule(); });
        tabs->setMinimumHeight(0);
        tabs->setMaximumHeight(QWIDGETSIZE_MAX);
        tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tabs->installEventFilter(this);
        tabs->tabBar()->installEventFilter(this);
        for (QWidget *child : tabs->findChildren<QWidget *>()) child->installEventFilter(this);
        schedule();
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::LayoutRequest: case QEvent::StyleChange: case QEvent::FontChange:
        case QEvent::Resize: case QEvent::Show:
            schedule();
            break;
        default: break;
        }
        return false;
    }

private:
    void schedule() { if (!m_timer.isActive()) m_timer.start(0); }
    void updateHeight()
    {
        int content = 0;
        if (m_expanded.contains(m_tabs->currentIndex())) {
            QWidget *page = m_tabs->currentWidget();
            if (page && page->layout()) {
                QLayout *layout = page->layout();
                layout->activate();
                content = qMax(layout->totalSizeHint().height(), layout->totalMinimumSize().height());
                if (layout->hasHeightForWidth())
                    content = qMax(content, layout->totalHeightForWidth(qMax(100, m_tabs->width() - 2 * m_padding)));
            }
        }
        const int height = m_tabs->tabBar()->sizeHint().height() + content + 2 * m_padding + 4;
        if (m_tabs->minimumHeight() != height || m_tabs->maximumHeight() != height) {
            m_tabs->setFixedHeight(height);
            m_tabs->updateGeometry();
        }
    }
    QTabWidget *m_tabs;
    QList<int> m_expanded;
    int m_padding;
    QTimer m_timer;
};

QIcon navigationIcon(int index, const QColor &color)
{
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    switch (index) {
    case 0:
        painter.drawRoundedRect(QRectF(3, 3, 14, 14), 3, 3);
        painter.drawLine(6, 10, 14, 10); painter.drawLine(10, 6, 10, 14);
        break;
    case 1:
        painter.drawRoundedRect(QRectF(2, 3, 16, 14), 3, 3);
        painter.drawLine(2, 7, 18, 7); painter.drawPoint(5, 5);
        break;
    case 2:
        painter.drawEllipse(QRectF(2, 2, 16, 16));
        painter.drawEllipse(QRectF(6, 2, 8, 16)); painter.drawLine(2, 10, 18, 10);
        break;
    case 3:
        painter.drawPolyline(QPolygonF{QPointF(6, 5), QPointF(2, 10), QPointF(6, 15)});
        painter.drawPolyline(QPolygonF{QPointF(14, 5), QPointF(18, 10), QPointF(14, 15)});
        painter.drawLine(12, 3, 8, 17);
        break;
    case 4:
        painter.drawRoundedRect(QRectF(3, 4, 14, 12), 3, 3);
        painter.drawLine(7, 2, 7, 4); painter.drawLine(13, 2, 13, 4);
        painter.drawLine(7, 16, 7, 18); painter.drawLine(13, 16, 13, 18);
        painter.drawLine(8, 7, 13, 10); painter.drawLine(13, 10, 8, 13); painter.drawLine(8, 13, 8, 7);
        break;
    case 5:
        painter.drawRoundedRect(QRectF(3, 3, 6, 6), 1, 1);
        painter.drawRoundedRect(QRectF(12, 3, 6, 6), 1, 1);
        painter.drawRoundedRect(QRectF(3, 12, 6, 6), 1, 1);
        painter.drawRoundedRect(QRectF(12, 12, 6, 6), 1, 1);
        break;
    default:
        painter.drawEllipse(QRectF(2, 2, 16, 16));
        painter.drawPoint(10, 6); painter.drawLine(10, 9, 10, 14);
    }
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}

void addEmptyState(QTableWidget *table, const QString &text)
{
    auto label = new QLabel(text, table->viewport());
    label->setObjectName("tableEmptyState");
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setStyleSheet("color: #8894a7; font-size: 13px; padding: 24px; background: transparent;");
    auto layout = new QVBoxLayout(table->viewport());
    layout->addWidget(label);
    auto update = [table, label]() { label->setVisible(table->rowCount() == 0); };
    QObject::connect(table->model(), &QAbstractItemModel::rowsInserted, label, update);
    QObject::connect(table->model(), &QAbstractItemModel::rowsRemoved, label, update);
    QObject::connect(table->model(), &QAbstractItemModel::modelReset, label, update);
    update();
}
}

void MainWindow::applyInterfaceStyle()
{
    setStyleSheet(styleSheet() + R"(
        QWidget#centralwidget { background: #f4f6fb; color: #25324a; }
        QToolTip { color: #25324a; background: white; border: 1px solid #dce3ee; padding: 6px 10px; }
        QPushButton[variant="primary"] { background: #2563eb; color: white; border: 1px solid #2563eb; border-radius: 6px; padding: 0; font-size: 12px; font-weight: 600; }
        QPushButton[variant="primary"]:hover { background: #1d4ed8; border-color: #1d4ed8; }
        QPushButton[variant="primary"]:pressed { background: #1e40af; }
        QPushButton[variant="secondary"] { background: white; color: #526077; border: 1px solid #dce3ee; border-radius: 6px; padding: 0 10px 0 0; font-size: 12px; }
        QPushButton[variant="secondary"]:hover { background: #f1f6ff; color: #2563eb; border-color: #b8cef9; }
        QPushButton[variant="secondary"]::menu-indicator { image: url(:/ui/chevron.svg); width: 12px; height: 12px; subcontrol-position: right center; right: 5px; }
        QMenu { background: white; color: #334155; border: 1px solid #dce3ee; border-radius: 8px; padding: 6px; }
        QMenu::item { padding: 9px 24px 9px 14px; border-radius: 4px; font-size: 13px; }
        QMenu::item:selected { background: #edf4ff; color: #1d4ed8; }
        QMenu::separator { height: 1px; background: #edf0f5; margin: 5px 8px; }
        QScrollBar:horizontal { height: 8px; background: transparent; margin: 0; }
        QScrollBar::handle:horizontal { background: #c8ced6; border-radius: 4px; min-width: 36px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )");

    const QList<QPushButton *> navigation{ui->pushButton_7, ui->pushButton_6, ui->pushButton_4,
        ui->pushButton_2, ui->pushButton_3, ui->pushButton, ui->pushButton_11};
    const QList<QWidget *> pages{ui->page_2, ui->page, ui->page_3, ui->page_4, ui->page_5, ui->page_6, ui->page_7};
    const QString navigationStyle = R"(
        QPushButton { background: transparent; color: #657188; border: none; border-radius: 8px; margin: 3px 14px; padding: 10px 16px; text-align: left; font-size: 13px; }
        QPushButton:hover { background: #f4f7fc; color: #2563eb; }
        QPushButton:checked { background: #eaf1ff; color: #1d4ed8; font-weight: 600; }
        QPushButton:focus { border: 1px solid #b5ccfb; }
    )";
    for (int index = 0; index < navigation.size(); ++index) {
        auto button = navigation[index];
        button->setCheckable(true);
        button->setFixedHeight(46);
        button->setIcon(navigationIcon(index, QColor("#64748b")));
        button->setIconSize(QSize(19, 19));
        button->setStyleSheet(navigationStyle);
    }
    auto updateNavigation = [this, navigation, pages]() {
        for (int index = 0; index < navigation.size(); ++index) {
            const bool active = ui->stackedWidget->currentWidget() == pages[index];
            navigation[index]->setChecked(active);
            navigation[index]->setIcon(navigationIcon(index, QColor(active ? "#2563eb" : "#64748b")));
        }
    };
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, updateNavigation);
    updateNavigation();
    ui->label->setStyleSheet("color: #2058df; font-size: 23px; font-weight: 700; padding: 30px 24px 28px; background: white;");
    ui->label_34->setStyleSheet("color: #9aa5b5; font-size: 11px; padding: 16px 25px; background: transparent;");
    for (QWidget *page : pages) {
        page->setFocusPolicy(Qt::ClickFocus);
        if (page->layout()) {
            page->layout()->setContentsMargins(24, 20, 24, 24);
            page->layout()->setSpacing(16);
        }
    }
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this]() {
        ui->stackedWidget->currentWidget()->setFocus(Qt::OtherFocusReason);
    });
    ui->stackedWidget->currentWidget()->setFocus(Qt::OtherFocusReason);
    const QString titleStyle = "color: #18243b; font-size: 22px; font-weight: 700; padding: 3px 0 8px; background: transparent; border: none;";
    for (QLabel *title : {ui->label_3, ui->label_2, ui->label_4, ui->label_5, ui->label_6, ui->label_19})
        title->setStyleSheet(titleStyle);

    const QString tableStyle = R"(
        QTableWidget { background: white; alternate-background-color: #fafbfe; color: #344159; border: 1px solid #e2e7f0; border-radius: 9px; outline: none; font-size: 12px; selection-background-color: #edf4ff; selection-color: #1d4ed8; }
        QHeaderView::section { background: #f8faff; color: #758199; border: none; border-bottom: 1px solid #e4eaf4; padding: 8px 6px; font-size: 12px; font-weight: 500; }
        QTableWidget::item { border: none; border-bottom: 1px solid #f0f3f8; padding: 8px; }
        QTableWidget::item:selected { background: #edf4ff; color: #1d4ed8; }
        QTableWidget::item:focus { border: none; }
        QTableWidget::indicator { width: 15px; height: 15px; border: 1px solid #cbd5e1; border-radius: 4px; background: white; }
        QTableWidget::indicator:checked { image: url(:/ui/check.svg); background: #2563eb; border-color: #2563eb; }
    )";
    for (QTableWidget *table : {ui->tableWidget, rightTable, ui->tableWidget_2, ui->tableWidget_3}) {
        table->setStyleSheet(tableStyle);
        table->setShowGrid(false);
        table->setFrameShape(QFrame::NoFrame);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setWordWrap(false);
        table->setTextElideMode(Qt::ElideRight);
        table->verticalHeader()->setDefaultSectionSize(52);
        table->horizontalHeader()->setFixedHeight(44);
        table->horizontalHeader()->setMinimumSectionSize(42);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setVerticalScrollBarPolicy(table == rightTable ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }
    rightTable->setObjectName("environmentActionsTable");
    rightTable->setStyleSheet(tableStyle + "QTableWidget#environmentActionsTable::item { padding: 0; }");
    auto header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    for (const auto &column : {QPair<int, int>(0, 44), {1, 56}, {3, 126}, {4, 106}, {5, 142}}) {
        header->setSectionResizeMode(column.first, QHeaderView::Fixed);
        ui->tableWidget->setColumnWidth(column.first, column.second);
    }
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(6, QHeaderView::Stretch);
    rightTable->setFixedWidth(156);
    rightTable->setColumnWidth(0, 154);
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        for (int column = 0; column < ui->tableWidget->columnCount(); ++column) {
            if (auto item = ui->tableWidget->item(row, column)) item->setToolTip(item->text());
        }
    }
    ui->tableWidget_2->setColumnCount(6);
    ui->tableWidget_2->setHorizontalHeaderLabels({"序号", "分组", "环境名称", "代理类型", "出口 IP", "调试端口"});
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->tableWidget, &QTableWidget::currentCellChanged, rightTable, [this](int row) {
        if (row >= 0) rightTable->selectRow(row);
    });
    addEmptyState(ui->tableWidget_2, QString::fromUtf8("暂无运行中的浏览器\n打开环境后，连接信息会显示在这里"));
    addEmptyState(ui->tableWidget_3, QString::fromUtf8("暂无模块\n选择程序包文件夹，添加第一个模块"));

    // QGridLayout's default zero spacing caused rows to touch. Give every
    // field its own minimum height and allow the tab to grow to fit all rows.
    ui->gridLayout_4->setHorizontalSpacing(16);
    ui->gridLayout_4->setVerticalSpacing(12);
    ui->gridLayout_4->setColumnStretch(0, 0);
    ui->gridLayout_4->setColumnStretch(1, 1);
    ui->gridLayout_4->setColumnStretch(2, 0);
    ui->gridLayout_5->setContentsMargins(14, 16, 14, 12);
    ui->gridLayout_6->setContentsMargins(26, 24, 26, 24);
    ui->formLayout_2->setVerticalSpacing(18);
    ui->formLayout_2->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for (QLineEdit *input : ui->page_2->findChildren<QLineEdit *>()) {
        if (qobject_cast<QComboBox *>(input->parentWidget())) {
            input->setMinimumHeight(0);
            input->setStyleSheet("QLineEdit { border: none; padding: 0; min-height: 0; background: transparent; font-size: 13px; }");
        } else input->setMinimumHeight(38);
    }
    for (QComboBox *input : ui->page_2->findChildren<QComboBox *>()) {
        input->setMinimumHeight(38);
        input->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        input->setMinimumContentsLength(6);
        input->setStyleSheet(input->styleSheet() + "QComboBox::down-arrow { image: url(:/ui/chevron.svg); width: 14px; height: 14px; border: none; }");
    }
    ui->horizontalLayout_5->setStretch(0, 4);
    ui->horizontalLayout_5->setStretch(1, 1);
    ui->horizontalLayout_7->setStretch(0, 1);
    ui->horizontalLayout_7->setStretch(1, 4);
    lineEditGroup->setPlaceholderText(QString::fromUtf8("分组名称"));
    lineEditTag->setPlaceholderText(QString::fromUtf8("标签（选填）"));
    ui->label_17->setText(QString::fromUtf8("IP 查询服务"));
    ui->pushButton_8->setText(QString::fromUtf8("创建环境"));
    ui->pushButton_5->setText(QString::fromUtf8("重置表单"));
    ui->pushButton_10->setStyleSheet("");
    ui->pushButton_10->setProperty("variant", "primary");
    ui->pushButton_10->setMinimumWidth(128);
    ui->tabWidget->setStyleSheet(ui->tabWidget->styleSheet()
        + "QTabWidget#tabWidget::pane { top: 0; padding: 10px; }");
    for (QTabWidget *tabs : {ui->tabWidget_2, ui->tabWidget_3, ui->tabWidget_4, ui->tabWidget_5,
             ui->tabWidget_6, ui->tabWidget_7, ui->tabWidget_8, ui->tabWidget_9, proxyInputTabs})
        tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
    new ContentSizedTabs(proxyInputTabs, {0, 1});
    new ContentSizedTabs(ui->tabWidget, {0, 1}, 12);
    new ContentSizedTabs(ui->tabWidget_3, {});
    new ContentSizedTabs(ui->tabWidget_4, {2});
    new ContentSizedTabs(ui->tabWidget_5, {2});
    new ContentSizedTabs(ui->tabWidget_6, {1});
    new ContentSizedTabs(ui->tabWidget_7, {2});
    new ContentSizedTabs(ui->tabWidget_8, {1, 2});
    new ContentSizedTabs(ui->tabWidget_9, {1});
    new ContentSizedTabs(ui->tabWidget_2, {1});
}
