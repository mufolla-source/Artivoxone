#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <atomic>
#include <cstdint>

// ============================================================================
//  ArtisTune - corrector de afinacion en tiempo real (TD-PSOLA)
//  1. Deteccion de tono YIN (umbral estricto, interpolacion parabolica,
//     mediana en modo Studio) para elegir la nota objetivo.
//  2. Marcas de periodo alineadas con los picos de cada ciclo de la voz:
//     el tono se mide ciclo a ciclo (precision de muestra).
//  3. Resintesis PSOLA: cada ciclo se recorta con una ventana de Hann de dos
//     periodos y se vuelve a colocar a la distancia del periodo CORREGIDO.
//     Asi cambia el tono pero la envolvente espectral (formantes / timbre)
//     se conserva: sin efecto "ardilla" ni voz de robot no deseada.
//  4. Velocidad (retune) y Humanizar suavizan la correccion ciclo a ciclo.
// ============================================================================
class PitchCorrector
{
public:
    void prepare (double sampleRate, bool studioMode)
    {
        sr = sampleRate;
        studio = studioMode;
        minF0 = studio ? 70.0 : 90.0;
        decim = sr > 60000.0 ? 2 : 1;
        srA = sr / decim;

        // --- Detector
        anaSize = 2048;
        integ = anaSize / 2;
        tauMin = juce::jmax (2, (int) std::floor (srA / maxF0));
        tauMax = juce::jmin (integ - 2, (int) std::ceil (srA / minF0));
        anaBuf.assign ((size_t) anaSize * 2, 0.0f);
        diff.assign ((size_t) integ + 2, 0.0f);
        anaWrite = 0; anaFilled = 0; hopCount = 0; decimAcc = 0.0f; decimCount = 0;
        hop = 256;

        // --- PSOLA
        pMax = (int) std::ceil (sr / minF0);
        latency = (int) std::ceil (2.5 * pMax) + 32;
        unvHop = (int) std::round (0.005 * sr);
        size = juce::nextPowerOfTwo (latency * 2 + pMax * 4 + 64);
        mask = size - 1;
        inBuf.assign ((size_t) size, 0.0f);
        lpBuf.assign ((size_t) size, 0.0f);
        outAcc.assign ((size_t) size, 0.0f);
        n = 0; lp = 0.0f;
        marks.assign (512, Mark {});
        markHead = 0; markCount = 0; lastMark = 0; nextSynth = latency; markIdx = 0; started = false;
        curSemis = 0.0; targetNote = -1; noteHeldSamples = 0; heldMarkNote = -1;
        history[0] = history[1] = history[2] = 0.0f;
        detectedHz.store (0.0f); correctionCents.store (0.0f); currentNoteIdx.store (-1);
        lastPeriod = sr / 200.0; lastF0 = 0.0f; lastConf = 0.0f; voiced = false;
    }

    int getLatencySamples() const { return latency; }

    void setParams (bool on, int key, int scale, float speedMs, float amountPct, float humanPct)
    {
        enabled = on;
        keyRoot = key; scaleIdx = scale;
        speed = speedMs; amount = amountPct * 0.01f; human = humanPct * 0.01f;
    }

    void process (float* x, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            const float in = x[i];
            pushAnalysis (in);
            inBuf[(size_t) (n & mask)] = in;
            lp += 0.25f * (in - lp);                 // copia suavizada para buscar picos
            lpBuf[(size_t) (n & mask)] = lp;

            placeMarks();

            // Sintetizar los granos cuya zona empieza a leerse
            while (nextSynth - pMax <= (double) n)
                synthGrain();

            const size_t idx = (size_t) (n & mask);
            x[i] = outAcc[idx];
            outAcc[idx] = 0.0f;
            ++n;
        }
    }

    std::atomic<float> detectedHz { 0.0f };
    std::atomic<float> correctionCents { 0.0f };
    std::atomic<int>   currentNoteIdx { -1 };

    float getLastF0() const      { return lastF0; }
    float getLastConfidence() const { return lastConf; }
    bool  isVoiced() const       { return voiced; }
    int   detectionCounter() const { return detections; }

    static bool noteInScale (int pc, int key, int scale)
    {
        static const int masks[6][12] = {
            { 1,0,1,0,1,1,0,1,0,1,0,1 }, // Mayor
            { 1,0,1,1,0,1,0,1,1,0,1,0 }, // Menor natural
            { 1,1,1,1,1,1,1,1,1,1,1,1 }, // Cromatica
            { 1,0,1,1,0,1,0,1,1,0,0,1 }, // Menor armonica
            { 1,0,0,1,0,1,0,1,0,0,1,0 }, // Pentatonica menor
            { 1,0,1,0,1,0,0,1,0,1,0,0 }, // Pentatonica mayor
        };
        const int rel = ((pc - key) % 12 + 12) % 12;
        return masks[juce::jlimit (0, 5, scale)][rel] != 0;
    }

private:
    static constexpr double maxF0 = 1000.0;
    double minF0 = 70.0;

    double sr = 44100.0, srA = 44100.0;
    int decim = 1;
    bool studio = true, enabled = true, voiced = false;

    // Analisis YIN
    std::vector<float> anaBuf, diff;
    int anaSize = 2048, integ = 1024, tauMin = 40, tauMax = 600;
    int anaWrite = 0, anaFilled = 0, hop = 256, hopCount = 0;
    float decimAcc = 0.0f; int decimCount = 0;
    float history[3] {};
    int detections = 0;
    float lastF0 = 0.0f, lastConf = 0.0f;
    double lastPeriod = 200.0;

    // Parametros
    int keyRoot = 0, scaleIdx = 1;
    float speed = 20.0f, amount = 1.0f, human = 0.2f;
    int targetNote = -1;

    // PSOLA
    struct Mark { int64_t pos = 0; float period = 240.0f; bool voiced = false; int note = -1; };
    std::vector<float> inBuf, lpBuf, outAcc;
    std::vector<Mark> marks;
    int size = 8192, mask = 8191, pMax = 640, latency = 1600, unvHop = 240;
    int64_t n = 0, lastMark = 0;
    int markHead = 0, markCount = 0, markIdx = 0;
    double nextSynth = 0.0;
    double curSemis = 0.0;
    int noteHeldSamples = 0, heldMarkNote = -1;
    double centerMidi = 60.0;
    bool started = false;
    int vCount = 0, uCount = 0;
    float lp = 0.0f;

    const Mark& markAt (int k) const { return marks[(size_t) (k & 511)]; }

    void addMark (const Mark& m)
    {
        marks[(size_t) (markHead & 511)] = m;
        ++markHead;
        markCount = juce::jmin (markCount + 1, 512);
        lastMark = m.pos;
    }

    // Marcas de periodo. Cada marca nueva se alinea con la anterior por correlacion
    // (misma fase del ciclo), asi los granos encajan sin cancelaciones ni zumbido.
    void placeMarks()
    {
        if (voiced && enabled)
        {
            const double P = juce::jlimit (sr / maxF0, (double) pMax, lastPeriod);
            const int half = (int) std::round (P * 0.5);
            const int search = (int) std::round (P * 0.25);
            const int64_t expected = lastMark + (int64_t) std::round (P);
            if (n < expected + search) return;          // la correlacion usa el ciclo ANTERIOR a la marca: sin esperar mas

            int64_t best = expected;
            const bool prevVoiced = markCount > 0 && markAt (markHead - 1).voiced;
            if (prevVoiced)
            {
                // Correlacion normalizada entre el ciclo anterior y el candidato
                double bestScore = -2.0;
                double e0 = 0.0;
                for (int j = -2 * half; j < 0; ++j) { const float a = inBuf[(size_t) ((lastMark + j) & mask)]; e0 += (double) a * a; }
                for (int64_t c = expected - search; c <= expected + search; ++c)
                {
                    double num = 0.0, e1 = 0.0;
                    for (int j = -2 * half; j < 0; ++j)
                    {
                        const float a = inBuf[(size_t) ((lastMark + j) & mask)];
                        const float b = inBuf[(size_t) ((c + j) & mask)];
                        num += (double) a * b; e1 += (double) b * b;
                    }
                    const double sc = num / std::sqrt (e0 * e1 + 1e-20);
                    if (sc > bestScore) { bestScore = sc; best = c; }
                }
            }
            else
            {
                // Primera marca de una zona con voz: el pico mas alto del ultimo periodo
                float bestV = -1e9f;
                for (int64_t k = n - (int64_t) P; k <= n; ++k)
                {
                    const float v = lpBuf[(size_t) (k & mask)];
                    if (v > bestV && k > lastMark) { bestV = v; best = k; }
                }
            }
            Mark m;
            m.pos = best;
            m.period = (float) P;
            m.voiced = true;
            m.note = targetNote;
            addMark (m);
        }
        else
        {
            if (n < lastMark + unvHop) return;
            Mark m; m.pos = lastMark + unvHop; m.period = (float) unvHop; m.voiced = false; m.note = -1;
            addMark (m);
        }
    }

    void synthGrain()
    {
        const int64_t ts = (int64_t) std::llround (nextSynth);
        const int64_t ti = ts - latency;

        // Eleccion de la marca de analisis: secuencial (la siguiente a la anterior) para que,
        // sin correccion, la salida sea identica a la entrada; si el tiempo se desvia mas de
        // medio periodo (al subir o bajar el tono), se salta o se repite un ciclo.
        const int oldest = markHead - markCount;
        if (markIdx < oldest || ! started)
        {
            markIdx = juce::jmax (markIdx, oldest);
            while (markIdx + 1 < markHead && std::abs (markAt (markIdx + 1).pos - ti) <= std::abs (markAt (markIdx).pos - ti))
                ++markIdx;
            started = true;
        }
        else
        {
            int cand = juce::jmin (markIdx + 1, markHead - 1);
            auto halfP = [this] (int idx)
            {
                const double d = idx + 1 < markHead ? (double) (markAt (idx + 1).pos - markAt (idx).pos) : (double) markAt (idx).period;
                return 0.5 * juce::jlimit (16.0, 2.0 * pMax, d);
            };
            // salir de la zona de las marcas mas viejas si el tiempo de entrada va por delante
            while (cand + 1 < markHead && (double) (ti - markAt (cand).pos) > halfP (cand)) ++cand;
            // repetir el ciclo si el tiempo de entrada va por detras
            if ((double) (markAt (cand).pos - ti) > halfP (cand) && cand > markIdx && cand - 1 >= oldest) --cand;
            markIdx = cand;
        }

        Mark m;
        double Pl = unvHop, Pr = unvHop;      // mitades izquierda y derecha del grano (ventana asimetrica)
        if (markCount == 0) { m.pos = ti; m.period = (float) unvHop; m.voiced = false; }
        else
        {
            m = markAt (markIdx);
            Pl = Pr = m.period;
            // Distancias reales a las marcas vecinas: los granos se solapan exactos (reconstruccion perfecta)
            if (markIdx + 1 < markHead)
                Pr = juce::jlimit (16.0, 2.0 * pMax, (double) (markAt (markIdx + 1).pos - m.pos));
            if (markIdx - 1 >= oldest)
                Pl = juce::jlimit (16.0, 2.0 * pMax, (double) (m.pos - markAt (markIdx - 1).pos));
            m.period = (float) Pr;
        }
        const int P = juce::jlimit (16, pMax, (int) std::round (m.period));
        double ratio = 1.0;

        if (m.voiced && enabled && m.note >= 0)
        {
            const double midi = 69.0 + 12.0 * std::log2 ((sr / m.period) / 440.0);
            const double dt = P / sr;

            // Centro de la nota: sigue el tono lento (deja pasar el vibrato y la expresion).
            // Con Velocidad 0 el centro es el tono instantaneo = efecto robot clasico.
            if (m.note != heldMarkNote) { heldMarkNote = m.note; noteHeldSamples = 0; centerMidi = midi; }
            noteHeldSamples += P;
            const double centerTau = speed < 1.0f ? 0.0 : juce::jlimit (0.015, 0.35, speed * 0.004);
            centerMidi = centerTau <= 0.0 ? midi : midi + (centerMidi - midi) * std::exp (-dt / centerTau);

            const double desired = juce::jlimit (-12.0, 12.0, (m.note - centerMidi) * amount);

            double tauMs = speed;
            const double heldMs = 1000.0 * noteHeldSamples / sr;
            if (heldMs > 150.0) tauMs *= 1.0 + human * 6.0 * juce::jmin (1.0, (heldMs - 150.0) / 250.0);

            if (tauMs < 0.5) curSemis = desired;
            else curSemis = desired + (curSemis - desired) * std::exp (-dt / (tauMs * 0.001));
        }
        else
        {
            const double dt = P / sr;
            curSemis *= std::exp (-dt / 0.03);
            heldMarkNote = -1;
        }

        if (m.voiced && enabled) ratio = std::pow (2.0, curSemis / 12.0);

        const double Ps = m.voiced ? Pr / ratio : Pr;
        const float scale = (float) (1.0 / (m.voiced ? ratio : 1.0));

        // Superponer el grano: ventana de Hann asimetrica (Pl a la izquierda, Pr a la derecha)
        const int L = juce::jlimit (8, pMax * 2, (int) std::round (Pl));
        const int R = juce::jlimit (8, pMax * 2, (int) std::round (Pr));
        for (int k = -L; k < R; ++k)
        {
            const double ph = k < 0 ? (double) k / L : (double) k / R;
            const float win = (float) (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * ph));
            outAcc[(size_t) ((ts + k) & mask)] += inBuf[(size_t) ((m.pos + k) & mask)] * win * scale;
        }

        nextSynth += Ps;
    }

    void pushAnalysis (float s)
    {
        decimAcc += s;
        if (++decimCount < decim) return;
        const float v = decimAcc / (float) decim;
        decimAcc = 0.0f; decimCount = 0;

        anaBuf[(size_t) anaWrite] = v;
        anaBuf[(size_t) (anaWrite + anaSize)] = v; // copia espejo para lectura lineal
        anaWrite = (anaWrite + 1) % anaSize;
        if (anaFilled < anaSize) ++anaFilled;

        if (++hopCount >= hop / decim)
        {
            hopCount = 0;
            if (anaFilled >= anaSize) detect();
        }
    }

    void detect()
    {
        const float* x = anaBuf.data() + anaWrite; // ventana lineal de anaSize muestras

        // Energia: si es muy baja, sin voz
        double energy = 0.0;
        for (int j = 0; j < integ; ++j) energy += (double) x[j + integ] * x[j + integ];
        const double rmsDb = 10.0 * std::log10 (energy / integ + 1e-12);
        if (rmsDb < -50.0) { setUnvoiced(); return; }

        // YIN: funcion diferencia
        diff[0] = 1.0f;
        double running = 0.0;
        int best = -1;
        const float thr = studio ? 0.12f : 0.18f;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float* xt = x + tau;
            float acc[8] = {};
            for (int j = 0; j < integ; j += 8)          // integ es multiplo de 8
                for (int u = 0; u < 8; ++u)
                {
                    const float dd = x[j + u] - xt[j + u];
                    acc[u] += dd * dd;
                }
            const float s = ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
            running += s;
            diff[(size_t) tau] = running > 0.0 ? (float) (s * tau / running) : 1.0f;
        }

        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < thr)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }

        if (best < 0) { setUnvoiced(); return; }

        // Interpolacion parabolica
        double betterTau = best;
        if (best > 1 && best < tauMax - 1)
        {
            const double s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const double den = 2.0 * (2.0 * s1 - s2 - s0);
            if (std::abs (den) > 1e-9) betterTau = best + (s2 - s0) / den;
        }

        float f0 = (float) (srA / betterTau);
        const float conf = 1.0f - diff[(size_t) best];

        if (studio)
        {
            // Mediana de 3 para evitar saltos de octava puntuales
            history[0] = history[1]; history[1] = history[2]; history[2] = f0;
            float a = history[0], b = history[1], c = history[2];
            if (a > 0 && b > 0 && c > 0)
                f0 = std::max (std::min (a, b), std::min (std::max (a, b), c));
        }

        uCount = 0;
        if (++vCount >= 2) voiced = true;      // 2 detecciones seguidas para entrar en voz
        lastF0 = f0; lastConf = conf;
        lastPeriod = sr / f0;
        ++detections;
        detectedHz.store (f0);

        // --- Nota objetivo
        const double midi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
        const int nearest = (int) std::round (midi);

        if (targetNote >= 0 && std::abs (midi - targetNote) < 0.65 && noteInScale (targetNote % 12, keyRoot, scaleIdx))
        {
            // mantener nota actual (histeresis)
        }
        else
        {
            int bestN = nearest; double bestDist = 99.0;
            for (int cand = nearest - 2; cand <= nearest + 2; ++cand)
            {
                if (cand < 0) continue;
                if (! noteInScale (cand % 12, keyRoot, scaleIdx)) continue;
                const double dist = std::abs (midi - cand);
                if (dist < bestDist) { bestDist = dist; bestN = cand; }
            }
            if (bestN != targetNote) noteHeldSamples = 0;
            targetNote = bestN;
        }

        currentNoteIdx.store (targetNote % 12);
        correctionCents.store ((float) ((targetNote - midi) * amount * 100.0));
    }

    void setUnvoiced()
    {
        vCount = 0;
        if (++uCount < 3) return;              // 3 seguidas para salir (evita cortes en consonantes)
        voiced = false;
        history[0] = history[1] = history[2] = 0.0f;
        lastF0 = 0.0f; lastConf = 0.0f;
        detectedHz.store (0.0f);
        currentNoteIdx.store (-1);
        correctionCents.store (0.0f);
        targetNote = -1;
    }

};
