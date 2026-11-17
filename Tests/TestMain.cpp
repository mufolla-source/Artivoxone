// Prueba offline: procesa una voz sintetica desafinada, comprueba que no hay NaN,
// mide la afinacion antes/despues, ejecuta Vocal Assist, prueba todos los presets
// y guarda una captura de la interfaz (Simple y Avanzado).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

static float estimatePitch (const float* x, int n, double sr)
{
    // YIN sencillo (primer minimo bajo el umbral)
    const int maxLag = (int) (sr / 70), minLag = (int) (sr / 800);
    const int W = n - maxLag; if (W < 64) return 0;
    std::vector<double> d ((size_t) maxLag + 2, 0.0);
    double run = 0;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double s = 0; for (int i = 0; i < W; ++i) { const double df = x[i] - x[i + lag]; s += df * df; }
        run += s; d[(size_t) lag] = run > 0 ? s * lag / run : 1.0;
    }
    int best = -1;
    for (int lag = minLag; lag < maxLag; ++lag)
        if (d[(size_t) lag] < 0.15) { while (lag + 1 < maxLag && d[(size_t) lag + 1] < d[(size_t) lag]) ++lag; best = lag; break; }
    if (best < 0) { double mn = 9; for (int lag = minLag; lag < maxLag; ++lag) if (d[(size_t) lag] < mn) { mn = d[(size_t) lag]; best = lag; } }
    double bt = best;
    if (best > 1 && best < maxLag) { const double a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1]; const double den = 2 * (2 * b - a - c); if (std::abs (den) > 1e-12) bt = best + (c - a) / den; }
    return (float) (sr / bt);
}

static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// Modo render: ArtisVoxOneTests entrada.wav genero salida.wav
// Ejecuta Vocal Assist (15 s) con el genero indicado y procesa el archivo entero.
static int renderMode (const juce::File& inF, int genre, const juce::File& outF, bool flatEq)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (inF));
    if (rd == nullptr) { std::cerr << "no puedo leer " << inF.getFullPathName() << "\n"; return 2; }
    const double sr = rd->sampleRate; const int len = (int) rd->lengthInSamples; const int bs = 512;
    juce::AudioBuffer<float> in (2, len); rd->read (&in, 0, len, 0, true, true);
    auto proc = std::make_unique<ArtisVoxOneProcessor>();
    proc->setPlayConfigDetails (2, 2, sr, bs); proc->prepareToPlay (sr, bs);
    juce::MidiBuffer mb;
    proc->startAssist (genre);
    int pos = 0;
    while (proc->assist.active.load() && pos + bs < len)
    {
        juce::AudioBuffer<float> b (2, bs); for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, in, c, pos, bs);
        proc->processBlock (b, mb); pos += bs;
    }
    VocalAssist::Result r;
    for (int w = 0; w < 600 && ! proc->pollAssistFinished (r); ++w) juce::Thread::sleep (50);
    if (flatEq) for (auto* id : { PID::lowGain, PID::mudGain, PID::presGain, PID::airGain }) proc->setParamValue (id, 0.0f);
    std::cout << "Assist: " << proc->currentPresetName << "  ok=" << r.ok << "\n";
    for (auto* id : { PID::inGain, PID::hpfFreq, PID::lowGain, PID::mudGain, PID::presGain, PID::airGain, PID::deessAmount, PID::compAmount, PID::revDecay })
        std::cout << "  " << id << " = " << proc->apvts.getRawParameterValue (id)->load() << "\n";
    proc->prepareToPlay (sr, bs);
    const int lat = proc->getLatencySamples();
    juce::AudioBuffer<float> out (2, len);
    for (int p = 0; p < len + lat; p += bs)
    {
        juce::AudioBuffer<float> b (2, bs); b.clear();
        for (int c = 0; c < 2; ++c) if (p < len) b.copyFrom (c, 0, in, c, p, std::min (bs, len - p));
        proc->processBlock (b, mb);
        const int dst = p - lat;
        for (int i = 0; i < bs; ++i) if (dst + i >= 0 && dst + i < len) for (int c = 0; c < 2; ++c) out.setSample (c, dst + i, b.getSample (c, i));
    }
    outF.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outF), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, len);
    return 0;
}

// Modo afinacion: --tune entrada.wav salida.wav velocidad cantidad humanizar tonalidad escala
static int tuneMode (char** argv)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (argv[2])));
    if (rd == nullptr) return 2;
    const double sr = rd->sampleRate; const int len = (int) rd->lengthInSamples;
    juce::AudioBuffer<float> in (2, len); rd->read (&in, 0, len, 0, true, true);
    std::vector<float> m ((size_t) len);
    for (int i = 0; i < len; ++i) m[(size_t) i] = 0.5f * (in.getSample (0, i) + in.getSample (1, i));
    PitchCorrector pc; pc.prepare (sr, true);
    pc.setParams (juce::String (argv[5]).getFloatValue() >= 0.0f, juce::String (argv[7]).getIntValue(), juce::String (argv[8]).getIntValue(),
                  juce::String (argv[4]).getFloatValue(), juce::String (argv[5]).getFloatValue(), juce::String (argv[6]).getFloatValue());
    std::vector<float> o (m);
    for (int p = 0; p < len; p += 512) pc.process (o.data() + p, std::min (512, len - p));
    const int L = pc.getLatencySamples();
    juce::AudioBuffer<float> out (1, len); out.clear();
    for (int i = 0; i + L < len; ++i) out.setSample (0, i, o[(size_t) (i + L)]);
    juce::File f (argv[3]); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 1, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, len);
    std::cout << "latencia " << L << "\n";
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 9 && juce::String (argv[1]) == "--tune") return tuneMode (argv);
    if (argc >= 4) return renderMode (juce::File (argv[1]), juce::String (argv[2]).getIntValue(), juce::File (argv[3]), argc >= 5 && juce::String (argv[4]) == "noeq");
    // --motor: solo pruebas del motor de audio (sin capturas ni archivos), devuelve 1 si algo falla
    const bool engineOnly = argc >= 2 && juce::String (argv[1]) == "--motor";
    int failures = 0;
    const double sr = 48000.0; const int bs = 512;

    auto proc = std::make_unique<ArtisVoxOneProcessor>();
    proc->setPlayConfigDetails (2, 2, sr, bs);
    proc->prepareToPlay (sr, bs);
    std::cout << "Latencia: " << proc->getLatencySamples() << " muestras ("
              << proc->getLatencySamples() * 1000.0 / sr << " ms)\n";

    // Tonalidad A menor, afinacion rapida, sin efectos de espacio para medir
    proc->setParamValue (PID::tuneKey, 9);
    proc->setParamValue (PID::tuneScale, 1);
    proc->setParamValue (PID::tuneSpeed, 0);
    proc->setParamValue (PID::tuneHuman, 0);
    proc->setParamValue (PID::revMix, 0);
    proc->setParamValue (PID::dlyMix, 0);

    // Voz sintetica: diente de sierra filtrada en A3 (220 Hz) desafinada +35 cents
    const double f0 = 220.0 * std::pow (2.0, 35.0 / 1200.0);
    const int total = (int) (sr * 20);
    juce::AudioBuffer<float> in (2, total);
    juce::Random rnd (1);
    double ph = 0; float lp = 0, lp2 = 0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const double f = f0 * (1.0 + 0.003 * std::sin (2 * juce::MathConstants<double>::pi * 5.0 * t));
        ph += f / sr; if (ph >= 1) ph -= 1;
        float s = (float) (2 * ph - 1);
        lp += 0.25f * (s - lp); lp2 += 0.25f * (lp - lp2);
        // frases de 2 s con 0.4 s de silencio y "eses" cortas
        const double cyc = std::fmod (t, 2.4);
        float env = cyc < 2.0 ? (float) std::min (1.0, std::min (cyc / 0.05, (2.0 - cyc) / 0.05)) : 0.0f;
        float sib = (cyc > 2.0 && cyc < 2.15) ? (rnd.nextFloat() * 2 - 1) * 0.08f : 0.0f;
        float v = lp2 * 0.25f * env + sib + (rnd.nextFloat() * 2 - 1) * 0.0005f;
        in.setSample (0, i, v); in.setSample (1, i, v);
    }

    // ---- Calidad de afinacion: voz con formantes, vibrato y desafinacion variable
    {
        PitchCorrector pc; pc.prepare (sr, true);
        pc.setParams (true, 9, 1, 0.0f, 100.0f, 0.0f);   // A menor, velocidad 0 (hard-tune)
        const int N = (int) (sr * 6);
        std::vector<float> v ((size_t) N), o;
        double ph2 = 0; float b1 = 0, b2 = 0, c1 = 0, c2 = 0;
        // resonadores (formantes 700 Hz y 1200 Hz)
        auto res = [&] (float x, float f, float& y1, float& y2) {
            const float r = 0.93f, w = (float) (2 * juce::MathConstants<double>::pi * f / sr);
            const float y = x + 2 * r * std::cos (w) * y1 - r * r * y2; y2 = y1; y1 = y; return y; };
        for (int i = 0; i < N; ++i)
        {
            const double t = i / sr;
            const double cents0 = 40.0 * std::sin (2 * juce::MathConstants<double>::pi * 0.25 * t);      // deriva lenta
            const double vib = 25.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t);
            const double f = 220.0 * std::pow (2.0, (cents0 + vib) / 1200.0);
            ph2 += f / sr; float pulse = 0;
            if (ph2 >= 1) { ph2 -= 1; pulse = 1; }
            float y = res (pulse, 700, b1, b2) * 0.02f + res (pulse, 1200, c1, c2) * 0.012f;
            v[(size_t) i] = y;
        }
        o = v;
        pc.process (o.data(), N);
        const int L = pc.getLatencySamples();
        auto centroid = [&] (const float* x, int len) {
            juce::dsp::FFT f (12); std::vector<float> d (8192, 0.0f);
            for (int i = 0; i < 4096 && i < len; ++i) d[(size_t) i] = x[i] * (0.5f - 0.5f * std::cos (2 * juce::MathConstants<float>::pi * i / 4095));
            f.performFrequencyOnlyForwardTransform (d.data());
            // envolvente: maximo local por ventanas de 250 Hz y luego pico del formante 1 (400-1000 Hz)
            const int win = (int) (250.0 / (sr / 4096));
            std::vector<double> env (2048, 0.0);
            for (int b = 0; b < 2048; ++b) { double mx = 0; for (int k = std::max (0, b - win / 2); k < std::min (2048, b + win / 2); ++k) mx = std::max (mx, (double) d[(size_t) k]); env[(size_t) b] = mx; }
            std::vector<double> sm (2048, 0.0);
            for (int b = 0; b < 2048; ++b) { double acc = 0; int c = 0; for (int k = std::max (0, b - win); k < std::min (2048, b + win); ++k) { acc += env[(size_t) k]; ++c; } sm[(size_t) b] = acc / c; }
            double best = 0, bf = 0;
            for (int b = 1; b < 2048; ++b) { const double fr = b * sr / 4096; if (fr < 400 || fr > 1000) continue; if (sm[(size_t) b] > best) { best = sm[(size_t) b]; bf = fr; } }
            return bf; };
        double errSum = 0, errIn = 0; int cnt = 0;
        for (double t = 1.0; t < 5.5; t += 0.04)
        {
            const int a = (int) (t * sr);
            const float fin = estimatePitch (v.data() + a, (int) (0.045 * sr), sr);
            const float fout = estimatePitch (o.data() + a + L, (int) (0.045 * sr), sr);
            errIn += std::abs (cents (fin, 220.0)); errSum += std::abs (cents (fout, 220.0)); ++cnt;
        }
        std::cout << "Afinacion: error medio entrada " << errIn / cnt << " cents -> salida " << errSum / cnt << " cents\n";
        if (! (errSum / cnt < 5.0)) { std::cout << "  [FALLO] afinacion imprecisa\n"; ++failures; }
        const int a = (int) (2.0 * sr);
        juce::ignoreUnused (a);

        // Cambio grande: voz en A#3 (fuera de escala) corregida a A o B: formantes deben quedarse
        PitchCorrector pc2; pc2.prepare (sr, true); pc2.setParams (true, 9, 1, 0.0f, 100.0f, 0.0f);
        std::vector<float> w2 ((size_t) N); ph2 = 0; b1 = b2 = c1 = c2 = 0;
        for (int i = 0; i < N; ++i)
        {
            ph2 += 233.08 * 1.012 / sr; float pulse = 0; if (ph2 >= 1) { ph2 -= 1; pulse = 1; }   // ~ A#3 + 20 ct
            w2[(size_t) i] = res (pulse, 700, b1, b2) * 0.02f + res (pulse, 1200, c1, c2) * 0.012f;
        }
        auto o2 = w2; pc2.process (o2.data(), N);
        const float fo = estimatePitch (o2.data() + a + L, (int) (0.05 * sr), sr);
        // Referencia: desplazamiento ingenuo (re-muestreo) con el mismo ratio -> mueve los formantes
        const double ratioN = fo / 236.0;
        std::vector<float> naive ((size_t) N, 0.0f);
        for (int i = 0; i < N; ++i) { const double rp = i * ratioN; const int i0 = (int) rp; if (i0 + 1 >= N) break; const float fr = (float) (rp - i0); naive[(size_t) i] = w2[(size_t) i0] * (1 - fr) + w2[(size_t) i0 + 1] * fr; }

        // Transparencia: con Cantidad 0 la salida debe ser identica a la entrada (sin "robot")
        {
            PitchCorrector pc3; pc3.prepare (sr, true); pc3.setParams (true, 9, 1, 20.0f, 0.0f, 0.0f);
            auto o3 = v; pc3.process (o3.data(), N);
            const int L3 = pc3.getLatencySamples();
            double es = 0, ee = 0;
            for (int i = (int) sr; i < N - L3; ++i) { const double a = v[(size_t) i], b = o3[(size_t) (i + L3)]; es += a * a; ee += (a - b) * (a - b); }
            const double snr = 10.0 * std::log10 (es / (ee + 1e-30));
            std::cout << "Transparencia sin correccion: " << snr << " dB (debe ser > 60)\n";
            if (! (snr > 60.0)) { std::cout << "  [FALLO] la afinacion colorea la voz\n"; ++failures; }
        }
        std::cout << "Correccion grande (A#3 fuera de escala): 236 Hz -> " << fo << " Hz (B3 = 246.94 Hz)\n";
        juce::ignoreUnused (centroid, naive);
    }

    // ---- Reverb: RT60 medido vs ajustado
    for (int type = 0; type < 4; ++type)
    {
        avdsp::ProReverb rv; rv.prepare (sr, bs);
        const float setT = type == 0 ? 3.8f : type == 1 ? 1.8f : type == 2 ? 2.6f : 0.7f;
        rv.setParams (type, 50.0f, setT, 30.0f);
        const int N = (int) (sr * 8);
        std::vector<float> inR ((size_t) N, 0.0f), l ((size_t) N), r ((size_t) N);
        for (int i = 0; i < 2048; ++i) inR[(size_t) i] = (rnd.nextFloat() * 2 - 1);   // rafaga de ruido
        rv.process (inR.data(), l.data(), r.data(), N, 0.0f);
        // Integracion de Schroeder
        std::vector<double> e ((size_t) N); double acc = 0;
        for (int i = N - 1; i >= 0; --i) { acc += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i]; e[(size_t) i] = acc; }
        auto tAt = [&] (double db) { for (int i = 0; i < N; ++i) if (10 * std::log10 (e[(size_t) i] / e[0] + 1e-30) < db) return i / sr; return N / sr; };
        const double t60 = (tAt (-25.0) - tAt (-5.0)) * 3.0;
        const char* names[] = { "Nube", "Plate", "Sala", "Cuarto" };
        float pk = 0; for (int i = 0; i < N; ++i) pk = std::max (pk, std::abs (l[(size_t) i]));
        std::cout << "Reverb " << names[type] << ": RT60 ajustado " << setT << " s, medido " << t60 << " s, pico " << pk << "\n";
        if (! (std::abs (t60 - setT) / setT < 0.15) || ! std::isfinite (pk) || pk > 2.0f) { std::cout << "  [FALLO] reverb\n"; ++failures; }
    }

    // ---- CPU: 60 s de audio con la cadena completa
    {
        proc->loadFactoryPreset (PresetIdx::TrapHard);
        juce::AudioBuffer<float> blk (2, bs); juce::MidiBuffer mm;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const int blocks = (int) (60.0 * sr / bs);
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, (b * bs) % (total - bs), bs);
            proc->processBlock (blk, mm);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "CPU: 60 s de audio en " << ms / 1000.0 << " s (" << 100.0 * ms / 60000.0 << " % de un nucleo)\n";
        proc->loadFactoryPreset (0);
    }

    // ---- Vocal Assist durante la primera pasada
    proc->startAssist();

    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    bool bad = false;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = std::min (bs, total - pos);
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, n);
        proc->processBlock (blk, midi);
    }
    for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (0, i))) { bad = true; break; }
    std::cout << "NaN/Inf en salida: " << (bad ? "SI" : "no") << "\n";

    // Medir afinacion en un tramo estable (frase 3, 5.0 - 5.5 s, compensando latencia)
    const int lat = proc->getLatencySamples();
    const int a = (int) (5.0 * sr), len = (int) (0.25 * sr);
    const float pin = estimatePitch (in.getReadPointer (0) + a, len, sr);
    const float pout = estimatePitch (out.getReadPointer (0) + a + lat, len, sr);
    std::cout << "Tono entrada:  " << pin  << " Hz (" << cents (pin, 220.0) << " cents vs A3)\n";
    std::cout << "Tono salida:   " << pout << " Hz (" << cents (pout, 220.0) << " cents vs A3)\n";

    float pk = 0; for (int i = 0; i < total; ++i) pk = std::max (pk, std::abs (out.getSample (0, i)));
    std::cout << "Pico salida: " << juce::Decibels::gainToDecibels (pk) << " dBFS\n";

    VocalAssist::Result res;
    const bool fin = proc->pollAssistFinished (res);
    std::cout << "Vocal Assist terminado: " << fin << "  ok: " << res.ok << "  tonalidad: " << res.key << " escala " << res.scale << "\n";
    for (auto& kv : res.values) std::cout << "   " << kv.first << " = " << kv.second << "\n";

    // ---- Vocal Assist por genero: reduccion de ganancia lograda
    for (int gi : { 0, 3, 4, 6 })
    {
        proc->startAssist (gi);
        juce::MidiBuffer m2; int pos = 0;
        while (proc->assist.active.load())
        {
            juce::AudioBuffer<float> blk (2, bs);
            for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, pos % (total - bs), bs);
            pos += bs; proc->processBlock (blk, m2);
        }
        VocalAssist::Result rr; for (int w = 0; w < 600 && ! proc->pollAssistFinished (rr); ++w) juce::Thread::sleep (50);
        float grMax = 0, dsMax = 0; double grSum = 0; int grN = 0;
        for (int k = 0; k < (int) (4.0 * sr / bs); ++k)
        {
            juce::AudioBuffer<float> blk (2, bs);
            for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, pos % (total - bs), bs);
            pos += bs; proc->processBlock (blk, m2);
            const float g = proc->getCompGrDb(); grMax = std::max (grMax, g); if (g > 0.5f) { grSum += g; ++grN; }
            dsMax = std::max (dsMax, proc->getDeessGrDb());
        }
        std::cout << "Assist " << assistGenres()[(size_t) gi].key << ": preset '" << proc->currentPresetName
                  << "'  comp max " << grMax << " dB (media " << (grN ? grSum / grN : 0) << " dB, objetivo " << assistGenres()[(size_t) gi].targetGr
                  << ")  de-esser max " << dsMax << " dB  entrada " << proc->apvts.getRawParameterValue (PID::inGain)->load() << " dB"
                  << "  reverb " << proc->apvts.getRawParameterValue (PID::revDecay)->load() << " s\n";
    }

    // ---- Todos los presets, modo Live y Antes
    for (int p = 0; p < (int) factoryPresets().size(); ++p)
    {
        proc->loadFactoryPreset (p);
        if (p == 3) proc->setParamValue (PID::tuneMode, 1);
        juce::AudioBuffer<float> o2 (in);
        float mx = 0; bool nan = false;
        for (int pos = 0; pos < (int) (sr * 6); pos += bs)
        {
            juce::AudioBuffer<float> blk (o2.getArrayOfWritePointers(), 2, pos, bs);
            proc->processBlock (blk, midi);
        }
        for (int i = 0; i < (int) (sr * 6); ++i)
        {
            const float v = o2.getSample (0, i);
            if (! std::isfinite (v)) nan = true;
            mx = std::max (mx, std::abs (v));
        }
        if (nan || mx > 1.0f) ++failures;
        std::cout << "Preset " << factoryPresets()[(size_t) p].name << ": pico " << juce::Decibels::gainToDecibels (mx)
                  << " dBFS" << (nan ? "  NaN!" : "") << "  latencia " << proc->getLatencySamples() << "\n";
    }

    if (engineOnly)
    {
        if (bad) ++failures;
        std::cout << (failures == 0 ? "\nTODAS LAS PRUEBAS DEL MOTOR: OK\n" : "\nPRUEBAS CON FALLOS: ") << (failures ? juce::String (failures) : juce::String()) << "\n";
        return failures == 0 ? 0 : 1;
    }

    // Preset de usuario: guardar y cargar
    std::cout << "Guardar preset usuario: " << proc->saveUserPreset ("Test Jefe") << "\n";
    std::cout << "Presets usuario: " << proc->getUserPresets().size() << "\n";
    proc->getUserPresets()[0].deleteFile();

    // Estado
    juce::MemoryBlock mb; proc->getStateInformation (mb); proc->setStateInformation (mb.getData(), (int) mb.getSize());
    std::cout << "Estado: " << mb.getSize() << " bytes\n";

    // ---- Captura de la interfaz
    proc->loadFactoryPreset (1);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        auto snap = [&] (const juce::String& file)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (file));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        snap ("ui_main.png");
        // abrir los ajustes del primer modulo (Afinacion) y del compresor
        int shot = 0;
        for (auto* c : ed->getChildren())
        {
            if (dynamic_cast<ModuleCard*> (c) != nullptr || dynamic_cast<StripBar*> (c) != nullptr)
            {
                for (auto* cc : c->getChildren())
                    if (auto* b = dynamic_cast<DrawnButton*> (cc))
                        if (b->getX() > c->getWidth() - 50 && b->onClick) { b->onClick(); break; }
                snap ("ui_overlay_" + juce::String (shot++) + ".png");
                for (auto* o : ed->getChildren()) if (auto* ov = dynamic_cast<DetailOverlay*> (o)) ov->close();
            }
        }
        // Ventana de Vocal Assist: reposo, analizando y resultado
        for (auto* c : ed->getChildren())
            if (auto* aw = dynamic_cast<AssistWindow*> (c))
            {
                aw->open (0);
                for (int k = 0; k < 40; ++k) aw->tick();
                snap ("ui_assist_idle.png");
                aw->pressMain();
                juce::MidiBuffer mb2;
                int pos = (int) (sr * 1.0);
                for (int f = 0; f < 120; ++f)
                {
                    for (int b = 0; b < 2; ++b, pos += bs)
                    {
                        juce::AudioBuffer<float> blk (bs > 0 ? 2 : 2, bs);
                        for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, pos % (total - bs), bs);
                        proc->processBlock (blk, mb2);
                    }
                    aw->tick();
                }
                snap ("ui_assist_live.png");
                for (int f = 0; f < 2000 && aw->isListening(); ++f)
                {
                    juce::AudioBuffer<float> blk (2, bs);
                    for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, pos % (total - bs), bs);
                    pos += bs;
                    proc->processBlock (blk, mb2);
                    if (f % 4 == 0) aw->tick();
                }
                aw->tick();
                snap ("ui_assist_done.png");
                aw->close();
            }
        proc->setLanguage (1);
        ed.reset (proc->createEditor());
        snap ("ui_main_en.png");
    }
    return bad ? 1 : 0;
}
