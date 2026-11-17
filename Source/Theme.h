#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// ============================================================================
//  Estetica ArtisComp Pro (ArtiVox): carbon oscuro, morado -> rosa, perillas
//  negras con anillo de puntos, pantallas tipo LCD con reflejo de cristal,
//  botones pastilla y esferas de luz.
// ============================================================================
namespace AC
{
    // ---- Colores
    const juce::Colour outerBg   { 0xff0c0b0f };
    const juce::Colour frame     { 0xff17161c };
    const juce::Colour frameEdge { 0xff2b2932 };
    const juce::Colour panel     { 0xff131217 };
    const juce::Colour panelEdge { 0xff24222a };
    const juce::Colour lcd       { 0xff09080c };
    const juce::Colour lcdEdge   { 0xff2e2b36 };
    const juce::Colour pill      { 0xff1c1b21 };
    const juce::Colour pillEdge  { 0xff2c2a33 };
    const juce::Colour text      { 0xfff3f1f7 };
    const juce::Colour sub       { 0xff9b97a6 };
    const juce::Colour muted     { 0xff686470 };
    const juce::Colour dotOff    { 0xff35323c };
    const juce::Colour purple    { 0xff8b5cf6 };
    const juce::Colour violet    { 0xffa855f7 };
    const juce::Colour pink      { 0xffe94fd2 };
    const juce::Colour lcdText   { 0xfff26bd9 };

    inline juce::Colour grad (float t) { return purple.interpolatedWith (pink, juce::jlimit (0.0f, 1.0f, t)); }

    // ---- Tipografias incrustadas (Inter + JetBrains Mono, licencia OFL)
    // Se borra al apagar JUCE (no en la descarga de la DLL) para no bloquear el DAW al quitar el plugin
    struct Fonts : private juce::DeletedAtShutdown
    {
        juce::Typeface::Ptr regular, medium, semibold, bold, xbold, monoMed, monoBold;
        static Fonts& get() { return *getInstance(); }
        ~Fonts() override { clearSingletonInstance(); }
        JUCE_DECLARE_SINGLETON_INLINE (Fonts, false)

        Fonts()
        {
            auto mk = [] (const void* d, int n) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); };
            regular  = mk (BinaryData::InterRegular_otf,       BinaryData::InterRegular_otfSize);
            medium   = mk (BinaryData::InterMedium_otf,        BinaryData::InterMedium_otfSize);
            semibold = mk (BinaryData::InterSemiBold_otf,      BinaryData::InterSemiBold_otfSize);
            bold     = mk (BinaryData::InterBold_otf,          BinaryData::InterBold_otfSize);
            xbold    = mk (BinaryData::InterExtraBold_otf,     BinaryData::InterExtraBold_otfSize);
            monoMed  = mk (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);
            monoBold = mk (BinaryData::JetBrainsMonoBold_ttf,  BinaryData::JetBrainsMonoBold_ttfSize);
        }
    };

    // weight: 0 regular, 1 medium, 2 semibold, 3 bold, 4 extrabold
    inline juce::Font sans (float h, int weight = 2, float kerning = 0.0f)
    {
        auto& F = Fonts::get();
        juce::Typeface::Ptr tf = weight <= 0 ? F.regular : weight == 1 ? F.medium : weight == 2 ? F.semibold : weight == 3 ? F.bold : F.xbold;
        return juce::Font (juce::FontOptions (tf).withHeight (h)).withExtraKerningFactor (kerning);
    }
    inline juce::Font mono (float h, bool bold = true, float kerning = 0.0f)
    {
        auto& F = Fonts::get();
        return juce::Font (juce::FontOptions (bold ? F.monoBold : F.monoMed).withHeight (h)).withExtraKerningFactor (kerning);
    }
    // Rotulo de seccion: mayusculas espaciadas
    inline juce::Font caps (float h, int weight = 2) { return sans (h, weight, 0.22f); }

    // ---- Dibujo
    inline void shadow (juce::Graphics& g, const juce::Path& p, float radius, float alpha = 0.7f, juce::Point<int> off = { 0, 3 })
    {
        juce::DropShadow (juce::Colours::black.withAlpha (alpha), (int) radius, off).drawForPath (g, p);
    }

    // Recuadro de funcion: mas oscuro que la placa y ligeramente hundido
    inline void panelBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 18.0f)
    {
        juce::Path p; p.addRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0c10), r.getX(), r.getY(), juce::Colour (0xff09080b), r.getX(), r.getBottom(), false));
        g.fillPath (p);
        {
            // sombra interior arriba (efecto hundido)
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 10.0f, false));
            g.fillRect (r.withHeight (12.0f));
        }
        g.setColour (juce::Colour (0xff050407));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        // filo de luz inferior (borde biselado hacia la placa)
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) r.getBottom(), r.getX() + radius, r.getRight() - radius);
    }

    // Pantalla OLED: negro puro con leve viñeta
    inline void oledBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 16.0f)
    {
        juce::Path p; p.addRoundedRectangle (r, radius);
        g.setColour (juce::Colour (0xff1a1920)); g.fillRoundedRectangle (r.expanded (2.5f), radius + 2.5f);   // marco de la pantalla
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (r.expanded (2.5f), radius + 2.5f, 1.0f);
        g.setColour (juce::Colour (0xff020203)); g.fillPath (p);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        // brillo residual del panel (las OLED no son negro absoluto en el centro)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0a10), r.getCentreX(), r.getCentreY(),
                                                 juce::Colour (0xff010102), r.getX(), r.getY(), true));
        g.fillPath (p);
    }

    // Reflejo de cristal suave (se pinta por encima del contenido)
    inline void glassSheen (juce::Graphics& g, juce::Rectangle<float> r, float radius = 16.0f)
    {
        juce::Path p; p.addRoundedRectangle (r, radius);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        // banda diagonal
        juce::Path band;
        band.startNewSubPath (r.getX(), r.getY());
        band.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
        band.lineTo (r.getX() + r.getWidth() * 0.18f, r.getBottom());
        band.lineTo (r.getX(), r.getBottom());
        band.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.065f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.45f, r.getY() + r.getHeight() * 0.55f, false));
        g.fillPath (band);
        // brillo superior
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0, r.getY(), juce::Colours::transparentWhite, 0, r.getY() + r.getHeight() * 0.22f, false));
        g.fillRect (r);
        // filo del cristal
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 0.8f);
    }

    // Pantalla oscura con reflejo de cristal
    inline void lcdBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f, bool glare = true)
    {
        juce::Path p; p.addRoundedRectangle (r, radius);
        g.setColour (juce::Colour (0xff1e1c24)); g.fillRoundedRectangle (r.expanded (3.0f), radius + 3.0f);
        g.setColour (juce::Colour (0xff08070a)); g.drawRoundedRectangle (r.expanded (3.0f), radius + 3.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141219), r.getX(), r.getY(), lcd, r.getX(), r.getBottom(), false));
        g.fillPath (p);
        // sombra interior
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 14.0f, false));
        g.fillPath (p);
        if (glare)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            juce::Path gl;
            gl.addEllipse (r.getX() - r.getWidth() * 0.05f, r.getY() - r.getHeight() * 0.9f, r.getWidth() * 0.7f, r.getHeight() * 1.25f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX() + r.getWidth() * 0.2f, r.getY(),
                                                     juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.45f, false));
            g.fillPath (gl);
        }
        g.setColour (lcdEdge); g.drawRoundedRectangle (r, radius, 1.0f);
    }

    // Texto LCD rosa con brillo
    inline void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f,
                          juce::Justification j = juce::Justification::centred, juce::Colour c = lcdText)
    {
        g.setFont (f);
        g.setColour (c.withAlpha (0.18f));
        for (auto o : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
            g.drawText (t, r.translated (o.x, o.y), j, false);
        g.setColour (c);
        g.drawText (t, r, j, false);
    }

    // Boton pastilla oscuro / seleccionado (degradado morado -> rosa)
    inline void pillBox (juce::Graphics& g, juce::Rectangle<float> r, bool selected, bool over, float radius = -1.0f)
    {
        if (radius < 0) radius = r.getHeight() * 0.5f;
        if (selected)
        {
            juce::Path p; p.addRoundedRectangle (r, radius);
            juce::DropShadow (pink.withAlpha (0.35f), 12, {}).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (purple, r.getX(), r.getY(), pink, r.getRight(), r.getBottom(), false));
            g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (over ? juce::Colour (0xff25232b) : juce::Colour (0xff1f1e24), r.getX(), r.getY(),
                                                     juce::Colour (0xff17161b), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, radius);
            g.setColour (pillEdge); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        }
    }

    inline void roundButton (juce::Graphics& g, juce::Rectangle<float> r, bool over)
    {
        auto c = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        g.setGradientFill (juce::ColourGradient (over ? juce::Colour (0xff27252d) : juce::Colour (0xff1f1e24), c.getX(), c.getY(),
                                                 juce::Colour (0xff141318), c.getX(), c.getBottom(), false));
        g.fillEllipse (c);
        g.setColour (pillEdge); g.drawEllipse (c.reduced (0.5f), 1.0f);
    }

    // Esfera de luz brillante (encendido)
    inline void sphere (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
    {
        auto s = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.45f), s.getCentreX(), s.getCentreY(), c.withAlpha (0.0f), s.getRight() + s.getWidth() * 0.4f, s.getCentreY(), true));
            g.fillEllipse (s.expanded (s.getWidth() * 0.45f));
        }
        g.setColour (juce::Colour (0xff0b0a0e)); g.fillEllipse (s.expanded (2.0f));
        const auto base = on ? c : juce::Colour (0xff2a2830);
        juce::ColourGradient body (base.brighter (on ? 0.6f : 0.3f), s.getX() + s.getWidth() * 0.35f, s.getY() + s.getHeight() * 0.3f,
                                   base.darker (on ? 0.5f : 0.6f), s.getRight(), s.getBottom(), true);
        g.setGradientFill (body); g.fillEllipse (s);
        auto hl = s.reduced (s.getWidth() * 0.22f).withHeight (s.getHeight() * 0.32f).translated (0, -s.getHeight() * 0.12f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.75f : 0.25f), hl.getCentreX(), hl.getY(),
                                                 juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false));
        g.fillEllipse (hl);
    }

    inline void screw (juce::Graphics& g, juce::Point<float> c, float r = 9.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (c.x - r, c.y - r + 1.5f, r * 2, r * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6e6b75), c.x - r, c.y - r, juce::Colour (0xff1e1d22), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colour (0xff111014));
        g.drawLine (c.x - r * 0.6f, c.y + r * 0.35f, c.x + r * 0.6f, c.y - r * 0.35f, 2.0f);
    }

    // Anillo de puntos alrededor de una perilla
    inline void dotRing (juce::Graphics& g, juce::Point<float> c, float radius, float startA, float endA, float pos, int n, float dotR, bool bipolar = false)
    {
        const float mid = 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1);
            const float a = startA + t * (endA - startA);
            const float x = c.x + std::sin (a) * radius, y = c.y - std::cos (a) * radius;
            bool active = bipolar ? ((pos >= mid && t >= mid - 0.001f && t <= pos + 0.001f) || (pos < mid && t <= mid + 0.001f && t >= pos - 0.001f))
                                  : t <= pos + 0.001f;
            if (active)
            {
                const auto col = grad (t);
                g.setColour (col.withAlpha (0.25f)); g.fillEllipse (x - dotR * 2.0f, y - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
                g.setColour (col); g.fillEllipse (x - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
            }
            else
            {
                g.setColour (dotOff); g.fillEllipse (x - dotR * 0.75f, y - dotR * 0.75f, dotR * 1.5f, dotR * 1.5f);
            }
        }
    }

    // Perilla negra brillante (cuerpo + cara + indicador)
    inline void knobBody (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        juce::Path body; body.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
        shadow (g, body, r * 0.5f, 0.85f, { 0, (int) (r * 0.12f) });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3842), c.x, c.y - r, juce::Colour (0xff0a090c), c.x, c.y + r, false));
        g.fillPath (body);
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawEllipse (c.x - r + 1.2f, c.y - r + 1.2f, r * 2 - 2.4f, r * 2 - 2.4f, 1.0f);

        // cara interior oscura con brillo morado abajo
        const float fr = r * 0.74f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050407), c.x, c.y - fr, juce::Colour (0xff131119), c.x, c.y + fr, false));
        g.fillEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2);
        juce::Path rim; rim.addCentredArc (c.x, c.y, fr - 1.0f, fr - 1.0f, 0.0f, juce::MathConstants<float>::pi * 0.62f, juce::MathConstants<float>::pi * 1.38f, true);
        g.setColour (purple.withAlpha (0.55f)); g.strokePath (rim, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colours::black); g.drawEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2, 1.0f);
        // reflejo horizontal sutil
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (c.x - fr * 0.9f, c.y - 0.5f, fr * 1.8f, 1.0f);

        // indicador: punto blanco en el borde
        const float ir = r * 0.87f;
        const float dx = c.x + std::sin (angle) * ir, dy = c.y - std::cos (angle) * ir;
        const float dr = juce::jmax (2.2f, r * 0.075f);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillEllipse (dx - dr * 2, dy - dr * 2, dr * 4, dr * 4);
        g.setColour (juce::Colours::white); g.fillEllipse (dx - dr, dy - dr, dr * 2, dr * 2);
    }

    // Columna/fila de LEDs en puntos
    inline void dotLed (juce::Graphics& g, float x, float y, float d, bool on, float t)
    {
        if (on)
        {
            const auto col = grad (t);
            g.setColour (col.withAlpha (0.3f)); g.fillEllipse (x - d * 0.5f, y - d * 0.5f, d * 2.0f, d * 2.0f);
            g.setColour (col); g.fillEllipse (x, y, d, d);
        }
        else { g.setColour (dotOff); g.fillEllipse (x + d * 0.1f, y + d * 0.1f, d * 0.8f, d * 0.8f); }
    }

    inline void gearIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        const auto cc = r.getCentre();
        const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.30f;
        juce::Path p;
        for (int i = 0; i < 8; ++i)
        {
            const float a = i * juce::MathConstants<float>::twoPi / 8.0f;
            juce::Path tooth; tooth.addRoundedRectangle (-R * 0.22f, -R * 1.32f, R * 0.44f, R * 0.5f, 1.0f);
            tooth.applyTransform (juce::AffineTransform::rotation (a).translated (cc));
            p.addPath (tooth);
        }
        g.setColour (c);
        g.fillPath (p);
        g.drawEllipse (cc.x - R, cc.y - R, R * 2, R * 2, R * 0.38f);
        g.setColour (c); g.drawEllipse (cc.x - R * 0.35f, cc.y - R * 0.35f, R * 0.7f, R * 0.7f, 1.4f);
    }
}
