#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Translations.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Fuentes del diseno
namespace AVFont
{
    inline juce::Font mono (float size, bool bold = false) { return AC::mono (size, bold); }
    inline juce::Font label (float size, bool bold = true, float kerning = 0.14f) { return AC::sans (size, bold ? 2 : 0, kerning); }
}

// ---------------------------------------------------------------------------
// Boton dibujado a mano (iconos, flechas, puntos...)
// ---------------------------------------------------------------------------
class DrawnButton : public juce::Button
{
public:
    DrawnButton() : juce::Button ({}) {}
    std::function<void (juce::Graphics&, juce::Rectangle<float>, bool over, bool on)> drawer;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        if (drawer) drawer (g, getLocalBounds().toFloat(), over || down, getToggleState());
    }
};

// ---------------------------------------------------------------------------
// Perilla 3D + nombre + etiqueta de valor ("MS 10")
// ---------------------------------------------------------------------------
class KnobUnit : public juce::Component
{
public:
    KnobUnit (APVTS& s, const juce::String& pid, const juce::String& captionKey, juce::Colour colour);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    juce::Slider slider;
    juce::String captionKey;
    int lang = 0;
    int knobSize = 92;
    std::function<std::pair<juce::String, juce::String>()> badge;   // prefijo, valor
    std::function<void()> onBadgeClick;
private:
    juce::Rectangle<int> badgeArea;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// ---------------------------------------------------------------------------
// Tarjeta de modulo (Afinacion, Tono, Espacio, Caracter)
// ---------------------------------------------------------------------------
class ModuleCard : public juce::Component
{
public:
    ModuleCard (ArtisVoxOneProcessor& p, int module, const juce::String& titleKey, juce::Colour col,
                const juce::String& powerId, const juce::String& styleId, juce::StringArray styleKeys);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    std::function<void()> onSettings;
    int lang = 0;
private:
    ArtisVoxOneProcessor& proc;
    int module;
    juce::String titleKey, styleId;
    juce::StringArray styleKeys;
    juce::Colour colour;
    DrawnButton power, settings, prev, next;
    std::unique_ptr<APVTS::ButtonAttachment> powerAtt;
    void drawIcon (juce::Graphics&, juce::Rectangle<float>, bool on);
};

// ---------------------------------------------------------------------------
// Tira compacta (Compresor, De-esser, Doubler)
// ---------------------------------------------------------------------------
class StripBar : public juce::Component
{
public:
    StripBar (ArtisVoxOneProcessor& p, const juce::String& nameKey, juce::Colour col, const juce::String& powerId);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<juce::String()> badge;
    std::function<void()> onBadgeClick, onSettings;
    int lang = 0;
private:
    juce::String nameKey;
    juce::Colour colour;
    DrawnButton dot, settings;
    std::unique_ptr<APVTS::ButtonAttachment> att;
    juce::Rectangle<int> badgeArea;
};

// ---------------------------------------------------------------------------
// Caja de Tonalidad (nota, Mayor/Menor y barra de correccion)
// ---------------------------------------------------------------------------
class KeyBox : public juce::Component
{
public:
    explicit KeyBox (ArtisVoxOneProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    int lang = 0;
private:
    ArtisVoxOneProcessor& proc;
    DrawnButton prev, next, major, minor;
    float cents = 0;
};

// ---------------------------------------------------------------------------
// Pantalla de EQ: analizador + curva + puntos arrastrables
// ---------------------------------------------------------------------------
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (ArtisVoxOneProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void updateAnalyzer();
    int lang = 0;
private:
    ArtisVoxOneProcessor& proc;
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 4096> fftData {};
    std::array<float, 256> spectrum {};
    int dragNode = -1;
    juce::Rectangle<float> plotArea() const;
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
};

// ---------------------------------------------------------------------------
// Pantalla de Espacio (barras de Reverb y Delay)
// ---------------------------------------------------------------------------
class SpaceDisplay : public juce::Component
{
public:
    explicit SpaceDisplay (ArtisVoxOneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int lang = 0;
    float level = 0;
private:
    ArtisVoxOneProcessor& proc;
};

// Medidor LED horizontal
class LedMeter : public juce::Component
{
public:
    std::function<float()> source;
    void update() { const float v = source ? source() : -100.0f; shown = v > shown ? v : shown - 1.0f; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float shown = -100.0f;
};

// Valor arrastrable que solo muestra texto ("+0.0 dB")
class DragValue : public juce::Slider
{
public:
    DragValue() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox) {}
    juce::String prefix;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        AC::lcdBox (g, r, juce::jmin (10.0f, r.getHeight() * 0.4f));
        AC::glowText (g, prefix + getTextFromValue (getValue()), r, AC::mono (13.0f));
    }
};

// ---------------------------------------------------------------------------
// Panel de ajustes detallados de cada modulo (se abre con "S" o el icono de ajustes)
// ---------------------------------------------------------------------------
class DetailOverlay : public juce::Component
{
public:
    explicit DetailOverlay (ArtisVoxOneProcessor& p) : proc (p) {}
    void open (int module, int lang);
    void close() { setVisible (false); items.clear(); boxes.clear(); toggles.clear(); boxAtts.clear(); toggleAtts.clear(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& e) override { if (! panelArea().contains (e.getPosition())) close(); }
private:
    ArtisVoxOneProcessor& proc;
    juce::String title;
    juce::Colour colour;
    juce::OwnedArray<KnobUnit> items;
    juce::OwnedArray<juce::ComboBox> boxes;
    juce::StringArray boxCaptions;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<APVTS::ComboBoxAttachment> boxAtts;
    juce::OwnedArray<APVTS::ButtonAttachment> toggleAtts;
    DrawnButton closeBtn;
    juce::Rectangle<int> panelArea() const { return getLocalBounds().withSizeKeepingCentre (880, 380); }
    int lang = 0;
};

// ---------------------------------------------------------------------------
// Ventana flotante de Vocal Assist: espectro en vivo + particulas tipo estrellas
// ---------------------------------------------------------------------------
class AssistWindow : public juce::Component, private juce::Timer
{
public:
    explicit AssistWindow (ArtisVoxOneProcessor& p);
    void open (int lang);
    void close();
    bool isListening() const { return state == Listening; }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onFinished;
    int lang = 0;
    void tick() { timerCallback(); }   // usado por las pruebas
    void pressMain() { mainBtn.onClick(); }
private:
    enum State { Idle, Listening, Done };
    State state = Idle;
    ArtisVoxOneProcessor& proc;
    VocalAssist::Result result;
    juce::TextButton mainBtn, undoBtn;
    DrawnButton closeBtn;
    juce::OwnedArray<DrawnButton> genreBtns;
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 4096> fftData {};
    std::array<float, 72> bars {}, peaks {};
    struct Particle { float x, y, vx, vy, life, maxLife, size, phase; juce::Colour c; };
    std::vector<Particle> particles;
    juce::Random rng;
    float level = -100.0f, t = 0.0f;
    juce::Rectangle<int> panel() const { return getLocalBounds().withSizeKeepingCentre (780, 540); }
    juce::Rectangle<float> specArea() const;
    void timerCallback() override;
    void startAnalysis();
    void updateButtons();
};

// ===========================================================================
class ArtisVoxOneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ArtisVoxOneEditor (ArtisVoxOneProcessor&);
    ~ArtisVoxOneEditor() override;
    void paint (juce::Graphics&) override;
    void parentHierarchyChanged() override;
    void paintStatic (juce::Graphics&);
    juce::Image bgCache, leather;
    int bgLang = -1;
    juce::String presetCategory, shownPreset { "\x01" };
    void resized() override;

private:
    void timerCallback() override;
    void applyLanguage();
    void refreshPresetBox();
    void stepPreset (int dir);
    void savePresetDialog();

    ArtisVoxOneProcessor& proc;
    ArtiVoxLookAndFeel lnf;
    int lang = 0;

    // Cabecera
    DrawnButton undoBtn, redoBtn, prevBtn, nextBtn, plusBtn, abBtn, assistBtn;
    juce::TextButton bypassBtn { "BYPASS" };
    std::unique_ptr<APVTS::ButtonAttachment> bypassAtt;
    juce::ComboBox presetBox;
    juce::OwnedArray<DrawnButton> langBtns;
    bool assistRunning = false; float assistProgress = 0.0f;
    juce::String statusMsg; int statusTimer = 0;
    int undoTick = 0;

    // Tarjetas y tiras
    std::unique_ptr<ModuleCard> cardTune, cardTone, cardSpace, cardChar;
    std::unique_ptr<StripBar> stripComp, stripDeess, stripDoubler;

    // Paneles
    KeyBox keyBox { proc };
    std::unique_ptr<KnobUnit> kSpeed, kHuman, kReverb, kDelay, kSat, kWidth;
    EqDisplay eqDisplay { proc };
    juce::Slider eqSliders[4];
    std::unique_ptr<APVTS::SliderAttachment> eqAtts[4];
    SpaceDisplay spaceDisplay { proc };

    // Pie
    LedMeter inMeter, outMeter;
    DragValue inGain, outGain, mixValue;
    std::unique_ptr<APVTS::SliderAttachment> inAtt, outAtt, mixAtt;

    DetailOverlay overlay { proc };
    AssistWindow assistWin { proc };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtisVoxOneEditor)
};
