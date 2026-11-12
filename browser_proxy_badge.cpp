#include "browser_proxy_badge.h"
#include "proxy_exit_ip.h"

#include <QLabel>
#include <QFontMetrics>
#include <QImage>
#include <QLibrary>
#include <QNetworkAccessManager>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QVector>

namespace {
class BadgeLabel : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        // Render an opaque rounded badge into the native child control's image.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(property("badgeBorder").value<QColor>(), 1));
        painter.setBrush(property("badgeBackground").value<QColor>());
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
        const QPixmap icon = property("badgeIcon").value<QPixmap>();
        if (!icon.isNull()) painter.drawPixmap(9, (height() - 14) / 2, 14, 14, icon);
        painter.end();
        QLabel::paintEvent(event);
    }
};
}

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
constexpr wchar_t badgeWindowClass[] = L"ProfilePowerProxyBadgeChild";

struct BadgeSurface {
    QImage image;
};

void paintBadgeSurface(HDC dc, const BadgeSurface *surface)
{
    if (!surface || surface->image.isNull()) return;
    const QImage &image = surface->image;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width();
    info.bmiHeader.biHeight = -image.height();
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, 0, 0, image.width(), image.height(), 0, 0, 0,
                     image.height(), image.constBits(), &info, DIB_RGB_COLORS);
}

LRESULT CALLBACK badgeWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new BadgeSurface));
    }
    auto surface = reinterpret_cast<BadgeSurface *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        const HDC dc = BeginPaint(window, &paint);
        paintBadgeSurface(dc, surface);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_PRINTCLIENT:
        paintBadgeSurface(reinterpret_cast<HDC>(wParam), surface);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        delete surface;
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

HWND createBadgeChild(HWND parent)
{
    static const ATOM registered = []() {
        WNDCLASSW definition = {};
        definition.lpfnWndProc = badgeWindowProcedure;
        definition.hInstance = GetModuleHandleW(nullptr);
        definition.lpszClassName = badgeWindowClass;
        return RegisterClassW(&definition);
    }();
    if (!registered) return nullptr;
    // Create as a child from the outset. Reparenting a Qt top-level window can
    // change DPI awareness and would retain independent popup-window behavior.
    return CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, badgeWindowClass,
        L"", WS_CHILD | WS_CLIPSIBLINGS | WS_DISABLED, 0, 0, 1, 1, parent, nullptr,
        GetModuleHandleW(nullptr), nullptr);
}

bool isBadgeChild(HWND window, HWND parent)
{
    wchar_t className[64] = {};
    return IsWindow(window) && GetParent(window) == parent
        && GetClassNameW(window, className, 64) && QString::fromWCharArray(className) == QString::fromWCharArray(badgeWindowClass);
}

struct WindowSearch {
    DWORD processId;
    QVector<quintptr> windows;
};

BOOL CALLBACK collectBrowserWindows(HWND window, LPARAM argument)
{
    auto *search = reinterpret_cast<WindowSearch *>(argument);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != search->processId || !IsWindowVisible(window)) return TRUE;
    wchar_t className[64] = {};
    GetClassNameW(window, className, 64);
    if (QString::fromWCharArray(className) == "Chrome_WidgetWin_1")
        search->windows.append(reinterpret_cast<quintptr>(window));
    return TRUE;
}

qreal windowScale(HWND window)
{
    using GetDpi = UINT(WINAPI *)(HWND);
    static const auto getDpi = reinterpret_cast<GetDpi>(QLibrary::resolve("user32", "GetDpiForWindow"));
    return getDpi ? qMax(96u, getDpi(window)) / 96.0 : 1.0;
}
}
#endif

BrowserProxyBadge::BrowserProxyBadge(qint64 browserProcessId, QNetworkAccessManager *proxyNetwork,
                                   QObject *parent, const QUrl &lookupUrl)
    : QObject(parent), m_processId(browserProcessId), m_lookup(new ProxyExitIpLookup(proxyNetwork, this, lookupUrl))
{
    setObjectName("browserProxyBadge");
    connect(m_lookup, &ProxyExitIpLookup::finished, this, [this](const QString &ip) {
        m_exitIp = ip;
        m_status = ip.isEmpty() ? Status::Unconfirmed : Status::Verified;
        updateLabels();
        emit exitIpChanged(ip);
    });
    connect(&m_windowTimer, &QTimer::timeout, this, [this]() { syncWindows(); });
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() { refresh(); });
    m_windowTimer.start(250);
    m_refreshTimer.start(60000);
    refresh();
}

BrowserProxyBadge::~BrowserProxyBadge()
{
    m_windowTimer.stop();
    m_refreshTimer.stop();
#ifdef Q_OS_WIN
    for (auto it = m_childWindows.cbegin(); it != m_childWindows.cend(); ++it) {
        const HWND child = reinterpret_cast<HWND>(it.value());
        if (isBadgeChild(child, reinterpret_cast<HWND>(it.key()))) DestroyWindow(child);
    }
#endif
    qDeleteAll(m_labels);
}

QString BrowserProxyBadge::displayText() const
{
    const QString name = m_environmentName.isEmpty() ? QString::fromUtf8("代理") : m_environmentName;
    return name + " | " + addressText();
}

QString BrowserProxyBadge::addressText() const
{
    if (m_status == Status::Verified) return m_exitIp;
    if (m_status == Status::Unconfirmed) return QString::fromUtf8("IP 未确认");
    return QString::fromUtf8("IP 检测中…");
}

void BrowserProxyBadge::setEnvironmentName(const QString &name)
{
    m_environmentName = name.simplified();
    updateLabels();
}

void BrowserProxyBadge::refresh()
{
    m_lookup->refresh();
}

QLabel *BrowserProxyBadge::createLabel() const
{
    auto label = new BadgeLabel;
    label->setObjectName("proxyExitIpLabel");
    label->setTextFormat(Qt::PlainText);
    label->setAccessibleName(QString::fromUtf8("代理出口 IP"));
    label->setProperty("badgeIcon", QPixmap(":/branding/profilepower.png"));
    label->setAttribute(Qt::WA_DontShowOnScreen);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setAttribute(Qt::WA_TranslucentBackground);
    label->setFocusPolicy(Qt::NoFocus);
    styleLabel(label);
    return label;
}

void BrowserProxyBadge::styleLabel(QLabel *label) const
{
    const QString color = m_status == Status::Unconfirmed ? "#92400e" : "#172033";
    label->setProperty("badgeBackground", QColor("#ffffff"));
    label->setProperty("badgeBorder", QColor("#e2e8f0"));
    label->setStyleSheet("QLabel { color: " + color + "; background: transparent; border: none; padding: 4px 10px 4px 29px; font-family: 'Microsoft YaHei UI'; font-size: 12px; font-weight: 400; }");
    label->setAccessibleDescription(displayText());
    label->ensurePolished();
    fitLabel(label, 440);
}

void BrowserProxyBadge::fitLabel(QLabel *label, int availableWidth) const
{
    const QFontMetrics metrics(label->font());
    const QString name = m_environmentName.isEmpty() ? QString::fromUtf8("代理") : m_environmentName;
    const QString suffix = " | " + addressText();
    // Only the environment name is shortened; always preserve the complete IP.
    const int nameWidth = qMin(170, availableWidth - metrics.horizontalAdvance(suffix) - 42);
    label->setText(nameWidth >= metrics.horizontalAdvance(QString::fromUtf8("…"))
        ? metrics.elidedText(name, Qt::ElideRight, nameWidth) + suffix : addressText());
    label->adjustSize();
}

void BrowserProxyBadge::updateLabels()
{
    for (QLabel *label : m_labels) styleLabel(label);
    syncWindows();
}

void BrowserProxyBadge::syncWindows()
{
#ifdef Q_OS_WIN
    if (m_processId <= 0) return;
    WindowSearch search{static_cast<DWORD>(m_processId), {}};
    EnumWindows(collectBrowserWindows, reinterpret_cast<LPARAM>(&search));
    const QSet<quintptr> current(search.windows.cbegin(), search.windows.cend());
    for (auto it = m_labels.begin(); it != m_labels.end();) {
        if (!current.contains(it.key())) {
            const HWND child = reinterpret_cast<HWND>(m_childWindows.take(it.key()));
            if (isBadgeChild(child, reinterpret_cast<HWND>(it.key()))) DestroyWindow(child);
            delete it.value();
            it = m_labels.erase(it);
        }
        else ++it;
    }
    for (quintptr id : search.windows) {
        const HWND window = reinterpret_cast<HWND>(id);
        QLabel *label = m_labels.value(id);
        if (!label) {
            label = createLabel();
            m_labels.insert(id, label);
        }
        HWND child = reinterpret_cast<HWND>(m_childWindows.value(id));
        if (!isBadgeChild(child, window)) {
            child = createBadgeChild(window);
            if (!child) continue;
            m_childWindows.insert(id, reinterpret_cast<quintptr>(child));
        }
        // Child coordinates and clipping belong to Chrome, not the desktop.
        // The OS moves/hides/occludes it together with its parent immediately.
        RECT bounds;
        if (IsIconic(window) || !GetClientRect(window, &bounds)) { ShowWindow(child, SW_HIDE); continue; }
        const qreal scale = windowScale(window);
        const int availableWidth = qMin(440, static_cast<int>((bounds.right - bounds.left) / scale) - 168);
        fitLabel(label, availableWidth);
        if (label->sizeHint().width() > availableWidth) { ShowWindow(child, SW_HIDE); continue; }
        const int width = qRound(label->sizeHint().width() * scale);
        const int height = qRound(label->sizeHint().height() * scale);
        const int x = qMax(qRound(8 * scale), static_cast<int>(bounds.right) - qRound(152 * scale) - width);
        const int y = qRound((IsZoomed(window) ? 4 : 8) * scale);
        auto surface = reinterpret_cast<BadgeSurface *>(GetWindowLongPtrW(child, GWLP_USERDATA));
        if (!surface) continue;
        const bool changed = label->property("nativeText").toString() != label->text()
            || surface->image.size() != QSize(width, height);
        if (changed) {
            surface->image = QImage(QSize(width, height), QImage::Format_RGB32);
            surface->image.setDevicePixelRatio(scale);
            surface->image.fill(Qt::white);
            label->render(&surface->image);
            label->setProperty("nativeText", label->text());
            HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, qRound(24 * scale), qRound(24 * scale));
            if (!SetWindowRgn(child, region, FALSE)) DeleteObject(region);
            SetWindowTextW(child, reinterpret_cast<LPCWSTR>(displayText().utf16()));
        }
        SetWindowPos(child, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (changed) InvalidateRect(child, nullptr, FALSE);
    }
#endif
}
