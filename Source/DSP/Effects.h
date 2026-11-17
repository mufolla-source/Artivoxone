#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace avdsp
{
// ---------------------------------------------------------------------------
// ArtisColor: saturacion analogica profesional (se procesa sobremuestreada x4)
//  0 Cinta:    pre-enfasis/de-enfasis de agudos (como una grabadora), compresion
//              suave, "head bump" en graves y perdida de agudos segun la carga
//  1 Valvula:  triodo con polarizacion dinamica -> armonicos pares, calidez
//  2 Drive:    transformador: satura mas los graves, armonicos impares, mordida
//  3 Lo-Fi:    banda limitada, menos bits y menos frecuencia de muestreo
//  Compensacion automatica de nivel: subir el Color no sube el volumen.
// ---------------------------------------------------------------------------
class ColorProcessor
{
public:
    void prepare (double s, int maxBlock, int osLatency)
    {
        sr = s;
        using C = juce::dsp::IIR::Coefficients<float>;
        auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
        tapePre.coefficients  = C::makeHighShelf (sr, 3000.0f, 0.6f, g (5.0f));
        tapePost.coefficients = C::makeHighShelf (sr, 3000.0f, 0.6f, g (-5.0f));
        bump.coefficients     = C::makePeakFilter (sr, 90.0f, 0.9f, g (1.5f));
        hfLoss.coefficients   = C::makeLowPass (sr, juce::jmin (15000.0f, (float) sr * 0.45f), 0.6f);
        xfPre.coefficients    = C::makeLowShelf (sr, 180.0f, 0.6f, g (5.0f));
        xfPost.coefficients   = C::makeLowShelf (sr, 180.0f, 0.6f, g (-5.0f));
        lofiHp.coefficients   = C::makeHighPass (sr, 280.0f, 0.7f);
        lofiLp.coefficients   = C::makeLowPass (sr, 5200.0f, 0.7f);
        for (auto* f : { &tapePre, &tapePost, &bump, &hfLoss, &xfPre, &xfPost, &lofiHp, &lofiLp }) f->reset();
        dc.prepare (sr);
        dryDelay.assign ((size_t) juce::nextPowerOfTwo (osLatency + maxBlock + 8), 0.0f);
        dMask = (int) dryDelay.size() - 1; dW = 0; lat = osLatency;
        dry.assign ((size_t) maxBlock + 8, 0.0f);
        rmsIn = rmsOut = 1e-6f; comp = 1.0f; tubeEnv = 0.0f; holdV = 0.0f; holdCnt = 0;
    }

    // Antes de sobremuestrear: guarda la senal limpia y aplica el pre-enfasis
    void pre (float* x, int n, int type, float drive)
    {
        for (int i = 0; i < n; ++i)
        {
            dryDelay[(size_t) dW] = x[i];
            dry[(size_t) i] = dryDelay[(size_t) ((dW - lat) & dMask)];
            dW = (dW + 1) & dMask;
            const float a = x[i] * x[i];
            rmsIn = a + (rmsIn - a) * 0.9995f;
            if (drive <= 0.01f) continue;
            if (type == 0) x[i] = tapePre.processSample (x[i]);
            else if (type == 2) x[i] = xfPre.processSample (x[i]);
        }
    }

    // En el dominio sobremuestreado
    void shape (float* x, int n, int type, float drivePct)
    {
        if (drivePct <= 0.01f) return;
        const float g = juce::Decibels::decibelsToGain (drivePct * 0.26f) * 0.8f;
        for (int i = 0; i < n; ++i)
        {
            const float v = x[i] * g;
            float y;
            switch (type)
            {
                case 0: y = std::tanh (v * 0.9f) / 0.9f; break;                                         // cinta
                case 1:
                {
                    tubeEnv = std::abs (v) > tubeEnv ? std::abs (v) : tubeEnv * 0.9998f;
                    const float bias = 0.12f + 0.25f * juce::jmin (1.0f, tubeEnv);
                    y = std::tanh (v + bias) - std::tanh (bias);                                          // valvula
                    break;
                }
                case 2: y = (v + 0.06f * v * v) / std::pow (1.0f + std::pow (std::abs (v), 2.4f), 1.0f / 2.4f); break;   // transformador
                default: y = std::tanh (v * 1.2f); break;                                               // lo-fi
            }
            x[i] = y / g;
        }
    }

    // Despues de bajar la frecuencia: de-enfasis, caracter, compensacion y mezcla
    void post (float* x, int n, int type, float drivePct, float mixPct)
    {
        const float mix = mixPct * 0.01f;
        const float d = drivePct * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            float y = x[i];
            if (drivePct > 0.01f)
            {
                if (type == 0) { y = tapePost.processSample (y); y += (bump.processSample (y) - y) * d; y += (hfLoss.processSample (y) - y) * d; }
                else if (type == 2) y = xfPost.processSample (y);
                else if (type == 3)
                {
                    // menos frecuencia de muestreo (~16 kHz) y 10 bits
                    if (holdCnt++ % juce::jmax (1, (int) std::round (sr / 16000.0)) == 0) holdV = y;
                    y = std::round (holdV * 512.0f) / 512.0f;
                    y = lofiLp.processSample (lofiHp.processSample (y));
                }
            }
            x[i] = y;
        }
        dc.process (x, n);
        for (int i = 0; i < n; ++i)
        {
            const float a = x[i] * x[i];
            rmsOut = a + (rmsOut - a) * 0.9995f;
            const float target = drivePct > 0.01f ? juce::jlimit (0.5f, 2.0f, std::sqrt ((rmsIn + 1e-9f) / (rmsOut + 1e-9f))) : 1.0f;
            comp = target + (comp - target) * 0.9997f;
            const float wet = x[i] * comp;
            x[i] = dry[(size_t) i] + (wet - dry[(size_t) i]) * mix;
        }
    }

private:
    double sr = 44100.0;
    juce::dsp::IIR::Filter<float> tapePre, tapePost, bump, hfLoss, xfPre, xfPost, lofiHp, lofiLp;
    struct Dc { float x1 = 0, y1 = 0, R = 0.995f;
        void prepare (double s) { R = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 10.0 / s); x1 = y1 = 0; }
        void process (float* x, int n) { for (int i = 0; i < n; ++i) { const float y = x[i] - x1 + R * y1; x1 = x[i]; y1 = y; x[i] = y; } } } dc;
    std::vector<float> dryDelay, dry; int dMask = 0, dW = 0, lat = 0;
    float rmsIn = 1e-6f, rmsOut = 1e-6f, comp = 1.0f, tubeEnv = 0.0f, holdV = 0.0f; int holdCnt = 0;
};

// Bloqueador de continua
struct DcBlocker
{
    float x1 = 0, y1 = 0, R = 0.995f;
    void prepare (double sr) { R = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 10.0 / sr); x1 = y1 = 0; }
    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i) { const float y = x[i] - x1 + R * y1; x1 = x[i]; y1 = y; x[i] = y; }
    }
};

// ---------------------------------------------------------------------------
// Excitador de agudos ("Aire"): genera armonicos solo en la banda alta
// ---------------------------------------------------------------------------
class Exciter
{
public:
    void prepare (double s)
    {
        sr = s;
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 3500.0f, 0.707f);
        hp1.coefficients = c; hp2.coefficients = c; hp1.reset(); hp2.reset();
    }
    void process (float* x, int n, float amountPct)
    {
        if (amountPct <= 0.01f) return;
        const float a = amountPct * 0.01f * 0.6f;
        for (int i = 0; i < n; ++i)
        {
            float h = hp1.processSample (x[i]);
            h = std::tanh (h * 5.0f) * 0.25f;
            h = hp2.processSample (h);
            x[i] += h * a;
        }
    }
private:
    double sr = 44100.0;
    juce::dsp::IIR::Filter<float> hp1, hp2;
};

// ---------------------------------------------------------------------------
// Doblador estereo: dos copias con micro-retardo y modulacion lenta
// ---------------------------------------------------------------------------
class Doubler
{
public:
    void prepare (double s)
    {
        sr = s;
        size = juce::nextPowerOfTwo ((int) (0.05 * sr) + 8);
        buf.assign ((size_t) size, 0.0f); w = 0; ph1 = 0; ph2 = 0.37;
    }

    // mono -> suma sobre L y R
    void process (const float* mono, float* l, float* r, int n, float amountPct, bool fourVoices = false)
    {
        const float amt = amountPct * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) w] = mono[i];
            if (amt > 0.001f)
            {
                const double dl = (0.0135 + 0.0012 * std::sin (2.0 * juce::MathConstants<double>::pi * ph1)) * sr;
                const double dr = (0.0205 + 0.0015 * std::sin (2.0 * juce::MathConstants<double>::pi * ph2)) * sr;
                const float tl = read (dl), tr = read (dr);
                l[i] = l[i] * (1.0f - 0.25f * amt) + tl * 0.7f * amt;
                r[i] = r[i] * (1.0f - 0.25f * amt) + tr * 0.7f * amt;
                if (fourVoices)
                {
                    const double d3 = (0.0095 + 0.0010 * std::sin (2.0 * juce::MathConstants<double>::pi * (ph2 + 0.5))) * sr;
                    const double d4 = (0.0275 + 0.0018 * std::sin (2.0 * juce::MathConstants<double>::pi * (ph1 + 0.25))) * sr;
                    const float t3 = read (d3), t4 = read (d4);
                    l[i] += (t4 * 0.45f + t3 * 0.15f) * amt;
                    r[i] += (t3 * 0.45f + t4 * 0.15f) * amt;
                }
            }
            ph1 += 0.31 / sr; if (ph1 >= 1.0) ph1 -= 1.0;
            ph2 += 0.23 / sr; if (ph2 >= 1.0) ph2 -= 1.0;
            w = (w + 1) & (size - 1);
        }
    }
private:
    float read (double delay) const
    {
        double rp = w - delay; while (rp < 0) rp += size;
        const int i0 = (int) rp; const float f = (float) (rp - i0);
        return buf[(size_t) (i0 & (size - 1))] * (1.0f - f) + buf[(size_t) ((i0 + 1) & (size - 1))] * f;
    }
    double sr = 44100.0, ph1 = 0, ph2 = 0;
    std::vector<float> buf; int size = 4096, w = 0;
};

// ---------------------------------------------------------------------------
// Delay estereo sincronizado al tempo, con ping-pong y filtros en la realimentacion
// ---------------------------------------------------------------------------
class TempoDelay
{
public:
    void prepare (double s)
    {
        sr = s;
        size = juce::nextPowerOfTwo ((int) (4.5 * sr));
        bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f);
        w = 0; curDelay = 0.25 * sr; lpL = lpR = hpL = hpR = 0;
    }

    void process (const float* in, float* outL, float* outR, int n, double delaySec, float fbPct, bool pingPong)
    {
        const double target = juce::jlimit (0.01, 4.0, delaySec) * sr;
        const float fb = fbPct * 0.01f;
        const float lpC = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 5500.0 / sr);
        const float hpC = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 220.0 / sr);
        const double smooth = std::exp (-1.0 / (0.08 * sr));

        for (int i = 0; i < n; ++i)
        {
            curDelay = target + (curDelay - target) * smooth;
            modPh += 0.35 / sr; if (modPh >= 1.0) modPh -= 1.0;
            const double wow = 0.0006 * sr * std::sin (2.0 * juce::MathConstants<double>::pi * modPh);   // "wow" de cinta
            float dl = read (bl, curDelay + wow), dr = read (br, curDelay - wow);

            // filtros (LP + HP de un polo) en la realimentacion
            lpL = dl + (lpL - dl) * lpC; lpR = dr + (lpR - dr) * lpC;
            hpL = lpL + (hpL - lpL) * hpC; hpR = lpR + (hpR - lpR) * hpC;
            const float fl = std::tanh ((lpL - hpL) * 1.2f) / 1.2f, fr = std::tanh ((lpR - hpR) * 1.2f) / 1.2f;   // saturacion de cinta en las repeticiones

            if (pingPong)
            {
                bl[(size_t) w] = in[i] + fr * fb;
                br[(size_t) w] = fl * fb;
            }
            else
            {
                bl[(size_t) w] = in[i] + fl * fb;
                br[(size_t) w] = in[i] + fr * fb;
            }
            outL[i] = fl; outR[i] = fr;
            w = (w + 1) & (size - 1);
        }
    }
private:
    float read (const std::vector<float>& b, double delay) const
    {
        double rp = w - delay; while (rp < 0) rp += size;
        const int i0 = (int) rp; const float f = (float) (rp - i0);
        return b[(size_t) (i0 & (size - 1))] * (1.0f - f) + b[(size_t) ((i0 + 1) & (size - 1))] * f;
    }
    double sr = 44100.0, curDelay = 10000, modPh = 0.0;
    std::vector<float> bl, br; int size = 1 << 18, w = 0;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0;
};

// ---------------------------------------------------------------------------
// ArtisVerb: reverb algoritmica profesional
//  - Pre-delay + filtro de envio (HPF 220 Hz como en un retorno de mezcla)
//  - Reflexiones tempranas (Sala / Cuarto)
//  - Difusor de 4 etapas x 8 canales (matriz de Hadamard)
//  - Red de retardo realimentada (FDN) de 8 lineas con matriz de Householder,
//    tiempo de caida RT60 real en segundos, amortiguacion de agudos y
//    modulacion lenta de los retardos para evitar el sonido metalico.
//  Tipos: 0 Nube (enorme y oscura), 1 Plate (denso, brillante, sin reflexiones),
//         2 Sala (hall), 3 Cuarto (room)
// ---------------------------------------------------------------------------
class ProReverb
{
public:
    static constexpr int N = 8;

    void prepare (double s, int)
    {
        sr = s;
        pre.assign ((size_t) juce::nextPowerOfTwo ((int) (0.3 * sr) + 4), 0.0f);
        preMask = (int) pre.size() - 1; pw = 0;
        for (auto& f : { &hp1, &hp2 }) { f->coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 220.0f, 0.707f); f->reset(); }
        lpIn.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jmin (11000.0f, (float) sr * 0.45f), 0.707f); lpIn.reset();

        const int maxLen = juce::nextPowerOfTwo ((int) (0.45 * sr) + 64);
        for (int c = 0; c < N; ++c)
        {
            fdn[c].assign ((size_t) maxLen, 0.0f);
            for (int st = 0; st < 4; ++st) diff[st][c].assign ((size_t) juce::nextPowerOfTwo ((int) (0.08 * sr) + 8), 0.0f);
            lpState[c] = 0.0f; lfoPh[c] = c * 0.137;
        }
        fdnMask = maxLen - 1; fw = 0; dw = 0;
        erBuf.assign ((size_t) juce::nextPowerOfTwo ((int) (0.12 * sr) + 8), 0.0f); erMask = (int) erBuf.size() - 1; ew = 0;
        type = -1; lastSize = -1; lastDecay = -1; lastDamp = -1;
        setParams (1, 50.0f, 1.6f, 40.0f);
    }

    void setParams (int t, float sizePct, float decaySec, float dampPct)
    {
        if (t == type && std::abs (sizePct - lastSize) < 0.01f && std::abs (decaySec - lastDecay) < 0.001f && std::abs (dampPct - lastDamp) < 0.01f) return;
        type = juce::jlimit (0, 3, t); lastSize = sizePct; lastDecay = decaySec; lastDamp = dampPct;

        static const float base[4][N] = {
            { 71.3f, 83.1f, 97.7f, 113.9f, 127.1f, 149.3f, 167.9f, 191.7f },   // Nube
            { 29.7f, 37.1f, 41.1f, 43.7f, 49.9f, 53.1f, 59.3f, 67.3f },        // Plate
            { 48.1f, 57.7f, 67.3f, 79.1f, 91.3f, 103.7f, 119.3f, 137.1f },     // Sala
            { 17.3f, 21.1f, 24.7f, 27.9f, 31.3f, 34.9f, 38.3f, 43.1f } };      // Cuarto
        const float scale = 0.6f + sizePct * 0.01f * 0.9f;
        const float rt = juce::jlimit (0.2f, 12.0f, decaySec) * 1.17f;   // compensa la perdida extra de la amortiguacion
        for (int c = 0; c < N; ++c)
        {
            delaySamp[c] = juce::jlimit (32.0, (double) fdnMask - 200.0, base[type][c] * 0.001 * scale * sr);
            gain[c] = (float) std::pow (10.0, -3.0 * delaySamp[c] / (rt * sr));
        }
        // Amortiguacion: Plate brillante, Nube oscura
        static const float dampBase[4] = { 5500.0f, 9500.0f, 7000.0f, 6500.0f };
        const float fc = dampBase[type] * std::pow (0.25f, dampPct * 0.01f) * 2.0f;
        lpCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jlimit (1500.0f, 16000.0f, fc) / sr);

        static const float diffMs[4] = { 60.0f, 45.0f, 40.0f, 18.0f };
        juce::Random rnd (1234 + type);
        for (int st = 0; st < 4; ++st)
        {
            const float range = diffMs[type] * scale * std::pow (0.6f, (float) (3 - st)) * 0.001f * (float) sr;
            for (int c = 0; c < N; ++c)
            {
                const float lo = range * c / N, hi = range * (c + 1) / N;
                diffDelay[st][c] = juce::jlimit (1, (int) diff[st][c].size() - 2, (int) (lo + rnd.nextFloat() * (hi - lo)) + 1);
                diffFlip[st][c] = rnd.nextBool() ? -1.0f : 1.0f;
            }
        }
        modDepth = (type == 0 ? 2.2 : type == 1 ? 1.0 : 0.6) * 0.001 * sr;
        erOn = type >= 2;
    }

    void process (const float* in, float* outL, float* outR, int n, float preDelayMs)
    {
        const int pd = juce::jlimit (0, preMask - 1, (int) (preDelayMs * 0.001 * sr));
        static const float erT[12] = { 0.0043f, 0.0071f, 0.0109f, 0.0151f, 0.0197f, 0.0233f, 0.0281f, 0.0337f, 0.0401f, 0.0463f, 0.0547f, 0.0631f };
        const float erScale = type == 3 ? 0.6f : 1.0f;
        const double lfoInc = 1.0 / sr;

        for (int i = 0; i < n; ++i)
        {
            float x = lpIn.processSample (hp2.processSample (hp1.processSample (in[i])));
            pre[(size_t) pw] = x;
            x = pre[(size_t) ((pw - pd) & preMask)];
            pw = (pw + 1) & preMask;

            // Reflexiones tempranas
            float erL = 0.0f, erR = 0.0f;
            if (erOn)
            {
                erBuf[(size_t) ew] = x;
                for (int k = 0; k < 12; ++k)
                {
                    const int d = (int) (erT[k] * erScale * sr);
                    const float v = erBuf[(size_t) ((ew - d) & erMask)] * (0.75f - 0.05f * k);
                    if (k & 1) erR += v; else erL += v;
                }
                ew = (ew + 1) & erMask;
            }

            // Difusor: 4 etapas (retardo + Hadamard)
            float ch[N];
            for (int c = 0; c < N; ++c) ch[c] = x * ((c & 1) ? -1.0f : 1.0f);
            for (int st = 0; st < 4; ++st)
            {
                for (int c = 0; c < N; ++c)
                {
                    auto& b = diff[st][c];
                    const int msk = (int) b.size() - 1;
                    b[(size_t) (dw & msk)] = ch[c];
                    ch[c] = b[(size_t) ((dw - diffDelay[st][c]) & msk)] * diffFlip[st][c];
                }
                hadamard (ch);
            }
            dw = (dw + 1) & 0x7fffffff;

            // FDN con modulacion
            float y[N];
            for (int c = 0; c < N; ++c)
            {
                lfoPh[c] += lfoInc * (0.13 + 0.07 * c);
                if (lfoPh[c] >= 1.0) lfoPh[c] -= 1.0;
                const double d = delaySamp[c] + modDepth * (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * lfoPh[c]));
                double rp = (double) fw - d;
                const int i0 = (int) std::floor (rp);
                const float fr = (float) (rp - i0);
                const auto& b = fdn[c];
                const float v = b[(size_t) (i0 & fdnMask)] * (1.0f - fr) + b[(size_t) ((i0 + 1) & fdnMask)] * fr;
                lpState[c] = v + (lpState[c] - v) * lpCoeff;   // amortiguacion de agudos
                y[c] = lpState[c] * gain[c];
            }
            // Householder: y - 2/N * suma
            float sum = 0.0f; for (int c = 0; c < N; ++c) sum += y[c];
            sum *= 2.0f / N;
            for (int c = 0; c < N; ++c) fdn[c][(size_t) fw] = y[c] - sum + ch[c] * 0.5f;
            fw = (fw + 1) & fdnMask;

            const float l = (y[0] - y[2] + y[4] - y[6] + y[1] * 0.5f - y[7] * 0.5f);
            const float r = (y[1] - y[3] + y[5] - y[7] + y[2] * 0.5f - y[4] * 0.5f);
            outL[i] = (l * 0.42f + erL * 0.18f);
            outR[i] = (r * 0.42f + erR * 0.18f);
        }
    }

private:
    static void hadamard (float* v)
    {
        for (int h = 1; h < N; h *= 2)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b; v[j + h] = a - b;
                }
        const float s = 1.0f / std::sqrt ((float) N);
        for (int i = 0; i < N; ++i) v[i] *= s;
    }

    double sr = 44100.0;
    std::vector<float> pre; int preMask = 0, pw = 0;
    juce::dsp::IIR::Filter<float> hp1, hp2, lpIn;
    std::vector<float> fdn[N]; int fdnMask = 0, fw = 0;
    std::vector<float> diff[4][N]; int diffDelay[4][N] {}; float diffFlip[4][N] {}; int dw = 0;
    std::vector<float> erBuf; int erMask = 0, ew = 0; bool erOn = false;
    double delaySamp[N] {}; float gain[N] {}; float lpState[N] {}; double lfoPh[N] {};
    float lpCoeff = 0.5f; double modDepth = 20.0;
    int type = -1; float lastSize = -1, lastDecay = -1, lastDamp = -1;
};

} // namespace avdsp
