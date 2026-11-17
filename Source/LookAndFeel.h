#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Colores heredados (se usan en varios sitios)
namespace AVColours
{
    const juce::Colour bg       = AC::outerBg;
    const juce::Colour panel    = AC::panel;
    const juce::Colour panel2   = AC::lcd;
    const juce::Colour line     = AC::panelEdge;
    const juce::Colour pink     = AC::pink;
    const juce::Colour purple   = AC::purple;
    const juce::Colour text     = AC::text;
    const juce::Colour dim      = AC::sub;
    const juce::Colour green    { 0xff3ce0a0 };
}

// ============================================================================
//  LookAndFeel con la estetica de ArtisComp Pro
// ============================================================================
class ArtiVoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArtiVoxLookAndFeel()
    {
        using namespace AC;
        setColour (juce::ResizableWindow::backgroundColourId, outerBg);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, pill);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, pillEdge);
        setColour (juce::ComboBox::arrowColourId, sub);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15141a));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, pink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, purple.withAlpha (0.55f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, pill);
        setColour (juce::TextButton::buttonOnColourId, purple);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, lcd);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, pillEdge);
        setColour (juce::TextEditor::focusedOutlineColourId, pink);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff15141a));
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, pillEdge);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff15141a));
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, pink);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        auto& F = AC::Fonts::get();
        if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName()) return F.monoBold;
        return f.isBold() ? F.semibold : F.medium;
    }

    // ---- Perilla ArtisComp: anillo de puntos + cuerpo negro + valor dentro
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float size = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float R = size * 0.5f;
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && (bool) s.getProperties().getWithDefault ("bipolar", true);
        const int dots = juce::jlimit (16, 34, (int) (R * 0.55f));

        AC::dotRing (g, c, R * 0.92f, startA, endA, pos, dots, juce::jmax (1.4f, R * 0.035f), bipolar);

        const float angle = startA + pos * (endA - startA);
        const float kr = R * 0.70f;
        AC::knobBody (g, c, kr, angle);

        // Valor dentro de la perilla
        if ((bool) s.getProperties().getWithDefault ("showValue", true) && kr > 18.0f)
        {
            auto t = s.getTextFromValue (s.getValue()).replace (" ", "");
            const float fh = juce::jlimit (9.0f, 22.0f, kr * (t.length() > 5 ? 0.30f : 0.38f));
            AC::glowText (g, t, juce::Rectangle<float> (c.x - kr * 0.72f, c.y - fh * 0.6f, kr * 1.44f, fh * 1.2f), AC::mono (fh), juce::Justification::centred,
                          s.isEnabled() ? AC::lcdText : AC::muted);
        }
    }

    // ---- Deslizador vertical: columna de LEDs en puntos + tirador negro
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cx = x + w * 0.5f;
        const float top = (float) y + 8.0f, bot = (float) (y + h) - 8.0f;
        const float d = 5.0f, step = 8.0f;
        const int n = (int) ((bot - top) / step);
        const float zeroY = s.getMinimum() < 0 && s.getMaximum() > 0 ? (float) s.getPositionOfValue (0.0) : bot;
        for (int i = 0; i <= n; ++i)
        {
            const float yy = bot - i * step;
            const bool on = (yy >= juce::jmin (pos, zeroY) - 1.0f && yy <= juce::jmax (pos, zeroY) + 1.0f);
            AC::dotLed (g, cx - d * 0.5f, yy - d * 0.5f, d, on, (float) i / (float) n);
        }
        // tirador
        juce::Rectangle<float> th (cx - 11.0f, pos - 11.0f, 22.0f, 22.0f);
        juce::Path p; p.addEllipse (th);
        AC::shadow (g, p, 7.0f, 0.9f, { 0, 3 });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff45424d), th.getX(), th.getY(), juce::Colour (0xff0d0c10), th.getX(), th.getBottom(), false));
        g.fillEllipse (th);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawEllipse (th.reduced (0.6f), 1.0f);
        g.setColour (juce::Colours::white); g.fillEllipse (th.withSizeKeepingCentre (5.0f, 5.0f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if ((bool) b.getProperties()["seg"])
        {
            // Selector de dos posiciones (Antes | Despues): la activa es una pastilla degradada
            AC::pillBox (g, r, false, over);
            const bool first = b.getToggleState();
            auto half = r.reduced (4.0f);
            auto sel = first ? half.removeFromLeft (half.getWidth() * 0.5f) : half.removeFromRight (half.getWidth() * 0.5f);
            AC::pillBox (g, sel, true, false);
            return;
        }
        AC::pillBox (g, r, b.getToggleState() || down, over);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        if ((bool) b.getProperties()["seg"])
        {
            auto parts = juce::StringArray::fromTokens (b.getButtonText(), "|", "");
            auto r = b.getLocalBounds().toFloat().reduced (5.0f);
            const bool first = b.getToggleState();
            for (int i = 0; i < 2 && i < parts.size(); ++i)
            {
                auto h = i == 0 ? r.withWidth (r.getWidth() * 0.5f) : r.withTrimmedLeft (r.getWidth() * 0.5f);
                const bool on = (i == 0) == first;
                g.setColour (on ? juce::Colours::white : AC::sub);
                g.setFont (AC::sans (13.0f, on ? 3 : 2));
                g.drawFittedText (parts[i], h.toNearestInt(), juce::Justification::centred, 1, 0.8f);
            }
            return;
        }
        g.setColour (AC::text);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 2, 0.9f);
    }

    // Interruptor: pastilla morada con bola blanca
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        auto sw = r.removeFromRight (46.0f).withSizeKeepingCentre (44.0f, 24.0f);
        if (on) AC::pillBox (g, sw, true, over);
        else { g.setColour (juce::Colour (0xff1c1b21)); g.fillRoundedRectangle (sw, 12.0f); g.setColour (AC::pillEdge); g.drawRoundedRectangle (sw.reduced (0.5f), 12.0f, 1.0f); }
        auto knob = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ on ? sw.getRight() - 12.0f : sw.getX() + 12.0f, sw.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (knob.translated (0, 1.5f));
        g.setColour (on ? juce::Colours::white : AC::sub); g.fillEllipse (knob);
        g.setColour (AC::text);
        g.setFont (AC::sans (13.5f, 1));
        g.drawText (b.getButtonText(), r.withTrimmedRight (8.0f), juce::Justification::centredLeft);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        if ((bool) box.getProperties()["bare"]) {}
        else if ((bool) box.getProperties()["lcd"]) AC::lcdBox (g, r.reduced (3.0f), 10.0f);
        else AC::pillBox (g, r, false, box.isMouseOver (true), juce::jmin (12.0f, r.getHeight() * 0.5f));
        if (! (bool) box.getProperties()["noArrow"])
        {
            juce::Path a; const float cx = w - 16.0f, cy = h * 0.5f;
            a.startNewSubPath (cx - 4, cy - 2); a.lineTo (cx, cy + 2); a.lineTo (cx + 4, cy - 2);
            g.setColour (AC::sub); g.strokePath (a, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        const bool lcd = (bool) box.getProperties()["lcd"] || (bool) box.getProperties()["bare"];
        label.setBounds ((bool) box.getProperties()["noArrow"] ? box.getLocalBounds().reduced (4, 0) : box.getLocalBounds().reduced (12, 0).withTrimmedRight (14));
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (lcd ? juce::Justification::centred : juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (0.7f);
        if (lcd) label.setColour (juce::Label::textColourId, AC::lcdText);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        if ((bool) box.getProperties()["bare"]) return AC::mono (19.0f);
        if ((bool) box.getProperties()["lcd"]) return AC::mono (15.0f);
        return AC::sans (13.5f, 2);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (0xff15141a));
        g.setColour (AC::pillEdge); g.drawRect (0, 0, w, h, 1);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return AC::sans (juce::jmin (14.0f, h * 0.42f), 2); }
    juce::Font getPopupMenuFont() override { return AC::sans (14.5f, 1); }
    juce::Font getAlertWindowTitleFont() override { return AC::sans (17.0f, 3); }
    juce::Font getAlertWindowMessageFont() override { return AC::sans (14.0f, 1); }
    juce::Font getAlertWindowFont() override { return AC::sans (13.0f, 1); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (AC::mono (12.0f));
        l->setColour (juce::Label::textColourId, AC::lcdText);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return l;
    }

    void drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body) override
    {
        AC::lcdBox (g, body.reduced (2.0f), 6.0f, false);
    }
    juce::Font getSliderPopupFont (juce::Slider&) override { return AC::mono (13.0f); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
};
