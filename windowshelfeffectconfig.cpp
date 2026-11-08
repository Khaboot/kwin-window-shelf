#include <KCModule>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KPluginFactory>

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

class WindowShelfConfig final : public KCModule
{
    Q_OBJECT

public:
    WindowShelfConfig(QObject *parent, const KPluginMetaData &data)
        : KCModule(parent, data)
        , m_mode(new QComboBox(widget()))
        , m_side(new QComboBox(widget()))
        , m_transparency(new QSpinBox(widget()))
    {
        m_mode->addItem(i18n("Show when the pointer reaches the screen edge"), QStringLiteral("hover"));
        m_mode->addItem(i18n("Always visible"), QStringLiteral("always"));
        m_side->addItem(i18n("Right"), QStringLiteral("right"));
        m_side->addItem(i18n("Left"), QStringLiteral("left"));
        m_transparency->setRange(0, 80);
        m_transparency->setSuffix(i18n("%"));

        auto *layout = new QFormLayout(widget());
        layout->addRow(i18n("Display mode:"), m_mode);
        layout->addRow(i18n("Screen edge:"), m_side);
        layout->addRow(i18n("Preview transparency:"), m_transparency);

        connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, &KCModule::markAsChanged);
        connect(m_side, qOverload<int>(&QComboBox::currentIndexChanged), this, &KCModule::markAsChanged);
        connect(m_transparency, qOverload<int>(&QSpinBox::valueChanged), this, &KCModule::markAsChanged);
    }

    void load() override
    {
        const KConfigGroup config(KSharedConfig::openConfig(QStringLiteral("kwinrc")),
                                  QStringLiteral("Effect-kwin4_effect_window_shelf"));
        int modeIndex = m_mode->findData(config.readEntry("Mode", QStringLiteral("hover")));
        int sideIndex = m_side->findData(config.readEntry("Side", QStringLiteral("right")));
        m_mode->setCurrentIndex(modeIndex < 0 ? 0 : modeIndex);
        m_side->setCurrentIndex(sideIndex < 0 ? 0 : sideIndex);
        m_transparency->setValue(qBound(0, config.readEntry("Transparency", 25), 80));
        setNeedsSave(false);
    }

    void save() override
    {
        KConfigGroup config(KSharedConfig::openConfig(QStringLiteral("kwinrc")),
                            QStringLiteral("Effect-kwin4_effect_window_shelf"));
        config.writeEntry("Mode", m_mode->currentData().toString());
        config.writeEntry("Side", m_side->currentData().toString());
        config.writeEntry("Transparency", m_transparency->value());
        config.sync();
        setNeedsSave(false);
    }

    void defaults() override
    {
        m_mode->setCurrentIndex(m_mode->findData(QStringLiteral("hover")));
        m_side->setCurrentIndex(m_side->findData(QStringLiteral("right")));
        m_transparency->setValue(25);
        markAsChanged();
    }

private:
    QComboBox *m_mode;
    QComboBox *m_side;
    QSpinBox *m_transparency;
};

K_PLUGIN_CLASS_WITH_JSON(WindowShelfConfig, "config.json")

#include "windowshelfeffectconfig.moc"
