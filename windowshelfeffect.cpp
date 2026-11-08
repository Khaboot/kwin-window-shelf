#include "windowshelfeffect.h"

#include "core/output.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "input.h"
#include "input_event.h"
#include "pointer_input.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QAction>
#include <QKeySequence>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace KWin
{

static constexpr int PanelWidth = 272;
static constexpr int PreviewMaxWidth = 240;
static constexpr int PreviewMaxHeight = 156;
static constexpr int PreviewSpacing = 8;
static constexpr int PanelMargin = 12;
static constexpr int PreviewShadowOffset = 3;
static constexpr qreal PreviewHoverScale = 1.08;

class WindowShelfEffect::AlwaysVisibleInputFilter final : public InputEventFilter
{
public:
    explicit AlwaysVisibleInputFilter(WindowShelfEffect *effect)
        : InputEventFilter(InputFilterOrder::Effects)
        , m_effect(effect)
    {
    }

    bool pointerMotion(PointerMotionEvent *event) override
    {
        if (m_effect->m_visible && m_effect->m_mode == QLatin1String("always")) {
            m_effect->updatePointerOverPanel(event->position.toPoint());
        }
        return false;
    }

    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!m_effect->m_visible || m_effect->m_mode != QLatin1String("always")
            || event->state != PointerButtonState::Pressed) {
            return false;
        }

        const QPoint position = event->position.toPoint();
        for (const Thumbnail &thumbnail : std::as_const(m_effect->m_thumbnails)) {
            if (thumbnail.geometry.contains(position)) {
                if (event->button == Qt::LeftButton) {
                    m_effect->activateThumbnailAt(position);
                }
                return true;
            }
        }
        return false;
    }

private:
    WindowShelfEffect *m_effect;
};

WindowShelfEffect::WindowShelfEffect()
    : m_alwaysVisibleInputFilter(std::make_unique<AlwaysVisibleInputFilter>(this))
{
    const KConfigGroup config(effects->config(), QStringLiteral("Effect-kwin4_effect_window_shelf"));
    m_side = config.readEntry("Side", QStringLiteral("right"));
    m_mode = config.readEntry("Mode", QStringLiteral("hover"));
    m_transparency = std::clamp(config.readEntry("Transparency", 25), 0, 80);
    if (m_side != QLatin1String("left")) {
        m_side = QStringLiteral("right");
    }
    if (m_mode != QLatin1String("always")) {
        m_mode = QStringLiteral("hover");
    }

    auto addShortcut = [this](const QString &name, const QString &text, const QKeySequence &sequence, auto callback) {
        auto *action = new QAction(this);
        action->setObjectName(name);
        action->setText(text);
        KGlobalAccel::self()->setGlobalShortcut(action, sequence);
        connect(action, &QAction::triggered, this, callback);
    };

    addShortcut(QStringLiteral("ToggleWindowShelf"),
                QStringLiteral("Toggle Window Shelf"),
                QKeySequence(Qt::META | Qt::CTRL | Qt::Key_Space),
                [this]() { setVisible(!m_visible); });
    addShortcut(QStringLiteral("ToggleWindowShelfSide"),
                QStringLiteral("Move Window Shelf to the Other Side"),
                QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Space),
                [this]() { toggleSide(); });
    addShortcut(QStringLiteral("ToggleWindowShelfMode"),
                QStringLiteral("Toggle Window Shelf Hover Mode"),
                QKeySequence(Qt::META | Qt::CTRL | Qt::ALT | Qt::Key_Space),
                [this]() { toggleMode(); });

    connect(effects, &EffectsHandler::windowAdded, this, &WindowShelfEffect::connectWindow);
    connect(effects, &EffectsHandler::windowClosed, this, [this](EffectWindow *) {
        updateThumbnails();
        requestRepaint();
    });
    connect(effects, &EffectsHandler::desktopChanged, this, [this]() {
        updateThumbnails();
        requestRepaint();
    });
    connect(effects, &EffectsHandler::currentActivityChanged, this, [this]() {
        updateThumbnails();
        requestRepaint();
    });
    connect(effects, &EffectsHandler::stackingOrderChanged, this, [this]() {
        updateThumbnails();
        requestRepaint();
    });
    connect(effects, &EffectsHandler::screenAdded, this, [this]() {
        updateThumbnails();
        requestRepaint();
    });
    connect(effects, &EffectsHandler::screenRemoved, this, [this]() {
        if (m_screen && !effects->screens().contains(m_screen)) {
            m_screen = nullptr;
        }
        updateThumbnails();
        requestRepaint();
    });

    for (EffectWindow *window : effects->stackingOrder()) {
        connectWindow(window);
    }

    reserveScreenEdge();
    if (m_mode == QLatin1String("always")) {
        setVisible(true);
    }
}

WindowShelfEffect::~WindowShelfEffect()
{
    if (m_inputFilterInstalled) {
        input()->uninstallInputEventFilter(m_alwaysVisibleInputFilter.get());
        m_inputFilterInstalled = false;
    }
    if (m_pointerCursorOverThumbnail) {
        input()->pointer()->removeEffectsOverrideCursor();
        m_pointerCursorOverThumbnail = false;
    }
    if (m_mouseIntercepted) {
        effects->stopMouseInterception(this);
        m_mouseIntercepted = false;
    }
    effects->unreserveElectricBorder(m_side == QLatin1String("left") ? ElectricLeft : ElectricRight, this);
}

void WindowShelfEffect::reconfigure(ReconfigureFlags)
{
    const KConfigGroup config(effects->config(), QStringLiteral("Effect-kwin4_effect_window_shelf"));
    m_side = config.readEntry("Side", QStringLiteral("right"));
    m_mode = config.readEntry("Mode", QStringLiteral("hover"));
    if (m_side != QLatin1String("left")) {
        m_side = QStringLiteral("right");
    }
    if (m_mode != QLatin1String("always")) {
        m_mode = QStringLiteral("hover");
    }
    m_transparency = std::clamp(config.readEntry("Transparency", 25), 0, 80);
    reserveScreenEdge();
    if (m_mode == QLatin1String("always")) {
        m_visible = true;
        m_screen = effects->activeScreen();
        updateInputHandling();
        updateThumbnails();
    } else if (m_visible) {
        setVisible(false);
    }
    requestRepaint();
}

void WindowShelfEffect::prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data)
{
    if (m_visible && window->isMinimized()
        && std::any_of(m_thumbnails.cbegin(), m_thumbnails.cend(),
                       [window](const Thumbnail &thumbnail) { return thumbnail.window == window; })) {
        data.mask &= ~EffectWindow::PAINT_DISABLED_BY_MINIMIZE;
    }
    effects->prePaintWindow(view, window, data);
}

bool WindowShelfEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport,
                                    int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (!effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen)) {
        return false;
    }
    if (!m_visible || !m_screen || screen != m_screen) {
        return true;
    }

    updatePointerOverPanel(Effect::cursorPos().toPoint());
    const qreal opacity = m_pointerOverPanel ? 1 : 1 - m_transparency / 100.0;
    Region cursorExclusion;
    const PlatformCursorImage cursorImage = effects->cursorImage();
    const qreal outputScale = m_screen->scale();
    if (!cursorImage.isNull() && outputScale > 0) {
        const qreal inverseScale = 1 / outputScale;
        const QSizeF cursorSize = cursorImage.image().size() * inverseScale;
        const QPointF cursorTopLeft = Effect::cursorPos() - cursorImage.hotSpot() * inverseScale;
        cursorExclusion = Region(viewport.mapToDeviceCoordinatesAligned(
            RectF(cursorTopLeft.x(), cursorTopLeft.y(), cursorSize.width(), cursorSize.height())));
    }
    for (const Thumbnail &thumbnail : std::as_const(m_thumbnails)) {
        if (!thumbnail.window || thumbnail.window->isDeleted()) {
            continue;
        }

        const bool hovered = thumbnail.window == m_hoveredThumbnail;
        const qreal scale = hovered ? PreviewHoverScale : 1;
        const qreal width = thumbnail.geometry.width() * scale;
        const qreal height = thumbnail.geometry.height() * scale;
        const qreal x = thumbnail.geometry.center().x() - width / 2;
        const qreal y = thumbnail.geometry.center().y() - height / 2;

        WindowPaintData shadowData;
        shadowData.setBrightness(0);
        shadowData.multiplyOpacity(0.5);
        Rect region;
        const Rect shadowTarget(x + PreviewShadowOffset, y + PreviewShadowOffset, width, height);
        setPositionTransformations(shadowData, region, thumbnail.window, shadowTarget, Qt::KeepAspectRatio);
        const Region shadowRegion = Region(viewport.mapToDeviceCoordinatesAligned(region)) - cursorExclusion;
        if (!shadowRegion.isEmpty()) {
            (void)effects->drawWindow(renderTarget, viewport, thumbnail.window,
                                      PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED,
                                      shadowRegion, shadowData);
        }

        WindowPaintData data;
        data.multiplyOpacity(opacity);
        const Rect target(x, y, width, height);
        setPositionTransformations(data, region, thumbnail.window, target, Qt::KeepAspectRatio);
        const Region thumbnailRegion = Region(viewport.mapToDeviceCoordinatesAligned(region)) - cursorExclusion;
        if (!thumbnailRegion.isEmpty()) {
            (void)effects->drawWindow(renderTarget, viewport, thumbnail.window,
                                      PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED,
                                      thumbnailRegion, data);
        }
    }
    return true;
}

void WindowShelfEffect::pointerMotion(PointerMotionEvent *event)
{
    if (!m_visible || !m_mouseIntercepted) {
        return;
    }

    updatePointerOverPanel(event->position.toPoint());

    if (m_pointerOverPanel) {
        m_waitingForPanelEntry = false;
    } else if (m_mode == QLatin1String("hover") && !m_waitingForPanelEntry) {
        scheduleDismiss();
    }
}

void WindowShelfEffect::pointerButton(PointerButtonEvent *event)
{
    if (!m_visible || !m_mouseIntercepted || event->state != PointerButtonState::Pressed
        || event->button != Qt::LeftButton) {
        return;
    }

    const QPoint position = event->position.toPoint();
    if (activateThumbnailAt(position)) {
        if (m_mode == QLatin1String("hover")) {
            scheduleDismiss(true);
        }
        return;
    }

    if (!panelGeometry().contains(position)) {
        if (m_mode == QLatin1String("hover")) {
            scheduleDismiss();
        }
    }
}

bool WindowShelfEffect::borderActivated(ElectricBorder border)
{
    const ElectricBorder trigger = m_side == QLatin1String("left") ? ElectricLeft : ElectricRight;
    if (border != trigger || m_mode != QLatin1String("hover")) {
        return false;
    }

    m_screen = effects->screenAt(Effect::cursorPos().toPoint());
    if (!m_screen) {
        m_screen = effects->activeScreen();
    }
    m_waitingForPanelEntry = true;
    setVisible(true);
    return true;
}

bool WindowShelfEffect::isActive() const
{
    return m_visible && !effects->isScreenLocked();
}

void WindowShelfEffect::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;
    if (m_visible) {
        m_screen = effects->screenAt(Effect::cursorPos().toPoint());
        if (!m_screen) {
            m_screen = effects->activeScreen();
        }
        updateThumbnails();
    }
    updateInputHandling();
    if (!m_visible) {
        m_pointerOverPanel = false;
        m_hoveredThumbnail = nullptr;
        m_waitingForPanelEntry = false;
    }
    requestRepaint();
}

void WindowShelfEffect::updateInputHandling()
{
    const bool alwaysVisible = m_visible && m_mode == QLatin1String("always");
    if (!alwaysVisible && m_pointerCursorOverThumbnail) {
        input()->pointer()->removeEffectsOverrideCursor();
        m_pointerCursorOverThumbnail = false;
    }
    if (alwaysVisible && !m_inputFilterInstalled) {
        input()->installInputEventFilter(m_alwaysVisibleInputFilter.get());
        m_inputFilterInstalled = true;
    } else if (!alwaysVisible && m_inputFilterInstalled) {
        input()->uninstallInputEventFilter(m_alwaysVisibleInputFilter.get());
        m_inputFilterInstalled = false;
    }

    const bool interceptMouse = m_visible && m_mode == QLatin1String("hover");
    if (interceptMouse && !m_mouseIntercepted) {
        effects->startMouseInterception(this, Qt::ArrowCursor);
        m_mouseIntercepted = true;
    } else if (!interceptMouse && m_mouseIntercepted) {
        effects->stopMouseInterception(this);
        m_mouseIntercepted = false;
    }
}

void WindowShelfEffect::updatePointerOverPanel(const QPoint &position)
{
    const bool overPanel = panelGeometry().contains(position);
    EffectWindow *hoveredThumbnail = nullptr;
    for (const Thumbnail &thumbnail : std::as_const(m_thumbnails)) {
        if (thumbnail.geometry.contains(position) && thumbnail.window && !thumbnail.window->isDeleted()) {
            hoveredThumbnail = thumbnail.window;
            break;
        }
    }

    if (m_pointerOverPanel != overPanel || m_hoveredThumbnail != hoveredThumbnail) {
        m_pointerOverPanel = overPanel;
        m_hoveredThumbnail = hoveredThumbnail;
        if (m_mode == QLatin1String("always")) {
            if (m_pointerCursorOverThumbnail != (hoveredThumbnail != nullptr)) {
                if (hoveredThumbnail) {
                    input()->pointer()->setEffectsOverrideCursor(Qt::PointingHandCursor);
                } else {
                    input()->pointer()->removeEffectsOverrideCursor();
                }
                m_pointerCursorOverThumbnail = hoveredThumbnail != nullptr;
            }
        } else if (m_mouseIntercepted) {
            effects->defineCursor(hoveredThumbnail ? Qt::PointingHandCursor : Qt::ArrowCursor);
        }
        requestRepaint();
    }
}

bool WindowShelfEffect::activateThumbnailAt(const QPoint &position)
{
    for (const Thumbnail &thumbnail : std::as_const(m_thumbnails)) {
        if (thumbnail.geometry.contains(position) && thumbnail.window && !thumbnail.window->isDeleted()) {
            thumbnail.window->setMinimized(false);
            effects->activateWindow(thumbnail.window);
            return true;
        }
    }
    return false;
}

void WindowShelfEffect::setSide(const QString &side)
{
    if (m_side == side) {
        return;
    }
    effects->unreserveElectricBorder(m_side == QLatin1String("left") ? ElectricLeft : ElectricRight, this);
    m_side = side;
    saveSetting(QStringLiteral("Side"), m_side);
    reserveScreenEdge();
    updateThumbnails();
    requestRepaint();
}

void WindowShelfEffect::setMode(const QString &mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    saveSetting(QStringLiteral("Mode"), m_mode);
    reserveScreenEdge();
    if (m_mode == QLatin1String("always")) {
        m_screen = effects->activeScreen();
        m_visible = true;
        updateThumbnails();
    } else {
        m_visible = false;
    }
    updateInputHandling();
    m_pointerOverPanel = false;
    m_hoveredThumbnail = nullptr;
    m_waitingForPanelEntry = false;
    requestRepaint();
}

void WindowShelfEffect::toggleSide()
{
    setSide(m_side == QLatin1String("left") ? QStringLiteral("right") : QStringLiteral("left"));
}

void WindowShelfEffect::toggleMode()
{
    setMode(m_mode == QLatin1String("hover") ? QStringLiteral("always") : QStringLiteral("hover"));
}

void WindowShelfEffect::reserveScreenEdge()
{
    effects->unreserveElectricBorder(ElectricLeft, this);
    effects->unreserveElectricBorder(ElectricRight, this);
    if (m_mode == QLatin1String("hover")) {
        effects->reserveElectricBorder(m_side == QLatin1String("left") ? ElectricLeft : ElectricRight, this);
    }
}

void WindowShelfEffect::updateThumbnails()
{
    m_thumbnails.clear();
    m_panelGeometry = {};
    if (!m_visible || !m_screen) {
        updatePointerOverPanel({});
        return;
    }

    const RectF area = effects->clientArea(MaximizeArea, m_screen);
    const QList<EffectWindow *> stackingOrder = effects->stackingOrder();
    QList<EffectWindow *> windows;
    for (EffectWindow *window : stackingOrder) {
        if (!window || window->isDeleted() || (!window->isMinimized() && !window->isVisible())
            || window->screen() != m_screen || !window->isOnCurrentDesktop() || !window->isOnCurrentActivity()
            || (!window->isNormalWindow() && !window->isDialog())) {
            continue;
        }
        const QString appId = (window->windowClass() + QLatin1Char(' ') + window->caption()).toLower();
        if (appId.contains(QStringLiteral("xwaylandvideobridge"))) {
            continue;
        }
        windows.append(window);
    }
    const qreal panelWidth = std::min<qreal>(PanelWidth, area.width() * 0.4);
    const qreal x = m_side == QLatin1String("left")
        ? area.x() + PanelMargin
        : area.right() - PanelMargin - panelWidth;
    m_panelGeometry = QRect(qRound(x), qRound(area.y()), qRound(panelWidth), qRound(area.height()));
    if (windows.isEmpty()) {
        updatePointerOverPanel(Effect::cursorPos().toPoint());
        return;
    }

    const qreal previewWidth = std::max<qreal>(1, std::min<qreal>(PreviewMaxWidth, panelWidth - 2 * PanelMargin));
    const qreal usableHeight = std::max<qreal>(1, area.height() - 2 * PanelMargin
                                                   - PreviewSpacing * (windows.size() - 1));
    const qreal previewHeight = std::max<qreal>(1, std::min<qreal>(PreviewMaxHeight, usableHeight / windows.size()));
    const qreal contentHeight = previewHeight * windows.size() + PreviewSpacing * (windows.size() - 1);
    qreal y = area.y() + (area.height() - contentHeight) / 2;
    for (EffectWindow *window : std::as_const(windows)) {
        const int previewX = qRound(x + (panelWidth - previewWidth) / 2);
        const int width = qRound(previewWidth);
        const int height = qRound(previewHeight);
        m_thumbnails.append({window, QRect(previewX, qRound(y), width, height)});
        y += previewHeight + PreviewSpacing;
    }
    updatePointerOverPanel(Effect::cursorPos().toPoint());
}

void WindowShelfEffect::connectWindow(EffectWindow *window)
{
    connect(window, &EffectWindow::windowDamaged, this, [this](EffectWindow *) {
        if (m_visible) {
            requestRepaint();
        }
    });
    connect(window, &EffectWindow::windowFrameGeometryChanged, this,
            [this](EffectWindow *, const RectF &) {
                if (m_visible) {
                    updateThumbnails();
                    requestRepaint();
                }
            });
    connect(window, &EffectWindow::windowHiddenChanged, this, [this](EffectWindow *) {
        if (m_visible) {
            updateThumbnails();
            requestRepaint();
        }
    });
    connect(window, &EffectWindow::minimizedChanged, this, [this](EffectWindow *) {
        if (m_visible) {
            updateThumbnails();
            requestRepaint();
        }
    });
}

void WindowShelfEffect::scheduleDismiss(bool force)
{
    m_dismissForced = m_dismissForced || force;
    if (m_dismissScheduled) {
        return;
    }
    m_dismissScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        const bool forceDismiss = m_dismissForced;
        m_dismissScheduled = false;
        m_dismissForced = false;
        if (m_mode == QLatin1String("hover")
            && (forceDismiss || (!m_waitingForPanelEntry && !panelGeometry().contains(Effect::cursorPos().toPoint())))) {
            setVisible(false);
        }
    });
}

QRect WindowShelfEffect::panelGeometry() const
{
    return m_panelGeometry;
}

void WindowShelfEffect::saveSetting(const QString &key, const QString &value)
{
    KConfigGroup config(effects->config(), QStringLiteral("Effect-kwin4_effect_window_shelf"));
    config.writeEntry(key, value);
    config.sync();
}

void WindowShelfEffect::requestRepaint()
{
    effects->addRepaintFull();
}

} // namespace KWin
