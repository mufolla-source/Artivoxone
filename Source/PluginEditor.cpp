#include "PluginEditor.h"
#include "BinaryData.h"

using namespace AVColours;

namespace
{
    constexpr int W = 1200, H = 780;
    constexpr int B = 10;                       // grosor del borde de goma/cuero
    constexpr int TW = W + 2 * B, TH = H + 2 * B;

    // Textura de goma/cuero gris (grano fino + manchas suaves), generada una vez
    juce::Image makeLeather (int w, int h)
    {
        juce::Image img (juce::Image::RGB, w, h, false, juce::SoftwareImageType());
        juce::Random rng (0xA27150);
        const int gs = 18;
        const int gw = w / gs + 2, gh = h / gs + 2;
        std::vector<float> grid ((size_t) (gw * gh));
        for (auto& v : grid) v = rng.nextFloat();
        auto coarse = [&] (int x, int y)
        {
            const float fx = x / (float) gs, fy = y / (float) gs;
            const int ix = (int) fx, iy = (int) fy;
            const float tx = fx - ix, ty = fy - iy;
            auto at = [&] (int a, int b) { return grid[(size_t) (b * gw + a)]; };
            const float a = at (ix, iy) + (at (ix + 1, iy) - at (ix, iy)) * tx;
            const float b = at (ix, iy + 1) + (at (ix + 1, iy + 1) - at (ix, iy + 1)) * tx;
            return a + (b - a) * ty;
        };
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float v = 50.0f;                                    // gris base
                v += (coarse (x, y) - 0.5f) * 9.0f;                  // manchas del cuero
                v += (coarse (x * 3 + 7, y * 3 + 11) - 0.5f) * 5.0f; // poro medio
                v += (rng.nextFloat() - 0.5f) * 10.0f;               // grano fino
                if (rng.nextFloat() < 0.02f) v -= 9.0f;              // poros oscuros
                const auto c = (juce::uint8) juce::jlimit (0, 255, (int) v);
                bd.setPixelColour (x, y, juce::Colour (c, c, (juce::uint8) juce::jlimit (0, 255, (int) v + 3)));
            }
        return img;
    }
    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    float pval (ArtisVoxOneProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    juce::String fmt (const juce::String& pid, double v)
    {
        static const juce::StringArray dbIds { PID::inGain, PID::outGain, PID::gateThresh, PID::lowGain, PID::mudGain,
                                               PID::presGain, PID::airGain, PID::compMakeup, PID::toneTilt };
        static const juce::StringArray hzIds { PID::hpfFreq, PID::lowFreq, PID::mudFreq, PID::presFreq, PID::airFreq, PID::deessFreq };
        static const juce::StringArray msIds { PID::tuneSpeed, PID::compAttack, PID::compRelease, PID::revPre };
        if (pid == PID::gateThresh && v <= -79.5) return "Off";
        if (dbIds.contains (pid)) return (v > 0.049 ? "+" : "") + juce::String (v, 1) + " dB";
        if (hzIds.contains (pid)) return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
        if (msIds.contains (pid)) return v < 10.0 ? juce::String (v, 1) + " ms" : juce::String ((int) std::round (v)) + " ms";
        if (pid == PID::compRatio) return juce::String (v, 1) + ":1";
        if (pid == PID::revDecay)  return juce::String (v, 1) + " s";
        return juce::String ((int) std::round (v)) + "%";
    }

    void drawBadge (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& prefix, const juce::String& value)
    {
        auto f1 = AC::sans (10.5f, 2, 0.08f), f2 = AC::mono (11.5f, true);
        const float w1 = prefix.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (f1, prefix + "  ");
        const float w2 = juce::GlyphArrangement::getStringWidth (f2, value);
        const float w = w1 + w2 + 24.0f;
        auto r = area.toFloat().withSizeKeepingCentre (w, 22.0f);
        AC::pillBox (g, r, false, false);
        auto t = r.reduced (12.0f, 0);
        g.setColour (AC::sub); g.setFont (f1);
        g.drawText (prefix, t.removeFromLeft (w1), juce::Justification::centredLeft);
        AC::glowText (g, value, t, f2, juce::Justification::centredLeft);
    }

    void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.07f * (4 - i)));
            g.strokePath (p, juce::PathStrokeType (w + i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, bool left, juce::Colour c)
    {
        juce::Path p;
        const auto cc = r.getCentre(); const float s = 4.0f;
        if (left) { p.startNewSubPath (cc.x + s * 0.5f, cc.y - s); p.lineTo (cc.x - s * 0.5f, cc.y); p.lineTo (cc.x + s * 0.5f, cc.y + s); }
        else      { p.startNewSubPath (cc.x - s * 0.5f, cc.y - s); p.lineTo (cc.x + s * 0.5f, cc.y); p.lineTo (cc.x - s * 0.5f, cc.y + s); }
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Flecha rellena estilo ArtisComp (dentro de las pantallas)
    void drawTri (juce::Graphics& g, juce::Rectangle<float> r, bool left, juce::Colour c)
    {
        const auto cc = r.getCentre(); const float s = 4.5f;
        juce::Path p;
        if (left) p.addTriangle (cc.x - s * 0.6f, cc.y, cc.x + s * 0.6f, cc.y - s, cc.x + s * 0.6f, cc.y + s);
        else      p.addTriangle (cc.x + s * 0.6f, cc.y, cc.x - s * 0.6f, cc.y - s, cc.x - s * 0.6f, cc.y + s);
        g.setColour (c.withAlpha (0.3f)); g.fillPath (p, juce::AffineTransform::scale (1.4f, 1.4f, cc.x, cc.y));
        g.setColour (c); g.fillPath (p);
    }

    void drawPower (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        auto cc = r.getCentre(); const float rad = 6.0f;
        juce::Path p;
        p.addCentredArc (cc.x, cc.y + 1, rad, rad, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
        p.startNewSubPath (cc.x, cc.y - rad - 1); p.lineTo (cc.x, cc.y);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawSliders (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        auto cc = r.getCentre();
        g.setColour (c);
        for (int i = -1; i <= 1; i += 2)
        {
            const float y = cc.y + i * 3.5f;
            g.drawLine (cc.x - 7, y, cc.x + 7, y, 1.4f);
            const float kx = cc.x + (i < 0 ? 3.0f : -3.0f);
            g.setColour (panel); g.fillEllipse (kx - 2.5f, y - 2.5f, 5, 5);
            g.setColour (c);     g.drawEllipse (kx - 2.5f, y - 2.5f, 5, 5, 1.3f);
        }
    }

    juce::Rectangle<int> cardRect (int i)  { return { 28 + i * 288, 110, 278, 184 }; }
    juce::Rectangle<int> stripRect (int i) { return { 28 + i * 385, 304, 374, 48 }; }
    juce::Rectangle<int> panelRect (int i) { return { 28 + i * 288, 362, 278, 342 }; }
    juce::Rectangle<int> presetLcd() { return { 392, 22, 276, 68 }; }
}

// ============================================================================
//  KnobUnit
// ============================================================================
KnobUnit::KnobUnit (APVTS& s, const juce::String& pid, const juce::String& key, juce::Colour colour) : captionKey (key)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
    slider.textFromValueFunction = [pid] (double v) { return fmt (pid, v); };
    if (auto* prm = s.getParameter (pid))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.onValueChange = [this] { repaint(); };
}

void KnobUnit::resized()
{
    auto r = getLocalBounds();
    slider.setBounds (r.removeFromTop (knobSize).withSizeKeepingCentre (knobSize, knobSize));
    badgeArea = r.withTrimmedTop (26).withHeight (24);
}

void KnobUnit::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (knobSize + 2);
    g.setColour (AC::text);
    g.setFont (AC::sans (knobSize >= 80 ? 15.0f : 13.5f, 2));
    juce::StringArray parts; parts.addTokens (captionKey, "|", "");
    juce::StringArray out; for (auto& pt : parts) out.add (tr (pt, lang));
    g.drawText (out.joinIntoString (" "), r.removeFromTop (22), juce::Justification::centred);
    if (badge)
    {
        auto b = badge();
        drawBadge (g, badgeArea, b.first, b.second);
    }
}

void KnobUnit::mouseUp (const juce::MouseEvent& e)
{
    if (onBadgeClick && badgeArea.contains (e.getPosition())) onBadgeClick();
}

// ============================================================================
//  ModuleCard
// ============================================================================
ModuleCard::ModuleCard (ArtisVoxOneProcessor& p, int m, const juce::String& tKey, juce::Colour col,
                        const juce::String& powerId, const juce::String& sId, juce::StringArray sKeys)
    : proc (p), module (m), titleKey (tKey), styleId (sId), styleKeys (sKeys), colour (col)
{
    for (auto* b : { &power, &settings, &prev, &next }) addAndMakeVisible (b);
    power.setClickingTogglesState (true);
    powerAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, powerId, power);
    power.drawer = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool, bool on) { AC::sphere (g, r.reduced (r.getWidth() * 0.23f), colour, on); };
    power.onClick = [this] { repaint(); };
    power.setTooltip ("On / Off");

    settings.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        AC::roundButton (g, r, over);
        AC::gearIcon (g, r, over ? AC::text : AC::sub);
    };
    settings.onClick = [this] { if (onSettings) onSettings(); };

    prev.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, true, over ? AC::text : AC::lcdText); };
    next.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, false, over ? AC::text : AC::lcdText); };
    prev.onClick = [this] { proc.applyStyle (module, (int) pval (proc, styleId.toRawUTF8()) - 1); repaint(); };
    next.onClick = [this] { proc.applyStyle (module, (int) pval (proc, styleId.toRawUTF8()) + 1); repaint(); };
}

void ModuleCard::resized()
{
    power.setBounds (8, 6, 42, 42);
    settings.setBounds (getWidth() - 48, 12, 34, 34);
    prev.setBounds (22, getHeight() - 52, 26, 36);
    next.setBounds (getWidth() - 48, getHeight() - 52, 26, 36);
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    AC::panelBox (g, r, 18.0f);
    AC::oledBox (g, r.reduced (6.0f), 13.0f);

    const bool on = power.getToggleState();
    g.setColour (on ? AC::text.withAlpha (0.85f) : AC::muted);
    g.setFont (AC::caps (11.0f, 3));
    g.drawText (upper (tr (titleKey, lang)), 52, 12, getWidth() - 110, 30, juce::Justification::centredLeft);

    drawIcon (g, juce::Rectangle<float> (0, 0, 64, 64).withCentre ({ r.getCentreX(), 88.0f }), on);

    // Linea de pixeles que separa el estilo elegido
    auto lcdR = juce::Rectangle<float> (18.0f, r.getBottom() - 52.0f, r.getWidth() - 36.0f, 36.0f);
    for (float x = lcdR.getX() + 8.0f; x < lcdR.getRight() - 8.0f; x += 4.0f)
    {
        g.setColour (AC::grad ((x - lcdR.getX()) / lcdR.getWidth()).withAlpha (on ? 0.35f : 0.12f));
        g.fillRect (x, lcdR.getY() - 4.0f, 1.5f, 1.5f);
    }
    const int idx = juce::jlimit (0, 3, (int) pval (proc, styleId.toRawUTF8()));
    AC::glowText (g, upper (tr (styleKeys[idx], lang)), lcdR.translated (0, -1), AC::mono (15.0f, true, 0.06f),
                  juce::Justification::centred, on ? AC::lcdText : AC::muted);
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i == idx ? AC::lcdText : AC::dotOff);
        g.fillEllipse (lcdR.getCentreX() - 13.0f + i * 8.0f, lcdR.getBottom() - 7.0f, 3.0f, 3.0f);
    }
}

void ModuleCard::paintOverChildren (juce::Graphics& g)
{
    AC::glassSheen (g, getLocalBounds().toFloat().reduced (6.0f), 13.0f);
}

void ModuleCard::drawIcon (juce::Graphics& g, juce::Rectangle<float> b, bool on)
{
    const auto c = on ? colour : dim.darker (0.2f);
    const auto cc = b.getCentre();
    juce::Path p;
    switch (module)
    {
        case 0: // Microfono
        {
            p.addRoundedRectangle (cc.x - 10, cc.y - 26, 20, 34, 10);
            p.startNewSubPath (cc.x - 5, cc.y - 14); p.lineTo (cc.x + 5, cc.y - 14);
            p.startNewSubPath (cc.x - 5, cc.y - 8);  p.lineTo (cc.x + 5, cc.y - 8);
            p.startNewSubPath (cc.x - 5, cc.y - 2);  p.lineTo (cc.x + 5, cc.y - 2);
            p.addCentredArc (cc.x, cc.y - 2, 17, 17, 0, juce::degreesToRadians (100.0f), juce::degreesToRadians (260.0f), true);
            p.startNewSubPath (cc.x, cc.y + 15); p.lineTo (cc.x, cc.y + 26);
            p.startNewSubPath (cc.x - 10, cc.y + 26); p.lineTo (cc.x + 10, cc.y + 26);
            break;
        }
        case 1: // Circulo con campana de EQ
        {
            p.addEllipse (cc.x - 28, cc.y - 28, 56, 56);
            p.startNewSubPath (cc.x - 18, cc.y + 6);
            p.cubicTo (cc.x - 8, cc.y + 6, cc.x - 6, cc.y - 16, cc.x, cc.y - 16);
            p.cubicTo (cc.x + 6, cc.y - 16, cc.x + 8, cc.y + 6, cc.x + 18, cc.y + 6);
            p.startNewSubPath (cc.x - 16, cc.y + 14); p.lineTo (cc.x + 16, cc.y + 14);
            break;
        }
        case 2: // Planeta con anillo y destellos
        {
            p.addEllipse (cc.x - 12, cc.y - 12, 24, 24);
            juce::Path ring; ring.addEllipse (cc.x - 32, cc.y - 9, 64, 18);
            ring.applyTransform (juce::AffineTransform::rotation (-0.35f, cc.x, cc.y));
            p.addPath (ring);
            auto star = [&] (float x, float y, float s) { p.startNewSubPath (x - s, y); p.lineTo (x + s, y); p.startNewSubPath (x, y - s); p.lineTo (x, y + s); };
            star (cc.x + 24, cc.y - 24, 4); star (cc.x - 26, cc.y - 18, 3);
            break;
        }
        default: // Llama
        {
            p.startNewSubPath (cc.x, cc.y - 28);
            p.cubicTo (cc.x + 6, cc.y - 14, cc.x + 22, cc.y - 6, cc.x + 20, cc.y + 10);
            p.cubicTo (cc.x + 18, cc.y + 24, cc.x + 6, cc.y + 28, cc.x, cc.y + 28);
            p.cubicTo (cc.x - 6, cc.y + 28, cc.x - 18, cc.y + 24, cc.x - 20, cc.y + 10);
            p.cubicTo (cc.x - 21, cc.y, cc.x - 12, cc.y - 6, cc.x - 8, cc.y - 12);
            p.cubicTo (cc.x - 8, cc.y - 4, cc.x - 4, cc.y, cc.x - 2, cc.y);
            p.cubicTo (cc.x - 4, cc.y - 12, cc.x - 2, cc.y - 20, cc.x, cc.y - 28);
            p.startNewSubPath (cc.x, cc.y + 22);
            p.cubicTo (cc.x - 7, cc.y + 22, cc.x - 8, cc.y + 14, cc.x - 4, cc.y + 8);
            p.cubicTo (cc.x - 2, cc.y + 12, cc.x + 1, cc.y + 12, cc.x + 1, cc.y + 4);
            p.cubicTo (cc.x + 6, cc.y + 8, cc.x + 7, cc.y + 22, cc.x, cc.y + 22);
            break;
        }
    }
    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (AC::pink.withAlpha (0.06f * (4 - i)));
            g.strokePath (p, juce::PathStrokeType (2.2f + i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setGradientFill (juce::ColourGradient (AC::purple.brighter (0.2f), b.getX(), b.getBottom(), AC::pink, b.getRight(), b.getY(), false));
        g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ============================================================================
//  StripBar
// ============================================================================
StripBar::StripBar (ArtisVoxOneProcessor& p, const juce::String& key, juce::Colour col, const juce::String& powerId)
    : nameKey (key), colour (col)
{
    addAndMakeVisible (dot); addAndMakeVisible (settings);
    dot.setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (p.apvts, powerId, dot);
    dot.drawer = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool, bool on) { AC::sphere (g, r.reduced (r.getWidth() * 0.25f), colour, on); };
    dot.onClick = [this] { repaint(); };
    settings.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        AC::roundButton (g, r, over);
        AC::gearIcon (g, r, over ? AC::text : AC::sub);
    };
    settings.onClick = [this] { if (onSettings) onSettings(); };
}

void StripBar::resized()
{
    dot.setBounds (6, (getHeight() - 42) / 2, 42, 42);
    settings.setBounds (getWidth() - 44, (getHeight() - 32) / 2, 32, 32);
    badgeArea = { getWidth() - 160, 0, 108, getHeight() };
}

void StripBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    AC::panelBox (g, r, 14.0f);
    g.setColour (dot.getToggleState() ? AC::text : AC::muted);
    g.setFont (AC::caps (12.5f, 3));
    g.drawText (upper (tr (nameKey, lang)), 52, 0, 200, getHeight(), juce::Justification::centredLeft);
    if (badge)
    {
        const auto t = badge();
        auto b = badgeArea.toFloat().withSizeKeepingCentre ((float) badgeArea.getWidth(), 30.0f);
        AC::lcdBox (g, b, 8.0f);
        AC::glowText (g, t, b, AC::mono (13.0f), juce::Justification::centred, dot.getToggleState() ? AC::lcdText : AC::muted);
    }
}

void StripBar::mouseUp (const juce::MouseEvent& e)
{
    if (onBadgeClick && badgeArea.contains (e.getPosition())) onBadgeClick();
}

// ============================================================================
//  KeyBox
// ============================================================================
KeyBox::KeyBox (ArtisVoxOneProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next, &major, &minor }) addAndMakeVisible (b);
    prev.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, true, over ? AC::text : AC::lcdText); };
    next.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, false, over ? AC::text : AC::lcdText); };
    auto stepKey = [this] (int d)
    {
        const int k = (int) pval (proc, PID::tuneKey);
        proc.setParamValue (PID::tuneKey, (float) (((k + d) % 12 + 12) % 12));
        repaint();
    };
    prev.onClick = [stepKey] { stepKey (-1); };
    next.onClick = [stepKey] { stepKey (1); };

    auto seg = [this] (DrawnButton& b, int scale, const char* key)
    {
        b.drawer = [this, scale, key] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
        {
            const bool sel = (int) pval (proc, PID::tuneScale) == scale;
            auto rr = r.reduced (3.0f);
            if (sel) AC::pillBox (g, rr, true, over);
            g.setColour (sel ? juce::Colours::white : (over ? AC::text : AC::sub));
            g.setFont (AC::sans (13.0f, 2));
            g.drawText (tr (key, lang), rr, juce::Justification::centred);
        };
        b.onClick = [this, scale] { proc.setParamValue (PID::tuneScale, (float) scale); repaint(); };
    };
    seg (major, 0, "Mayor");
    seg (minor, 1, "Menor");
}

void KeyBox::resized()
{
    prev.setBounds (14, 34, 24, 38);
    next.setBounds (88, 34, 24, 38);
    const int sx = 124, sw = (getWidth() - 12 - sx) / 2;
    major.setBounds (sx, 34, sw, 38);
    minor.setBounds (sx + sw, 34, sw, 38);
}

void KeyBox::update()
{
    cents = cents * 0.7f + proc.getCorrectionCents() * 0.3f;
    repaint();
}

void KeyBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (AC::sub); g.setFont (AC::caps (10.5f, 2));
    g.drawText (upper (tr ("Tonalidad", lang)), 6, 4, 200, 18, juce::Justification::centredLeft);

    // Pantalla con la tonalidad
    auto lcdR = juce::Rectangle<float> (8.0f, 30.0f, 110.0f, 46.0f);
    AC::lcdBox (g, lcdR, 10.0f);
    const int key = (int) pval (proc, PID::tuneKey);
    AC::glowText (g, noteNames[key], lcdR, AC::mono (22.0f));

    // Selector Mayor / Menor
    auto seg = juce::Rectangle<float> (122.0f, 32.0f, r.getWidth() - 132.0f, 42.0f);
    g.setColour (juce::Colour (0xff0f0e12)); g.fillRoundedRectangle (seg, 21.0f);
    g.setColour (AC::pillEdge); g.drawRoundedRectangle (seg.reduced (0.5f), 21.0f, 1.0f);

    // Correccion en vivo: fila de LEDs desde el centro (-50 .. +50 cents)
    const int n = 25;
    const float d = 5.0f;
    const float x0 = 10.0f, x1 = r.getWidth() - 10.0f;
    const float step = (x1 - x0 - d) / (n - 1);
    const float y = r.getBottom() - 16.0f;
    const float c = juce::jlimit (-50.0f, 50.0f, cents);
    const int mid = n / 2;
    const int lit = (int) std::round (c / 50.0f * mid);
    for (int i = 0; i < n; ++i)
    {
        const int rel = i - mid;
        const bool on = rel == 0 || (lit > 0 && rel > 0 && rel <= lit) || (lit < 0 && rel < 0 && rel >= lit);
        AC::dotLed (g, x0 + i * step, y, d, on, (float) std::abs (rel) / (float) mid);
    }
    g.setColour (AC::muted); g.setFont (AC::mono (9.0f, false));
    g.drawText ("-50", (int) x0 - 4, (int) y - 15, 30, 12, juce::Justification::centredLeft);
    g.drawText ("+50", (int) x1 - 26, (int) y - 15, 30, 12, juce::Justification::centredRight);
    g.drawText ("cents", 0, (int) y - 15, getWidth(), 12, juce::Justification::centred);
}

// ============================================================================
//  EqDisplay
// ============================================================================
EqDisplay::EqDisplay (ArtisVoxOneProcessor& p) : proc (p) { spectrum.fill (-100.0f); }

juce::Rectangle<float> EqDisplay::plotArea() const { return getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (18.0f); }
float EqDisplay::xForFreq (double f) const { auto a = plotArea(); return a.getX() + (float) (std::log10 (f / 20.0) / 3.0) * a.getWidth(); }
double EqDisplay::freqForX (float x) const { auto a = plotArea(); return 20.0 * std::pow (1000.0, (x - a.getX()) / a.getWidth()); }
float EqDisplay::yForDb (double db) const { auto a = plotArea(); return a.getCentreY() - (float) (db / 15.0) * a.getHeight() * 0.5f; }
double EqDisplay::dbForY (float y) const { auto a = plotArea(); return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * 15.0; }

void EqDisplay::updateAnalyzer()
{
    proc.copyAnalyzer (fftData.data(), 2048);
    std::fill (fftData.begin() + 2048, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), 2048);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const double sr = proc.getCurrentSampleRate() > 0 ? proc.getCurrentSampleRate() : 48000.0;
    for (int i = 0; i < (int) spectrum.size(); ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / (spectrum.size() - 1));
        const int bin = juce::jlimit (1, 1023, (int) std::round (f / sr * 2048.0));
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) bin] / 512.0f, -100.0f);
        spectrum[(size_t) i] = db > spectrum[(size_t) i] ? spectrum[(size_t) i] * 0.4f + db * 0.6f : spectrum[(size_t) i] * 0.88f + db * 0.12f;
    }
    repaint();
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    AC::lcdBox (g, full, 12.0f, false);
    auto a = plotArea();

    // Rejilla
    g.setColour (juce::Colour (0xff221f29));
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) xForFreq (f), a.getY(), a.getBottom());
    for (double db : { -10.0, 0.0, 10.0 }) g.drawHorizontalLine ((int) yForDb (db), a.getX(), a.getRight());

    // Analizador
    {
        juce::Path sp; sp.startNewSubPath (a.getX(), a.getBottom());
        for (int i = 0; i < (int) spectrum.size(); ++i)
        {
            const float x = a.getX() + a.getWidth() * i / (float) (spectrum.size() - 1);
            const float y = juce::jmap (juce::jlimit (-90.0f, 0.0f, spectrum[(size_t) i] + 4.5f * std::log2 (1.0f + i * 0.05f)), -90.0f, 0.0f, a.getBottom(), a.getY() + a.getHeight() * 0.15f);
            sp.lineTo (x, y);
        }
        sp.lineTo (a.getRight(), a.getBottom()); sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (AC::violet.withAlpha (0.38f), 0, a.getY(), AC::purple.withAlpha (0.04f), 0, a.getBottom(), false));
        g.fillPath (sp);
    }

    // Curva de EQ
    const double sr = proc.getCurrentSampleRate() > 0 ? proc.getCurrentSampleRate() : 48000.0;
    EqSettings s { 20.0f, pval (proc, PID::lowFreq), pval (proc, PID::lowGain), pval (proc, PID::mudFreq), pval (proc, PID::mudGain),
                   pval (proc, PID::presFreq), pval (proc, PID::presGain), pval (proc, PID::airFreq), pval (proc, PID::airGain), pval (proc, PID::toneTilt) };
    auto c = EqCoeffs::make (sr, s);
    c.hpf1 = nullptr; c.hpf2 = nullptr;
    juce::Path p;
    for (int i = 0; i <= (int) a.getWidth(); ++i)
    {
        const float x = a.getX() + (float) i;
        const float y = juce::jlimit (a.getY(), a.getBottom(), yForDb (c.magnitudeDb (freqForX (x), sr)));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    {
        juce::Path glowP (p);
        g.setColour (AC::pink.withAlpha (0.18f)); g.strokePath (glowP, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (AC::purple, a.getX(), 0, AC::pink, a.getRight(), 0, false));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Nodos
    const char* fIds[] = { PID::lowFreq, PID::mudFreq, PID::presFreq, PID::airFreq };
    for (int i = 0; i < 4; ++i)
    {
        const double f = pval (proc, fIds[i]);
        const float x = xForFreq (f), y = juce::jlimit (a.getY(), a.getBottom(), yForDb (c.magnitudeDb (f, sr)));
        g.setColour (AC::grad (i / 3.0f).withAlpha (0.35f)); g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        g.setColour (dragNode == i ? AC::pink : juce::Colours::white);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
    }

    // Etiquetas
    auto lab = full.removeFromBottom (20).reduced (8, 0);
    g.setColour (AC::lcdText); g.setFont (AC::caps (9.5f, 3));
    g.drawText (upper (tr ("Analizador", lang)), lab, juce::Justification::centredLeft);
    g.setColour (AC::muted); g.setFont (AC::mono (9.5f, false));
    const float lx = lab.getX() + 82.0f, step = (lab.getRight() - lx) / 3.0f;
    int li = 0;
    for (auto* t : { "100Hz", "1kHz", "10kHz" })
        g.drawText (t, juce::Rectangle<float> (lx + step * li++, lab.getY(), step, lab.getHeight()), li == 3 ? juce::Justification::centredRight : juce::Justification::centred);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const char* fIds[] = { PID::lowFreq, PID::mudFreq, PID::presFreq, PID::airFreq };
    float best = 1e9f; dragNode = -1;
    for (int i = 0; i < 4; ++i)
    {
        const float d = std::abs (xForFreq (pval (proc, fIds[i])) - e.position.x);
        if (d < best) { best = d; dragNode = i; }
    }
    if (best > 30.0f) dragNode = -1;
    repaint();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    const char* fIds[] = { PID::lowFreq, PID::mudFreq, PID::presFreq, PID::airFreq };
    const char* gIds[] = { PID::lowGain, PID::mudGain, PID::presGain, PID::airGain };
    proc.setParamValue (fIds[dragNode], (float) freqForX (e.position.x));
    proc.setParamValue (gIds[dragNode], (float) juce::jlimit (-12.0, 12.0, dbForY (e.position.y)));
}

void EqDisplay::mouseUp (const juce::MouseEvent&) { dragNode = -1; repaint(); }

// ============================================================================
//  SpaceDisplay
// ============================================================================
void SpaceDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    AC::lcdBox (g, r, 12.0f);
    const float half = r.getWidth() * 0.5f;
    g.setColour (AC::lcdEdge); g.drawVerticalLine ((int) half, r.getY() + 10, r.getBottom() - 10);

    const bool on = pval (proc, PID::spaceOn) > 0.5f;
    auto drawHalf = [&] (juce::Rectangle<float> h, const juce::String& title, juce::Colour col,
                         const juce::String& footKey, const juce::String& footVal, bool reverb)
    {
        g.setColour (on ? AC::sub : AC::muted); g.setFont (AC::caps (9.5f, 3));
        g.drawText (upper (title), h.reduced (12, 0).removeFromTop (26), juce::Justification::centredLeft);
        auto bars = h.reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (26);
        const int nb = 9;
        const float bw = 6.0f, gap = (bars.getWidth() - nb * bw) / (nb - 1);
        const float size = pval (proc, PID::revSize) * 0.01f, fb = pval (proc, PID::dlyFeedback) * 0.01f;
        const float mix = (reverb ? pval (proc, PID::revMix) : pval (proc, PID::dlyMix)) * 0.01f;
        for (int i = 0; i < nb; ++i)
        {
            float hgt;
            if (reverb) hgt = (0.35f + 0.65f * std::exp (-i / (1.0f + size * 5.0f))) * (i < 2 ? 0.75f + 0.25f * i : 1.0f);
            else        hgt = (i % 2 == 0 ? 1.0f : 0.55f) * std::pow (juce::jmax (0.15f, fb), i * 0.5f);
            hgt *= juce::jlimit (0.25f, 1.0f, 0.45f + mix * 2.0f) * (0.85f + 0.15f * level);
            const float x = bars.getX() + i * (bw + gap);
            const int dots = (int) (bars.getHeight() / (bw + 2.0f));
            const int litN = (int) std::round (hgt * dots);
            for (int k = 0; k < dots; ++k)
                AC::dotLed (g, x, bars.getBottom() - (k + 1) * (bw + 2.0f), bw, on && k < litN, reverb ? 0.15f + 0.5f * k / (float) dots : 0.5f + 0.5f * k / (float) dots);
        }
        juce::ignoreUnused (col);
        auto foot = h.reduced (12, 0).removeFromBottom (22);
        g.setColour (AC::muted); g.setFont (AC::caps (8.5f, 2));
        g.drawText (upper (tr (footKey, lang)), foot, juce::Justification::centredLeft);
        g.setColour (AC::lcdText); g.setFont (AC::mono (10.0f, true));
        g.drawText (footVal, foot, juce::Justification::centredRight);
    };
    static const char* styles[] = { "Nube", "Plate", "Sala", "Cuarto" };
    drawHalf (r.withWidth (half), "Reverb", purple, "Tipo", upper (tr (styles[juce::jlimit (0, 3, (int) pval (proc, PID::spaceStyle))], lang)), true);
    drawHalf (r.withTrimmedLeft (half), "Delay", pink, "Mezcla", juce::String ((int) pval (proc, PID::dlyMix)) + "%", false);
}

void SpaceDisplay::mouseUp (const juce::MouseEvent& e)
{
    // Clic en la mitad Reverb: siguiente tipo de sala
    if (e.position.x < getWidth() * 0.5f) proc.applyStyle (2, (int) pval (proc, PID::spaceStyle) + 1);
    repaint();
}

// ============================================================================
//  LedMeter
// ============================================================================
void LedMeter::paint (juce::Graphics& g)
{
    const float d = juce::jmin ((float) getHeight(), 6.0f);
    const int n = juce::jmax (8, (int) ((getWidth() + 3.0f) / (d + 3.0f)));
    const float step = (getWidth() - d) / (float) (n - 1);
    const float frac = juce::jlimit (0.0f, 1.0f, (shown + 48.0f) / 48.0f);
    const int lit = (int) std::round (frac * n);
    for (int i = 0; i < n; ++i)
        AC::dotLed (g, i * step, (getHeight() - d) * 0.5f, d, i < lit, i / (float) (n - 1));
}

// ============================================================================
//  DetailOverlay
// ============================================================================
void DetailOverlay::open (int module, int l)
{
    close();
    lang = l;
    struct K { const char* id; const char* key; };
    std::vector<K> ks;
    struct B { const char* id; juce::StringArray items; const char* key; };
    std::vector<B> bs;
    std::vector<std::pair<const char*, const char*>> ts;
    colour = purple;

    switch (module)
    {
        case 0: title = "Afinacion"; colour = pink;
            ks = { { PID::tuneSpeed, "Velocidad" }, { PID::tuneAmount, "Cantidad" }, { PID::tuneHuman, "Humanizar" } };
            bs = { { PID::tuneScale, scaleNames(), "Escala" }, { PID::tuneMode, { "Studio", "Live" }, "Modo" } };
            break;
        case 1: title = "Tono";
            ks = { { PID::hpfFreq, "Filtro graves" }, { PID::gateThresh, "Puerta" }, { PID::toneTilt, "Inclinacion" }, { PID::toneExciter, "Aire" },
                   { PID::lowFreq, "Graves|Frec" }, { PID::mudFreq, "Medios|Frec" }, { PID::presFreq, "Agudos|Frec" }, { PID::airFreq, "Aire|Frec" } };
            break;
        case 2: title = "Espacio";
            ks = { { PID::revDecay, "Tiempo" }, { PID::revSize, "Tamano" }, { PID::revDamp, "Oscuridad" }, { PID::revPre, "Pre-delay" },
                   { PID::dlyFeedback, "Repeticiones" }, { PID::spaceDuck, "Ducking" } };
            bs = { { PID::dlyDiv, delayDivNames(), "Division" } };
            ts = { { PID::dlyPingPong, "Ping-Pong" } };
            break;
        case 3: title = "Caracter"; colour = pink;
            ks = { { PID::satDrive, "Saturacion" }, { PID::satMix, "Mezcla" }, { PID::width, "Ancho" } };
            bs = { { PID::satType, { "Cinta", "Valvula", "Drive", "Lo-Fi" }, "Tipo" } };
            break;
        case 4: title = "Compresor"; colour = pink;
            ks = { { PID::compAmount, "Compresion" }, { PID::compRatio, "Ratio" }, { PID::compAttack, "Ataque" }, { PID::compRelease, "Liberacion" },
                   { PID::compMakeup, "Ganancia" }, { PID::compMix, "Mezcla" } };
            bs = { { PID::compStyle, { "Limpio", "Vintage", "Agresivo", "Serie" }, "Estilo" } };
            ts = { { PID::compAuto, "Auto" } };
            break;
        case 5: title = "De-esser";
            ks = { { PID::deessAmount, "Cantidad" }, { PID::deessFreq, "Frecuencia" } };
            break;
        default: title = "Doubler"; colour = pink;
            ks = { { PID::doubler, "Cantidad" } };
            bs = { { PID::doublerVoices, { "2", "4" }, "Voces" } };
            break;
    }

    for (auto& k : ks)
    {
        auto* u = items.add (new KnobUnit (proc.apvts, k.id, k.key, colour));
        u->lang = lang; u->knobSize = 70;
        const juce::String id = k.id;
        u->badge = [u, id] { return std::pair<juce::String, juce::String> { {}, fmt (id, u->slider.getValue()) }; };
        addAndMakeVisible (u);
    }
    boxCaptions.clear();
    for (auto& b : bs)
    {
        auto* cb = boxes.add (new juce::ComboBox());
        for (int i = 0; i < b.items.size(); ++i) cb->addItem (tr (b.items[i], lang), i + 1);
        boxAtts.add (new APVTS::ComboBoxAttachment (proc.apvts, b.id, *cb));
        boxCaptions.add (b.key);
        addAndMakeVisible (cb);
    }
    for (auto& t : ts)
    {
        auto* tb = toggles.add (new juce::ToggleButton (tr (t.second, lang)));
        toggleAtts.add (new APVTS::ButtonAttachment (proc.apvts, t.first, *tb));
        addAndMakeVisible (tb);
    }
    addAndMakeVisible (closeBtn);
    closeBtn.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        AC::roundButton (g, r.reduced (1.0f), over);
        auto c = r.withSizeKeepingCentre (10, 10);
        g.setColour (over ? AC::text : AC::sub);
        g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 1.6f);
        g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 1.6f);
    };
    closeBtn.onClick = [this] { close(); };
    setVisible (true);
    resized();
    toFront (false);
}

void DetailOverlay::resized()
{
    auto p = panelArea();
    closeBtn.setBounds (p.getRight() - 50, p.getY() + 10, 34, 34);
    auto inner = p.reduced (22).withTrimmedTop (36);
    if (! boxes.isEmpty() || ! toggles.isEmpty())
    {
        auto side = inner.removeFromRight (170);
        for (auto* b : boxes) { side.removeFromTop (18); b->setBounds (side.removeFromTop (30)); side.removeFromTop (8); }
        for (auto* t : toggles) { side.removeFromTop (8); t->setBounds (side.removeFromTop (28)); }
    }
    const int n = items.size();
    if (n == 0) return;
    const int perRow = n > 5 ? 4 : n;
    const int rows = (n + perRow - 1) / perRow;
    const int cw = juce::jmin (130, inner.getWidth() / perRow);
    const int rh = juce::jmin (175, inner.getHeight() / rows);
    inner = inner.withSizeKeepingCentre (inner.getWidth(), rh * rows);
    for (int i = 0; i < n; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        const int inRow = juce::jmin (perRow, n - row * perRow);
        const int x0 = inner.getX() + (inner.getWidth() - inRow * cw) / 2;
        items[i]->knobSize = rows > 1 ? 70 : 84;
        items[i]->setBounds (x0 + col * cw, inner.getY() + row * rh, cw, rh);
    }
}

void DetailOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.66f));
    auto p = panelArea().toFloat();
    for (int k = 1; k <= 10; ++k) { g.setColour (juce::Colours::black.withAlpha (0.06f)); g.drawRoundedRectangle (p.expanded ((float) k * 3.0f).translated (0, 8.0f), 22.0f + k * 3.0f, 3.0f); }
    g.setColour (AC::frame); g.fillRoundedRectangle (p, 22.0f);
    g.setColour (AC::frameEdge); g.drawRoundedRectangle (p.reduced (0.5f), 22.0f, 1.2f);
    auto head = p.reduced (24, 0).removeFromTop (50);
    g.setColour (AC::text); g.setFont (AC::sans (20.0f, 4));
    const auto ttl = tr (title, lang);
    g.drawText (ttl, head, juce::Justification::centredLeft);
    const float tw = AC::sans (20.0f, 4).getStringWidthFloat (ttl) + 12.0f;
    g.setColour (AC::sub); g.setFont (AC::caps (10.5f, 2));
    g.drawText (upper (tr ("Ajustes", lang)), head.withTrimmedLeft (tw), juce::Justification::centredLeft);
    juce::ignoreUnused (colour);
    g.setColour (AC::sub); g.setFont (AC::caps (9.5f, 2));
    for (int i = 0; i < boxes.size(); ++i)
        g.drawText (upper (tr (boxCaptions[i], lang)), boxes[i]->getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);
}

// ============================================================================
//  AssistWindow
// ============================================================================
AssistWindow::AssistWindow (ArtisVoxOneProcessor& p) : proc (p)
{
    bars.fill (0.0f); peaks.fill (0.0f);
    addAndMakeVisible (mainBtn); addAndMakeVisible (undoBtn); addAndMakeVisible (closeBtn);
    mainBtn.setClickingTogglesState (false);
    mainBtn.setToggleState (true, juce::dontSendNotification);   // degradado rosa
    mainBtn.onClick = [this]
    {
        if (state == Listening) { proc.assist.cancel(); state = Idle; }
        else if (state == Done && result.ok) { close(); return; }
        else startAnalysis();
        updateButtons(); repaint();
    };
    undoBtn.onClick = [this] { proc.undoAssist(); state = Idle; updateButtons(); repaint(); };
    closeBtn.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        AC::roundButton (g, r.reduced (1.0f), over);
        auto c = r.withSizeKeepingCentre (10, 10);
        g.setColour (over ? AC::text : AC::sub);
        g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 1.6f);
        g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 1.6f);
    };
    closeBtn.onClick = [this] { close(); };

    for (int i = 0; i < (int) assistGenres().size(); ++i)
    {
        auto* b = genreBtns.add (new DrawnButton());
        b->drawer = [this, i] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
        {
            const bool sel = proc.assistGenre == i;
            auto rr = r.reduced (4.0f, 4.0f);
            AC::pillBox (g, rr, sel, over);
            g.setColour (sel ? juce::Colours::white : (over ? AC::text : AC::sub));
            g.setFont (AC::sans (13.0f, sel ? 3 : 2));
            g.drawFittedText (tr (assistGenres()[(size_t) i].key, lang), rr.toNearestInt().reduced (6, 0), juce::Justification::centred, 1, 0.8f);
        };
        b->onClick = [this, i] { proc.assistGenre = i; for (auto* gb : genreBtns) gb->repaint(); };
        addChildComponent (b);
    }
}

void AssistWindow::open (int l)
{
    lang = l;
    if (state != Listening) { state = Idle; bars.fill (0.0f); peaks.fill (0.0f); }
    particles.clear();
    updateButtons();
    setVisible (true);
    toFront (true);
    startTimerHz (40);
}

void AssistWindow::close()
{
    if (state == Listening) proc.assist.cancel();
    state = Idle;
    stopTimer();
    setVisible (false);
}

void AssistWindow::startAnalysis()
{
    proc.startAssist (proc.assistGenre);
    state = Listening;
    particles.clear();
    result = {};
}

void AssistWindow::updateButtons()
{
    switch (state)
    {
        case Idle:      mainBtn.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\xa6  ")) + tr ("Analizar", lang) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String ((int) VocalAssist::listenSeconds) + " s"); break;
        case Listening: mainBtn.setButtonText (tr ("Cancelar", lang)); break;
        case Done:      mainBtn.setButtonText (tr (result.ok ? "Listo" : "Analizar", lang)); break;
    }
    undoBtn.setButtonText (tr ("Deshacer", lang));
    undoBtn.setVisible (state == Done && result.ok);
    for (auto* b : genreBtns) b->setVisible (state == Idle);
    resized();
}

juce::Rectangle<float> AssistWindow::specArea() const
{
    auto p = panel().toFloat().reduced (28.0f);
    p.removeFromTop (56.0f);
    return p.removeFromTop (state == Listening ? 220.0f : 150.0f);
}

void AssistWindow::resized()
{
    auto p = panel();
    closeBtn.setBounds (p.getRight() - 54, p.getY() + 16, 34, 34);
    auto bottom = p.reduced (28).removeFromBottom (48);
    if (undoBtn.isVisible())
    {
        mainBtn.setBounds (bottom.withSizeKeepingCentre (420, 48).removeFromRight (200));
        undoBtn.setBounds (bottom.withSizeKeepingCentre (420, 48).removeFromLeft (200));
    }
    else mainBtn.setBounds (bottom.withSizeKeepingCentre (300, 48));

    // Botones de genero (2 filas de 5) en la pantalla inicial
    auto ga = p.reduced (28).withTrimmedTop (56 + 150 + 62).withTrimmedBottom (52);
    const int cols = 5, rows = 2;
    const int cw = ga.getWidth() / cols, rh = juce::jmin (40, ga.getHeight() / rows);
    for (int i = 0; i < genreBtns.size(); ++i)
        genreBtns[i]->setBounds (ga.getX() + (i % cols) * cw, ga.getY() + (i / cols) * rh, cw, rh);
}

void AssistWindow::timerCallback()
{
    t += 1.0f / 60.0f;
    const double sr = proc.getCurrentSampleRate() > 0 ? proc.getCurrentSampleRate() : 48000.0;
    auto spec = specArea();

    // --- Espectro de la voz de entrada
    if (state == Listening)
    {
        proc.copyInput (fftData.data(), 2048);
        std::fill (fftData.begin() + 2048, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), 2048);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        const int nb = (int) bars.size();
        for (int i = 0; i < nb; ++i)
        {
            const double f0 = 60.0 * std::pow (14000.0 / 60.0, (double) i / nb);
            const double f1 = 60.0 * std::pow (14000.0 / 60.0, (double) (i + 1) / nb);
            const int b0 = juce::jlimit (1, 1023, (int) (f0 / sr * 2048.0)), b1 = juce::jlimit (b0 + 1, 1024, (int) (f1 / sr * 2048.0));
            float mx = 0; for (int b = b0; b < b1; ++b) mx = std::max (mx, fftData[(size_t) b]);
            float db = juce::Decibels::gainToDecibels (mx / 256.0f, -100.0f) + 3.0f * std::log2 (1.0f + i * 0.15f);
            const float v = juce::jlimit (0.0f, 1.0f, (db + 70.0f) / 70.0f);
            bars[(size_t) i] = v > bars[(size_t) i] ? v : bars[(size_t) i] * 0.9f + v * 0.1f;
            peaks[(size_t) i] = std::max (peaks[(size_t) i] - 0.006f, bars[(size_t) i]);
        }
        level = proc.inLevelDb.load();

        // Nacen estrellas en los picos del espectro
        const float energy = juce::jlimit (0.0f, 1.0f, (level + 50.0f) / 45.0f);
        const int spawn = (int) (energy * 5.0f) + (rng.nextFloat() < 0.3f ? 1 : 0);
        for (int k = 0; k < spawn && particles.size() < 420; ++k)
        {
            int i = rng.nextInt (nb);
            for (int tries = 0; tries < 3; ++tries) { const int j = rng.nextInt (nb); if (bars[(size_t) j] > bars[(size_t) i]) i = j; }
            const float x = spec.getX() + (i + 0.5f) * spec.getWidth() / nb;
            const float y = spec.getBottom() - bars[(size_t) i] * spec.getHeight() * 0.85f;
            Particle p;
            p.x = x + rng.nextFloat() * 6.0f - 3.0f; p.y = y;
            p.vx = (rng.nextFloat() - 0.5f) * 0.8f; p.vy = -(0.4f + rng.nextFloat() * 1.6f) * (0.6f + energy);
            p.maxLife = p.life = 0.8f + rng.nextFloat() * 1.8f;
            p.size = 1.2f + rng.nextFloat() * (rng.nextFloat() < 0.12f ? 4.5f : 2.0f);
            p.phase = rng.nextFloat() * 6.28f;
            const float m = rng.nextFloat();
            p.c = m < 0.45f ? pink : (m < 0.85f ? purple : juce::Colour (0xfffff0fb));
            particles.push_back (p);
        }

        VocalAssist::Result res;
        if (proc.pollAssistFinished (res))
        {
            result = res;
            state = Done;
            updateButtons();
            if (onFinished) onFinished();
        }
    }
    else
    {
        const int nb = (int) bars.size();
        for (int i = 0; i < nb; ++i)
        {
            // En reposo: ola suave que respira
            const float idle = state == Idle ? 0.06f + 0.05f * (0.5f + 0.5f * std::sin (t * 1.8f + i * 0.22f)) * (0.6f + 0.4f * std::sin (t * 0.7f + i * 0.05f)) : 0.0f;
            bars[(size_t) i] = std::max (idle, bars[(size_t) i] * 0.96f);
            peaks[(size_t) i] = std::max (peaks[(size_t) i] - 0.006f, bars[(size_t) i]);
        }
        // estrellas tranquilas de fondo
        if (particles.size() < 60 && rng.nextFloat() < 0.25f)
        {
            Particle p;
            p.x = spec.getX() + rng.nextFloat() * spec.getWidth(); p.y = spec.getY() + rng.nextFloat() * spec.getHeight();
            p.vx = 0; p.vy = -0.15f; p.maxLife = p.life = 2.0f + rng.nextFloat() * 2.0f;
            p.size = 1.0f + rng.nextFloat() * 2.0f; p.phase = rng.nextFloat() * 6.28f;
            p.c = rng.nextBool() ? pink : purple;
            particles.push_back (p);
        }
    }

    for (auto& p : particles) { p.x += p.vx; p.y += p.vy; p.vy *= 0.995f; p.life -= 1.0f / 60.0f; }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [&] (const Particle& p)
                     { return p.life <= 0 || p.y < spec.getY() - 20.0f; }), particles.end());
    repaint (panel().expanded (4));
}

void AssistWindow::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto p = panel().toFloat();
    for (int k = 1; k <= 10; ++k)   // halo morado barato (sin desenfoque por cada fotograma)
    {
        g.setColour (AC::purple.withAlpha (0.035f * (11 - k) / 10.0f));
        g.drawRoundedRectangle (p.expanded ((float) k * 3.0f), 24.0f + k * 3.0f, 3.0f);
    }
    g.setColour (AC::frame); g.fillRoundedRectangle (p, 24.0f);
    g.setColour (AC::frameEdge); g.drawRoundedRectangle (p.reduced (0.5f), 24.0f, 1.2f);
    for (auto c : { p.getTopLeft() + juce::Point<float> (16, 16), p.getTopRight() + juce::Point<float> (-16, 16),
                    p.getBottomLeft() + juce::Point<float> (16, -16), p.getBottomRight() + juce::Point<float> (-16, -16) })
        AC::screw (g, c, 6.0f);

    // Titulo
    static const juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    g.drawImage (logo, juce::Rectangle<float> (p.getX() + 30, p.getY() + 14, 44, 38), juce::RectanglePlacement::centred);
    {
        auto f = AC::sans (24.0f, 4);
        const float x0 = p.getX() + 84;
        g.setColour (AC::text); g.setFont (f);
        g.drawText ("Vocal", (int) x0, (int) p.getY() + 14, 100, 30, juce::Justification::centredLeft);
        const float w1 = f.getStringWidthFloat ("Vocal ") ;
        g.setGradientFill (juce::ColourGradient (AC::purple, x0 + w1, 0, AC::pink, x0 + w1 + 80, 0, false));
        g.drawText ("Assist", (int) (x0 + w1), (int) p.getY() + 14, 120, 30, juce::Justification::centredLeft);
        g.setColour (AC::sub); g.setFont (AC::caps (9.0f, 2));
        g.drawText (upper (tr ("Analisis de voz", lang)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 BY ARTIVOX")), (int) x0, (int) p.getY() + 42, 400, 14, juce::Justification::centredLeft);
    }

    auto spec = specArea();
    AC::lcdBox (g, spec, 14.0f, false);

    // Rejilla sutil
    g.setColour (juce::Colour (0xff1d1a24));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (spec.getY() + spec.getHeight() * i / 4.0f), spec.getX() + 8, spec.getRight() - 8);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (spec.toNearestInt());

        // Barras del espectro con brillo
        const int nb = (int) bars.size();
        const float bw = spec.getWidth() / nb;
        const float base = spec.getBottom() - 6.0f, hMax = spec.getHeight() * 0.85f;
        for (int i = 0; i < nb; ++i)
        {
            const float h = bars[(size_t) i] * hMax;
            if (h < 1.0f) continue;
            auto b = juce::Rectangle<float> (spec.getX() + i * bw + 1.2f, base - h, bw - 2.4f, h);
            g.setGradientFill (juce::ColourGradient (pink, 0, base - hMax, purple.withAlpha (0.6f), 0, base, false));
            g.setOpacity (0.25f); g.fillRoundedRectangle (b.expanded (2.0f, 2.0f), 2.0f);
            g.setOpacity (0.95f); g.fillRoundedRectangle (b, 1.5f);
            // picos
            const float py = base - peaks[(size_t) i] * hMax;
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.fillRect (b.getX(), py - 2.0f, b.getWidth(), 1.6f);
        }

        // Linea suave por encima
        juce::Path curve;
        for (int i = 0; i < nb; ++i)
        {
            const float x = spec.getX() + (i + 0.5f) * bw, y = base - bars[(size_t) i] * hMax - 6.0f;
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        if (state == Listening) glowStroke (g, curve.createPathWithRoundedCorners (8.0f), juce::Colour (0xffffb8e0), 1.4f);

        // Estrellas
        for (auto& pt : particles)
        {
            const float lifeA = juce::jlimit (0.0f, 1.0f, pt.life / pt.maxLife);
            const float tw = 0.6f + 0.4f * std::sin (t * 9.0f + pt.phase);
            const float a = lifeA * tw;
            const float r = pt.size;
            juce::ColourGradient glow (pt.c.withAlpha (0.55f * a), pt.x, pt.y, pt.c.withAlpha (0.0f), pt.x + r * 4.0f, pt.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (pt.x - r * 4.0f, pt.y - r * 4.0f, r * 8.0f, r * 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f * a));
            g.fillEllipse (pt.x - r * 0.5f, pt.y - r * 0.5f, r, r);
            if (r > 2.6f)   // destello en cruz
            {
                g.setColour (pt.c.brighter (0.5f).withAlpha (0.8f * a));
                g.drawLine (pt.x - r * 3.0f, pt.y, pt.x + r * 3.0f, pt.y, 1.0f);
                g.drawLine (pt.x, pt.y - r * 3.0f, pt.x, pt.y + r * 3.0f, 1.0f);
            }
        }
    }

    // Debajo del espectro: estado
    auto info = panel().toFloat().reduced (28.0f);
    info.removeFromTop (56.0f + spec.getHeight() + 16.0f);
    info.removeFromBottom (52.0f);

    if (state == Idle)
    {
        g.setColour (AC::text); g.setFont (AC::sans (15.0f, 3));
        g.drawText (tr ("Toca tu voz: analizando 15 s", lang), info.removeFromTop (22), juce::Justification::centred);
        g.setColour (AC::sub); g.setFont (AC::sans (12.0f, 1));
        g.drawText (tr ("AssistDesc", lang), info.removeFromTop (18), juce::Justification::centred);
    }
    else if (state == Listening)
    {
        const float prog = proc.assist.progress.load();
        {
            auto bar = info.removeFromTop (8.0f);
            const int n = 48; const float d = 6.0f, step = (bar.getWidth() - d) / (n - 1);
            for (int i = 0; i < n; ++i) AC::dotLed (g, bar.getX() + i * step, bar.getY(), d, i < (int) std::round (prog * n), i / (float) (n - 1));
        }
        info.removeFromTop (12.0f);

        const float hz = proc.getDetectedHz();
        juce::String note = "--";
        if (hz > 0)
        {
            const int midi = (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0));
            note = juce::String (noteNames[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
        }
        const bool refining = proc.isRefiningAssist();
        const juce::String vals[] = { level > -99 ? juce::String (level, 1) + " dB" : "-- dB", note,
                                      refining ? upper (tr ("Ajustando", lang)) + juce::String::repeatedString (".", 1 + (int) (t * 3) % 3)
                                               : juce::String ((int) std::ceil (VocalAssist::listenSeconds * (1.0 - prog))) + " s" };
        const char* keys[] = { "Nivel", "Nota", "Tiempo" };
        const float cw = info.getWidth() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto c = juce::Rectangle<float> (info.getX() + i * cw, info.getY(), cw, info.getHeight()).reduced (8, 0);
            g.setColour (AC::sub); g.setFont (AC::caps (9.5f, 2));
            g.drawText (upper (tr (keys[i], lang)), c.removeFromTop (16), juce::Justification::centred);
            auto lc = c.removeFromTop (34).reduced (14, 2);
            AC::lcdBox (g, lc, 10.0f, false);
            AC::glowText (g, vals[i], lc, AC::mono (17.0f));
        }
    }
    else // Done
    {
        if (! result.ok)
        {
            g.setColour (AC::text); g.setFont (AC::sans (15.0f, 3));
            g.drawText (tr ("No se detecto voz. Prueba otra vez.", lang), info, juce::Justification::centred);
        }
        else
        {
            g.setColour (AC::lcdText); g.setFont (AC::caps (11.5f, 3));
            const auto gname = upper (tr (assistGenres()[(size_t) juce::jlimit (0, (int) assistGenres().size() - 1, result.genre)].key, lang));
            g.drawText (upper (tr ("Listo. Ajustes aplicados.", lang)) + "  -  " + gname, info.removeFromTop (22), juce::Justification::centred);
            info.removeFromTop (8);
            auto val = [&] (const char* id) { return pval (proc, id); };
            juce::String keyTxt = "--";
            if (result.key >= 0) keyTxt = juce::String (noteNames[result.key]) + " " + tr (result.scale == 0 ? "Mayor" : "Menor", lang);
            const std::pair<juce::String, juce::String> items[] = {
                { tr ("Tonalidad", lang), keyTxt },
                { tr ("Entrada", lang), fmt (PID::inGain, val ("inGain")) },
                { tr ("Filtro graves", lang), fmt (PID::hpfFreq, val ("hpfFreq")) },
                { tr ("Compresion", lang), fmt (PID::compAmount, val ("compAmount")) + "  " + fmt (PID::compRatio, val ("compRatio")) },
                { tr ("De-esser", lang), fmt (PID::deessAmount, val ("deessAmount")) },
                { tr ("Brillo", lang), fmt (PID::airGain, val ("airGain")) } };
            const float cw = info.getWidth() / 3.0f, rh = info.getHeight() / 2.0f;
            for (int i = 0; i < 6; ++i)
            {
                auto c = juce::Rectangle<float> (info.getX() + (i % 3) * cw, info.getY() + (i / 3) * rh, cw, rh).reduced (6, 3);
                AC::lcdBox (g, c, 10.0f, false);
                g.setColour (AC::sub); g.setFont (AC::caps (8.5f, 2));
                g.drawText (upper (items[i].first), c.reduced (12, 0).removeFromTop (c.getHeight() * 0.48f), juce::Justification::bottomLeft);
                AC::glowText (g, items[i].second, c.reduced (12, 0).removeFromBottom (c.getHeight() * 0.52f), AC::mono (13.0f), juce::Justification::topLeft);
            }
        }
    }
}

// ============================================================================
//  Editor
// ============================================================================
ArtisVoxOneEditor::ArtisVoxOneEditor (ArtisVoxOneProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    lang = proc.getLanguage();

    // ---- Cabecera (estilo ArtisComp)
    auto roundIcon = [this] (DrawnButton& b, std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> icon)
    {
        b.drawer = [icon] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
        {
            auto rr = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) - 2.0f, juce::jmin (r.getWidth(), r.getHeight()) - 2.0f);
            AC::roundButton (g, rr, over);
            icon (g, rr, over ? AC::text : AC::sub);
        };
        addAndMakeVisible (b);
    };
    auto undoIcon = [] (bool redo)
    {
        return [redo] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            auto cc = r.getCentre();
            juce::Path p;
            p.addCentredArc (cc.x, cc.y + 1, 6.5f, 6.5f, 0, juce::degreesToRadians (redo ? 60.0f : -60.0f), juce::degreesToRadians (redo ? -150.0f : 150.0f), true);
            const float ax = cc.x + (redo ? 5.6f : -5.6f), ay = cc.y - 3.0f;
            p.startNewSubPath (ax + (redo ? -4.0f : 4.0f), ay - 2.0f); p.lineTo (ax, ay); p.lineTo (ax + (redo ? -1.0f : 1.0f), ay + 4.0f);
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
    };
    roundIcon (undoBtn, undoIcon (false));
    roundIcon (redoBtn, undoIcon (true));
    roundIcon (plusBtn, [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        auto cc = r.getCentre(); g.setColour (c);
        g.drawLine (cc.x - 6, cc.y, cc.x + 6, cc.y, 1.8f); g.drawLine (cc.x, cc.y - 6, cc.x, cc.y + 6, 1.8f);
    });
    prevBtn.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, true,  over ? AC::text : AC::lcdText); };
    nextBtn.drawer = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool) { drawTri (g, r, false, over ? AC::text : AC::lcdText); };
    addAndMakeVisible (prevBtn); addAndMakeVisible (nextBtn);

    assistBtn.drawer = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        auto rr = r.reduced (1.0f);
        AC::pillBox (g, rr, true, over);
        if (assistRunning)
        {
            auto pr = rr.reduced (rr.getHeight() * 0.5f, 0).withHeight (3.0f).withY (rr.getBottom() - 7.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (pr, 1.5f);
            g.setColour (juce::Colours::white); g.fillRoundedRectangle (pr.withWidth (pr.getWidth() * assistProgress), 1.5f);
        }
        g.setColour (juce::Colours::white);
        g.setFont (AC::sans (14.0f, 3));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\xa6 ")) + tr ("Assist", lang), rr, juce::Justification::centred);
    };
    addAndMakeVisible (assistBtn);

    abBtn.drawer = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
    {
        auto rr = r.reduced (1.0f);
        AC::pillBox (g, rr, false, over);
        const bool b = proc.getABSlot() == 1;
        const float d = rr.getHeight() - 8.0f;
        for (int i = 0; i < 2; ++i)
        {
            auto c = juce::Rectangle<float> (d, d).withCentre ({ i == 0 ? rr.getX() + 4.0f + d * 0.5f : rr.getRight() - 4.0f - d * 0.5f, rr.getCentreY() });
            const bool sel = (i == 1) == b;
            if (sel) AC::pillBox (g, c, true, false, d * 0.5f);
            g.setColour (sel ? juce::Colours::white : AC::sub);
            g.setFont (AC::sans (14.0f, 3));
            g.drawText (i == 0 ? "A" : "B", c, juce::Justification::centred);
        }
    };
    addAndMakeVisible (abBtn);

    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    plusBtn.onClick = [this] { savePresetDialog(); };
    abBtn.onClick   = [this] { proc.toggleAB(); repaint(); };
    assistBtn.onClick = [this] { assistWin.open (lang); };
    assistWin.onFinished = [this]
    {
        statusMsg = tr ("Listo. Ajustes aplicados.", lang);
        statusTimer = 30 * 8;
        refreshPresetBox();
        repaint();
    };

    addAndMakeVisible (presetBox);
    presetBox.getProperties().set ("bare", true);
    presetBox.getProperties().set ("noArrow", true);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) proc.loadFactoryPreset (id - 1);
        else if (auto files = proc.getUserPresets(); juce::isPositiveAndBelow (id - 1001, files.size()))
            proc.loadUserPreset (files[id - 1001]);
        repaint();
    };

    // Idioma: desplegable "ES" como en ArtisComp
    for (int i = 0; i < 3; ++i)
    {
        auto* b = langBtns.add (new DrawnButton());
        if (i > 0) { addChildComponent (b); continue; }
        b->drawer = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
        {
            const char* names[] = { "ES", "EN", "PT" };
            auto rr = r.reduced (1.0f);
            AC::pillBox (g, rr, false, over);
            g.setColour (AC::text); g.setFont (AC::sans (13.5f, 3));
            g.drawText (names[juce::jlimit (0, 2, lang)], rr.withTrimmedRight (14.0f), juce::Justification::centred);
            juce::Path a; const float cx = rr.getRight() - 14.0f, cy = rr.getCentreY();
            a.startNewSubPath (cx - 3.5f, cy - 1.5f); a.lineTo (cx, cy + 2.0f); a.lineTo (cx + 3.5f, cy - 1.5f);
            g.setColour (AC::sub); g.strokePath (a, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        b->onClick = [this, b]
        {
            juce::PopupMenu m;
            m.setLookAndFeel (&lnf);
            const char* full[] = { "ES  Español", "EN  English", "PT  Português (BR)" };
            for (int k = 0; k < 3; ++k) m.addItem (k + 1, juce::String (juce::CharPointer_UTF8 (full[k])), true, lang == k);
            juce::Component::SafePointer<ArtisVoxOneEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (b), [safe] (int res)
            {
                if (safe == nullptr || res <= 0) return;
                safe->lang = res - 1; safe->proc.setLanguage (res - 1); safe->applyLanguage();
            });
        };
        addAndMakeVisible (b);
    }

    bypassBtn.setClickingTogglesState (true);
    bypassBtn.getProperties().set ("seg", true);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, PID::before, bypassBtn);
    addAndMakeVisible (bypassBtn);

    // ---- Tarjetas
    cardTune  = std::make_unique<ModuleCard> (proc, 0, "Afinacion", pink,   PID::tuneOn,  PID::tuneStyle,  juce::StringArray { "Rapido", "Natural", "Suave", "Robot" });
    cardTone  = std::make_unique<ModuleCard> (proc, 1, "Tono",      purple, PID::toneOn,  PID::toneStyle,  juce::StringArray { "Brillante", "Calido", "Oscuro", "Neutro" });
    cardSpace = std::make_unique<ModuleCard> (proc, 2, "Espacio",   purple, PID::spaceOn, PID::spaceStyle, juce::StringArray { "Nube", "Plate", "Sala", "Cuarto" });
    cardChar  = std::make_unique<ModuleCard> (proc, 3, "Caracter",  pink,   PID::charOn,  PID::satType,    juce::StringArray { "Cinta", "Valvula", "Drive", "Lo-Fi" });
    int mi = 0;
    for (auto* c : { cardTune.get(), cardTone.get(), cardSpace.get(), cardChar.get() })
    {
        c->onSettings = [this, mi] { overlay.open (mi, lang); };
        addAndMakeVisible (c); ++mi;
    }

    // ---- Tiras
    stripComp    = std::make_unique<StripBar> (proc, "Compresor", pink,   PID::compOn);
    stripDeess   = std::make_unique<StripBar> (proc, "De-esser",  purple, PID::deessOn);
    stripDoubler = std::make_unique<StripBar> (proc, "Doubler",   pink,   PID::doublerOn);
    stripComp->badge  = [this] { return "-" + juce::String (proc.getCompGrDb(), 1) + " dB"; };
    stripDeess->badge = [this] { return "-" + juce::String (proc.getDeessGrDb(), 1) + " dB"; };
    stripDoubler->badge = [this] { return juce::String ((int) pval (proc, PID::doublerVoices) == 1 ? 4 : 2) + " " + upper (tr ("Voces", lang)); };
    stripDoubler->onBadgeClick = [this] { proc.setParamValue (PID::doublerVoices, (int) pval (proc, PID::doublerVoices) == 1 ? 0.0f : 1.0f); };
    stripComp->onSettings    = [this] { overlay.open (4, lang); };
    stripDeess->onSettings   = [this] { overlay.open (5, lang); };
    stripDoubler->onSettings = [this] { overlay.open (6, lang); };
    for (auto* s : { stripComp.get(), stripDeess.get(), stripDoubler.get() }) addAndMakeVisible (s);

    // ---- Panel Afinacion
    addAndMakeVisible (keyBox);
    kSpeed = std::make_unique<KnobUnit> (proc.apvts, PID::tuneSpeed, "Velocidad", pink);
    kHuman = std::make_unique<KnobUnit> (proc.apvts, PID::tuneHuman, "Humanizar", pink);
    kSpeed->badge = [this] { return std::pair<juce::String, juce::String> { "MS", juce::String ((int) std::round (pval (proc, PID::tuneSpeed))) }; };
    kHuman->badge = [this] { return std::pair<juce::String, juce::String> { upper (tr ("Nivel", lang)), juce::String ((int) pval (proc, PID::tuneHuman)) + "%" }; };

    // ---- Panel EQ
    addAndMakeVisible (eqDisplay);
    const char* eqIds[] = { PID::lowGain, PID::mudGain, PID::presGain, PID::airGain };
    for (int i = 0; i < 4; ++i)
    {
        auto& s = eqSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::trackColourId, purple);
        eqAtts[i] = std::make_unique<APVTS::SliderAttachment> (proc.apvts, eqIds[i], s);
        s.setDoubleClickReturnValue (true, 0.0);
        s.onValueChange = [this] { repaint (panelRect (1).toFloat().transformedBy (juce::AffineTransform::translation ((float) B, (float) B).scaled (getWidth() / (float) TW)).getSmallestIntegerContainer().expanded (2)); };
        addAndMakeVisible (s);
    }

    // ---- Panel Espacio
    addAndMakeVisible (spaceDisplay);
    kReverb = std::make_unique<KnobUnit> (proc.apvts, PID::revMix, "Reverb", purple);
    kDelay  = std::make_unique<KnobUnit> (proc.apvts, PID::dlyMix, "Delay", purple);
    kReverb->badge = [this] { return std::pair<juce::String, juce::String> { upper (tr ("Tiempo", lang)), juce::String (pval (proc, PID::revDecay), 1) + "s" }; };
    kDelay->badge  = [this] { return std::pair<juce::String, juce::String> { upper (tr ("Ritmo", lang)), delayDivNames()[(int) pval (proc, PID::dlyDiv)].removeCharacters (" ") }; };
    kDelay->onBadgeClick = [this] { proc.setParamValue (PID::dlyDiv, (float) (((int) pval (proc, PID::dlyDiv) + 1) % delayDivNames().size())); kDelay->repaint(); };

    // ---- Panel Caracter
    kSat   = std::make_unique<KnobUnit> (proc.apvts, PID::satDrive, "Saturacion", pink);
    kWidth = std::make_unique<KnobUnit> (proc.apvts, PID::width, "Ancho", purple);
    kSat->badge = [this]
    {
        static const char* t[] = { "Cinta", "Valvula", "Drive", "Lo-Fi" };
        return std::pair<juce::String, juce::String> { upper (tr ("Modo", lang)), upper (tr (t[juce::jlimit (0, 3, (int) pval (proc, PID::satType))], lang)) };
    };
    kSat->onBadgeClick = [this] { proc.applyStyle (3, (int) pval (proc, PID::satType) + 1); repaint(); };
    kWidth->badge = [this] { return std::pair<juce::String, juce::String> { "ST", juce::String ((int) pval (proc, PID::width)) + "%" }; };

    for (auto* k : { kSpeed.get(), kHuman.get(), kReverb.get(), kDelay.get(), kSat.get(), kWidth.get() })
        addAndMakeVisible (k);

    // ---- Pie
    inMeter.source  = [this] { return proc.inLevelDb.load(); };
    outMeter.source = [this] { return juce::jmax (proc.outLevelL.load(), proc.outLevelR.load()); };
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
    inAtt  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, PID::inGain, inGain);
    outAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, PID::outGain, outGain);
    mixAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, PID::mix, mixValue);
    inGain.textFromValueFunction  = [] (double v) { return fmt (PID::inGain, v); };
    outGain.textFromValueFunction = [] (double v) { return fmt (PID::outGain, v); };
    mixValue.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + "%"; };
    mixValue.prefix = "MIX ";
    for (auto* d : { &inGain, &outGain, &mixValue })
    {
        d->setDoubleClickReturnValue (true, d == &mixValue ? 100.0 : 0.0);
        d->setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        d->onValueChange = [d] { d->repaint(); };
        addAndMakeVisible (d);
    }

    addChildComponent (overlay);
    addChildComponent (assistWin);

    applyLanguage();
    refreshPresetBox();

    setResizable (true, true);
    setResizeLimits (TW * 3 / 4, TH * 3 / 4, TW * 2, TH * 2);
    getConstrainer()->setFixedAspectRatio ((double) TW / TH);
    setSize (TW, TH);
    startTimerHz (30);
}

ArtisVoxOneEditor::~ArtisVoxOneEditor()
{
    stopTimer();
    overlay.close();
    assistWin.close();
    setLookAndFeel (nullptr);
}

void ArtisVoxOneEditor::applyLanguage()
{
    for (auto* c : { cardTune.get(), cardTone.get(), cardSpace.get(), cardChar.get() }) { c->lang = lang; c->repaint(); }
    for (auto* s : { stripComp.get(), stripDeess.get(), stripDoubler.get() }) { s->lang = lang; s->repaint(); }
    for (auto* k : { kSpeed.get(), kHuman.get(), kReverb.get(), kDelay.get(), kSat.get(), kWidth.get() }) { k->lang = lang; k->repaint(); }
    keyBox.lang = lang; eqDisplay.lang = lang; spaceDisplay.lang = lang;
    undoBtn.setTooltip (tr ("Deshacer", lang)); redoBtn.setTooltip (tr ("Rehacer", lang));
    plusBtn.setTooltip (tr ("Guardar preset", lang));
    bypassBtn.setTooltip (tr ("Antes / Despues de toda la cadena", lang));
    abBtn.setTooltip (tr ("Compara dos ajustes", lang));
    assistBtn.setTooltip (tr ("Analiza tu voz 15 s y ajusta la cadena", lang));
    bypassBtn.setButtonText (tr ("Antes", lang) + "|" + tr ("Despues", lang));
    if (overlay.isVisible()) overlay.close();
    assistWin.lang = lang;
    refreshPresetBox();
    repaint();
}

void ArtisVoxOneEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& fp = factoryPresets();
    presetBox.addSectionHeading (tr ("Fabrica", lang));
    for (int i = 0; i < (int) fp.size(); ++i) presetBox.addItem (fp[(size_t) i].name, i + 1);
    auto user = proc.getUserPresets();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (tr ("Usuario", lang));
        for (int i = 0; i < user.size(); ++i) presetBox.addItem (user[i].getFileNameWithoutExtension(), 1001 + i);
    }
    presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
    presetCategory = upper (tr ("Fabrica", lang));
    for (auto& f : user) if (f.getFileNameWithoutExtension() == proc.currentPresetName) { presetCategory = upper (tr ("Usuario", lang)); break; }
    if (proc.currentPresetName.startsWithIgnoreCase ("Vocal Assist") || proc.currentPresetName.startsWithIgnoreCase ("Assist")) presetCategory = "VOCAL ASSIST";
    repaint (getLocalBounds().withHeight (getHeight() / 7));
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == proc.currentPresetName)
            presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
}

void ArtisVoxOneEditor::stepPreset (int dir)
{
    const int n = presetBox.getNumItems();
    if (n == 0) return;
    int idx = presetBox.getSelectedItemIndex();
    idx = idx < 0 ? 0 : (idx + dir + n) % n;
    presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

void ArtisVoxOneEditor::savePresetDialog()
{
    auto* aw = new juce::AlertWindow (tr ("Guardar preset", lang), tr ("Nombre del preset", lang), juce::MessageBoxIconType::NoIcon, this);
    aw->setLookAndFeel (&lnf);
    aw->addTextEditor ("name", proc.currentPresetName);
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("X", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<ArtisVoxOneEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            safe->proc.saveUserPreset (aw->getTextEditorContents ("name"));
            safe->refreshPresetBox();
        }
    }), true);
}

void ArtisVoxOneEditor::timerCallback()
{
    inMeter.update(); outMeter.update();
    keyBox.update();
    eqDisplay.updateAnalyzer();
    spaceDisplay.level = juce::jlimit (0.0f, 1.0f, (juce::jmax (proc.outLevelL.load(), proc.outLevelR.load()) + 40.0f) / 40.0f);
    spaceDisplay.repaint();
    stripComp->repaint(); stripDeess->repaint();

    // Agrupar cambios para Deshacer / Rehacer
    if (++undoTick >= 10)
    {
        undoTick = 0;
        if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            proc.undoManager.beginNewTransaction();
    }

    assistRunning = assistWin.isListening();
    assistProgress = proc.assist.progress.load();
    if (assistRunning) assistBtn.repaint();
    else if (statusTimer > 0 && --statusTimer == 0) { statusMsg = {}; repaint(); }

    if (proc.currentPresetName != shownPreset && ! presetBox.isPopupActive())
    {
        shownPreset = proc.currentPresetName;
        refreshPresetBox();
    }

    static int slow = 0;
    if (++slow >= 6)
    {
        slow = 0;
        for (auto* c : { cardTune.get(), cardTone.get(), cardSpace.get(), cardChar.get() }) c->repaint();
        for (auto* k : { kSpeed.get(), kHuman.get(), kReverb.get(), kDelay.get(), kSat.get(), kWidth.get() }) k->repaint();
        stripDoubler->repaint(); abBtn.repaint();
        repaint (panelRect (1).toFloat().transformedBy (juce::AffineTransform::translation ((float) B, (float) B).scaled (getWidth() / (float) TW)).getSmallestIntegerContainer().expanded (2));
    }
}

// ============================================================================
void ArtisVoxOneEditor::parentHierarchyChanged()
{
   #if JUCE_WINDOWS
    // Direct2D (JUCE 8) bloquea FL Studio en algunos equipos: usamos el render por software (GDI)
    if (auto* peer = getPeer())
        if (peer->getAvailableRenderingEngines().size() > 1 && peer->getCurrentRenderingEngine() != 0)
            peer->setCurrentRenderingEngine (0);
   #endif
}

void ArtisVoxOneEditor::paint (juce::Graphics& g)
{
    // Fondo fijo pre-renderizado una vez (marco, logo, paneles...) para que la interfaz sea ligera
    const float phys = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int cw = juce::roundToInt (getWidth() * phys), ch = juce::roundToInt (getHeight() * phys);
    if (cw <= 0 || ch <= 0) return;
    if (bgCache.getWidth() != cw || bgCache.getHeight() != ch || bgLang != lang)
    {
        bgCache = juce::Image (juce::Image::RGB, cw, ch, true, juce::SoftwareImageType());
        juce::Graphics cg (bgCache);
        cg.addTransform (juce::AffineTransform::scale (cw / (float) TW));
        paintStatic (cg);
        bgLang = lang;
    }
    g.drawImage (bgCache, getLocalBounds().toFloat());

    // Partes que cambian
    g.addTransform (juce::AffineTransform::translation ((float) B, (float) B).scaled (getWidth() / (float) TW));
    {
        auto lr = presetLcd().toFloat();
        g.setColour (AC::lcdText.withAlpha (0.55f)); g.setFont (AC::mono (10.5f, false, 0.18f));
        g.drawText (presetCategory.isNotEmpty() ? presetCategory : upper (tr ("Fabrica", lang)), lr.withHeight (30).translated (0, 6), juce::Justification::centred);
    }
    for (int i = 0; i < 4; ++i)
    {
        auto b = eqSliders[i].getBounds().toFloat();
        const double v = eqSliders[i].getValue();
        auto vr = juce::Rectangle<float> (b.getCentreX() - 28, b.getY() - 30, 56, 24);
        AC::glowText (g, (v > 0.049 ? "+" : "") + juce::String (v, 1), vr, AC::mono (11.5f));
    }
    {
        auto help = juce::Rectangle<float> (384, 714, 290, 46);
        const auto ay = tr ("Ayuda", lang);
        const float aw = AC::sans (13.5f, 3).getStringWidthFloat (ay) + 12.0f;
        g.setColour (statusMsg.isNotEmpty() ? AC::text : AC::sub); g.setFont (AC::sans (12.0f, 1));
        g.drawFittedText (statusMsg.isNotEmpty() ? statusMsg : tr ("HelpTip", lang), help.withTrimmedLeft (aw).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    }
}

void ArtisVoxOneEditor::paintStatic (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050507));

    // Borde exterior de goma / cuero gris (estilo ArtisEQ Pro)
    {
        static const juce::Image leather = makeLeather (TW, TH);
        auto outer = juce::Rectangle<float> (0.0f, 0.0f, (float) TW, (float) TH).reduced (1.0f);
        juce::Path op; op.addRoundedRectangle (outer, 38.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (op);
            g.drawImageAt (leather, 0, 0);
            // relieve: luz arriba, sombra abajo
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, 0, juce::Colours::transparentWhite, 0, (float) B * 1.6f, false));
            g.fillRect (outer);
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, (float) TH - B * 1.8f, juce::Colours::black.withAlpha (0.40f), 0, (float) TH, false));
            g.fillRect (outer);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), 0, 0, juce::Colours::transparentBlack, (float) B * 1.4f, 0, false));
            g.fillRect (outer.withWidth ((float) B * 1.5f));
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, (float) TW - B * 1.4f, 0, juce::Colours::black.withAlpha (0.25f), (float) TW, 0, false));
            g.fillRect (outer.withTrimmedLeft ((float) TW - B * 1.5f));
        }
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.strokePath (op, juce::PathStrokeType (1.4f));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (outer.expanded (0.5f), 38.5f, 1.0f);

        // Surco donde encaja la placa
        auto groove = juce::Rectangle<float> ((float) B + 6.0f, (float) B + 6.0f, (float) W - 12.0f, (float) H - 12.0f).expanded (3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (groove.translated (0, 1.5f).expanded (1.0f), 31.0f, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawRoundedRectangle (groove, 31.0f, 3.0f);
    }
    g.addTransform (juce::AffineTransform::translation ((float) B, (float) B));

    // Marco exterior con tornillos
    auto frame = juce::Rectangle<float> (6.0f, 6.0f, (float) W - 12.0f, (float) H - 12.0f);
    {
        juce::Path fp; fp.addRoundedRectangle (frame, 28.0f);
        AC::shadow (g, fp, 18.0f, 0.8f, { 0, 4 });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1a21), 0, frame.getY(), juce::Colour (0xff141318), 0, frame.getBottom(), false));
        g.fillPath (fp);
        g.setColour (AC::frameEdge); g.strokePath (fp, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawRoundedRectangle (frame.reduced (2.0f), 26.0f, 1.0f);
    }
    for (auto p : { juce::Point<float> (22, 22), { (float) W - 22, 22 }, { 22, (float) H - 22 }, { (float) W - 22, (float) H - 22 } })
        AC::screw (g, p, 6.5f);

    // Logo + titulo integrados en la placa
    {
        static const juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
        const float lh = 48.0f, lw = lh * logo.getWidth() / (float) logo.getHeight();
        auto lr = juce::Rectangle<float> (lw, lh).withCentre ({ 34.0f + lw * 0.5f, 54.0f });

        // halo morado suave detras del logo (como luz sobre la placa)
        g.setGradientFill (juce::ColourGradient (AC::purple.withAlpha (0.16f), lr.getCentreX(), lr.getCentreY() + 4.0f,
                                                 AC::purple.withAlpha (0.0f), lr.getCentreX() + lw * 0.85f, lr.getCentreY() + 4.0f, true));
        g.fillEllipse (juce::Rectangle<float> (lw * 1.7f, lw * 1.7f).withCentre (lr.getCentre().translated (0.0f, 4.0f)));
        // sombra proyectada
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::scale (lw / logo.getWidth()).translated (lr.getX(), lr.getY() + 2.5f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 9, { 0, 0 }).drawForImage (g, logo);
        }
        g.drawImage (logo, lr, juce::RectanglePlacement::centred);

        // separador vertical grabado
        const float dx = lr.getRight() + 16.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), dx, 26.0f, juce::Colours::black.withAlpha (0.0f), dx, 84.0f, false));
        {
            juce::ColourGradient cg (juce::Colours::transparentBlack, dx, 26.0f, juce::Colours::transparentBlack, dx, 84.0f, false);
            cg.addColour (0.5, juce::Colours::black.withAlpha (0.9f));
            g.setGradientFill (cg); g.fillRect (dx, 26.0f, 1.0f, 58.0f);
            juce::ColourGradient hg (juce::Colours::transparentWhite, dx, 26.0f, juce::Colours::transparentWhite, dx, 84.0f, false);
            hg.addColour (0.5, juce::Colours::white.withAlpha (0.10f));
            g.setGradientFill (hg); g.fillRect (dx + 1.0f, 26.0f, 1.0f, 58.0f);
        }

        // Nombre: grabado (sombra abajo + luz arriba)
        const float tx = dx + 16.0f;
        auto f = AC::sans (31.0f, 3, -0.005f);
        auto tr1 = juce::Rectangle<float> (tx, 24.0f, 240.0f, 40.0f);
        const float w1 = f.getStringWidthFloat ("ArtisVox");
        g.setFont (f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawText ("ArtisVox", tr1.translated (0, 1.5f), juce::Justification::centredLeft);
        g.drawText ("One", tr1.translated (w1 + 9.0f, 1.5f), juce::Justification::centredLeft);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), 0, tr1.getY() + 8.0f, juce::Colour (0xffbdb8c8), 0, tr1.getBottom() - 6.0f, false));
        g.drawText ("ArtisVox", tr1, juce::Justification::centredLeft);
        {
            juce::ColourGradient og (juce::Colour (0xff9d6bff), tx + w1 + 9.0f, 0, AC::pink, tx + w1 + 70.0f, 0, false);
            g.setGradientFill (og);
            g.drawText ("One", tr1.translated (w1 + 9.0f, 0), juce::Justification::centredLeft);
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.setFont (AC::caps (8.6f, 2));
        const auto sub = upper (tr ("Cadena vocal profesional", lang)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 BY ARTIVOX"));
        g.drawText (sub, (int) tx + 1, 67, 270, 16, juce::Justification::centredLeft);
        g.setColour (AC::sub);
        g.drawText (sub, (int) tx + 1, 66, 270, 16, juce::Justification::centredLeft);
    }

    // Pantalla de presets (2 filas)
    {
        auto lr = presetLcd().toFloat();
        AC::lcdBox (g, lr, 14.0f);
    }

    // Paneles
    for (int i = 0; i < 4; ++i) AC::panelBox (g, panelRect (i).toFloat(), 18.0f);

    // Valores y nombres de los deslizadores de EQ
    const char* eqKeys[] = { "Graves", "Medios", "Agudos", "Aire" };
    for (int i = 0; i < 4; ++i)
    {
        auto b = eqSliders[i].getBounds().toFloat();
        auto vr = juce::Rectangle<float> (b.getCentreX() - 28, b.getY() - 30, 56, 24);
        AC::lcdBox (g, vr, 8.0f, false);
        g.setColour (AC::text); g.setFont (AC::sans (13.0f, 2));
        g.drawText (tr (eqKeys[i], lang), juce::Rectangle<float> (b.getCentreX() - 40, b.getBottom() + 4, 80, 20), juce::Justification::centred);
    }

    // Pie
    auto foot = juce::Rectangle<float> (28, 714, (float) W - 56, 46);
    AC::panelBox (g, foot, 16.0f);
    g.setColour (AC::sub); g.setFont (AC::caps (9.0f, 3));
    g.drawText ("IN", 44, 714, 30, 46, juce::Justification::centredLeft);
    g.drawText ("OUT", (int) foot.getRight() - 52, 714, 40, 46, juce::Justification::centredLeft);
    {
        auto help = juce::Rectangle<float> (384, 714, 290, 46);
        g.setColour (AC::pink); g.setFont (AC::sans (13.5f, 3));
        const auto ay = tr ("Ayuda", lang);
        g.drawText (ay, help, juce::Justification::centredLeft);
        g.setColour (AC::muted); g.setFont (AC::sans (12.0f, 2));
        g.drawText ("artivoxaudio.com", juce::Rectangle<float> (660, 714, 132, 46), juce::Justification::centredRight);
    }
}

void ArtisVoxOneEditor::resized()
{
    const float sc = getWidth() / (float) TW;
    for (auto* c : getChildren()) c->setTransform (juce::AffineTransform::translation ((float) B, (float) B).scaled (sc));

    // Cabecera
    auto lcd = presetLcd();
    prevBtn.setBounds (lcd.getX() + 8, lcd.getY() + 28, 26, 34);
    nextBtn.setBounds (lcd.getRight() - 34, lcd.getY() + 28, 26, 34);
    presetBox.setBounds (lcd.getX() + 36, lcd.getY() + 28, lcd.getWidth() - 72, 34);
    plusBtn.setBounds (lcd.getRight() + 8, 37, 38, 38);
    bypassBtn.setBounds (722, 35, 150, 42);
    abBtn.setBounds (880, 35, 78, 42);
    undoBtn.setBounds (964, 37, 38, 38);
    redoBtn.setBounds (1006, 37, 38, 38);
    assistBtn.setBounds (1052, 35, 64, 42);
    langBtns[0]->setBounds (1120, 35, 54, 42);

    // Tarjetas y tiras
    cardTune->setBounds (cardRect (0)); cardTone->setBounds (cardRect (1));
    cardSpace->setBounds (cardRect (2)); cardChar->setBounds (cardRect (3));
    stripComp->setBounds (stripRect (0)); stripDeess->setBounds (stripRect (1));
    stripDoubler->setBounds (stripRect (2));

    // Panel Afinacion
    auto p0 = panelRect (0);
    keyBox.setBounds (p0.getX() + 12, p0.getY() + 12, p0.getWidth() - 24, 120);
    kSpeed->setBounds (p0.getX() + 8, p0.getY() + 166, 130, 150);
    kHuman->setBounds (p0.getRight() - 138, p0.getY() + 166, 130, 150);

    // Panel EQ
    auto p1 = panelRect (1);
    eqDisplay.setBounds (p1.getX() + 12, p1.getY() + 12, p1.getWidth() - 24, 128);
    for (int i = 0; i < 4; ++i)
    {
        const int cx = p1.getX() + (int) std::round (p1.getWidth() * (i + 0.5f) / 4.0f);
        eqSliders[i].setBounds (cx - 14, p1.getY() + 186, 28, 122);
    }

    // Panel Espacio
    auto p2 = panelRect (2);
    spaceDisplay.setBounds (p2.getX() + 12, p2.getY() + 12, p2.getWidth() - 24, 128);
    kReverb->setBounds (p2.getX() + 8, p2.getY() + 166, 130, 150);
    kDelay->setBounds (p2.getRight() - 138, p2.getY() + 166, 130, 150);

    // Panel Caracter
    auto p3 = panelRect (3);
    kSat->setBounds (p3.getX() + 8, p3.getY() + 96, 130, 150);
    kWidth->setBounds (p3.getRight() - 138, p3.getY() + 96, 130, 150);
    for (auto* k : { kSpeed.get(), kHuman.get(), kReverb.get(), kDelay.get(), kSat.get(), kWidth.get() }) { k->knobSize = 92; k->resized(); }

    // Pie
    inGain.setBounds (72, 722, 76, 30);
    inMeter.setBounds (160, 734, 200, 6);
    mixValue.setBounds (806, 722, 92, 30);
    outMeter.setBounds (910, 734, 120, 6);
    outGain.setBounds (1040, 722, 76, 30);

    overlay.setBounds (-B, -B, TW, TH);
    assistWin.setBounds (-B, -B, TW, TH);
}
