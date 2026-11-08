#pragma once

#include "effect/effect.h"

#include <QRect>
#include <QVector>
#include <memory>

namespace KWin
{

class WindowShelfEffect final : public Effect
{
    Q_OBJECT

public:
    WindowShelfEffect();
    ~WindowShelfEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data) override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport,
                     int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    bool borderActivated(ElectricBorder border) override;
    bool isActive() const override;

private:
    class AlwaysVisibleInputFilter;

    struct Thumbnail
    {
        EffectWindow *window = nullptr;
        QRect geometry;
    };

    void updateInputHandling();
    void updatePointerOverPanel(const QPoint &position);
    bool activateThumbnailAt(const QPoint &position);
    void setVisible(bool visible);
    void setSide(const QString &side);
    void setMode(const QString &mode);
    void toggleSide();
    void toggleMode();
    void reserveScreenEdge();
    void updateThumbnails();
    void connectWindow(EffectWindow *window);
    void scheduleDismiss(bool force = false);
    QRect panelGeometry() const;
    void saveSetting(const QString &key, const QString &value);
    void requestRepaint();

    QVector<Thumbnail> m_thumbnails;
    std::unique_ptr<AlwaysVisibleInputFilter> m_alwaysVisibleInputFilter;
    LogicalOutput *m_screen = nullptr;
    QRect m_panelGeometry;
    QString m_side = QStringLiteral("right");
    QString m_mode = QStringLiteral("hover");
    bool m_visible = false;
    bool m_mouseIntercepted = false;
    bool m_inputFilterInstalled = false;
    bool m_pointerOverPanel = false;
    EffectWindow *m_hoveredThumbnail = nullptr;
    bool m_pointerCursorOverThumbnail = false;
    bool m_waitingForPanelEntry = false;
    bool m_dismissScheduled = false;
    bool m_dismissForced = false;
    int m_transparency = 25;
};

} // namespace KWin
