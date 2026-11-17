#include "PluginProcessor.h"
#include "PluginEditor.h"

ArtisVoxOneProcessor::ArtisVoxOneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "ArtisVoxOne", createParameterLayout())
{
    apvts.state.setProperty ("lang", 0, nullptr);
    apvts.state.setProperty ("advanced", false, nullptr);
    apvts.state.setProperty ("preset", currentPresetName, nullptr);
}

bool ArtisVoxOneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in  != juce::AudioChannelSet::mono() && in  != juce::AudioChannelSet::stereo()) return false;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono()) return false;
    return true;
}

void ArtisVoxOneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    maxBlock = juce::jmax (64, juce::jmin (samplesPerBlock, 1024));

    tuneStudio.prepare (sampleRate, true);
    tuneLive.prepare (sampleRate, false);
    tuneModeActive = (int) pv (PID::tuneMode);

    gate.prepare (sampleRate);
    for (auto* f : { &hpf1, &hpf2, &eqLow, &eqMud, &eqPres, &eqAir, &tiltLow, &tiltHigh }) f->reset();
    eqDirty = true;
    deesser.prepare (sampleRate);
    compressor.prepare (sampleRate);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, 2,
                        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    oversampler->reset();

    color.prepare (sampleRate, maxBlock, (int) std::round (oversampler->getLatencyInSamples()));
    exciter.prepare (sampleRate);
    doubler.prepare (sampleRate);
    tempoDelay.prepare (sampleRate);
    reverb.prepare (sampleRate, maxBlock);
    limiter.prepare (sampleRate);
    assist.prepare (sampleRate);

    monoBuf.setSize (1, maxBlock);
    wetBuf.setSize (2, maxBlock);
    revBuf.setSize (2, maxBlock);
    dlyBuf.setSize (2, maxBlock);

    const int maxLat = juce::jmax (tuneStudio.getLatencySamples(), tuneLive.getLatencySamples()) + 1024;
    const int dsz = juce::nextPowerOfTwo (maxLat + maxBlock + 8);
    dryDelay.setSize (2, dsz); dryDelay.clear(); dryMask = dsz - 1; dryWrite = 0;

    for (auto* s : { &inGainSm, &outGainSm, &mixSm, &revSm, &dlySm }) s->reset (sampleRate, 0.03);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pv (PID::inGain)));
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pv (PID::outGain)));
    mixSm.setCurrentAndTargetValue (pv (PID::mix) * 0.01f);
    revSm.setCurrentAndTargetValue (pv (PID::revMix) * 0.01f);
    dlySm.setCurrentAndTargetValue (pv (PID::dlyMix) * 0.01f);
    duckEnv = 0.0f;
    beforeFade = pv (PID::before) > 0.5f ? 1.0f : 0.0f;

    updateLatency();
}

void ArtisVoxOneProcessor::updateLatency()
{
    const int tl = tuneModeActive == 0 ? tuneStudio.getLatencySamples() : tuneLive.getLatencySamples();
    const int ol = oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    totalLatency = tl + ol + deesser.getLatencySamples();
    setLatencySamples (totalLatency);
}

void ArtisVoxOneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, buffer.getNumSamples());

    // Cambio de modo de afinacion Studio/Live -> cambia la latencia
    const int mode = (int) pv (PID::tuneMode);
    if (mode != tuneModeActive)
    {
        tuneModeActive = mode;
        (mode == 0 ? tuneStudio : tuneLive).prepare (currentSR, mode == 0);
        updateLatency();
    }

    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, total - start));
}

void ArtisVoxOneProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n)
{
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const double sr = currentSR;

    // ---------------- Parametros
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pv (PID::inGain)));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pv (PID::outGain)));
    mixSm.setTargetValue (pv (PID::mix) * 0.01f);
    revSm.setTargetValue (pv (PID::revMix) * 0.01f);
    dlySm.setTargetValue (pv (PID::dlyMix) * 0.01f);

    auto& tune = tuneModeActive == 0 ? tuneStudio : tuneLive;
    tune.setParams (pv (PID::tuneOn) > 0.5f, (int) pv (PID::tuneKey), (int) pv (PID::tuneScale),
                    pv (PID::tuneSpeed), pv (PID::tuneAmount), pv (PID::tuneHuman));
    gate.setThreshold (pv (PID::gateThresh));

    EqSettings eq { pv (PID::hpfFreq), pv (PID::lowFreq), pv (PID::lowGain), pv (PID::mudFreq), pv (PID::mudGain),
                    pv (PID::presFreq), pv (PID::presGain), pv (PID::airFreq), pv (PID::airGain), pv (PID::toneTilt) };
    if (eqDirty || std::memcmp (&eq, &lastEq, sizeof (EqSettings)) != 0)
    {
        auto c = EqCoeffs::make (sr, eq);
        hpf1.coefficients = c.hpf1; hpf2.coefficients = c.hpf2; eqLow.coefficients = c.low; eqMud.coefficients = c.mud;
        eqPres.coefficients = c.pres; eqAir.coefficients = c.air; tiltLow.coefficients = c.tiltLow; tiltHigh.coefficients = c.tiltHigh;
        lastEq = eq; eqDirty = false;
    }

    deesser.setParams (pv (PID::deessAmount), pv (PID::deessFreq));
    compressor.setParams (pv (PID::compAmount), pv (PID::compRatio), pv (PID::compAttack), pv (PID::compRelease),
                          (int) pv (PID::compStyle), pv (PID::compMakeup), pv (PID::compAuto) > 0.5f, pv (PID::compMix));
    reverb.setParams ((int) pv (PID::spaceStyle), pv (PID::revSize), pv (PID::revDecay), pv (PID::revDamp));

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
    hostBpm.store ((float) bpm);
    const double delaySec = delayDivBeats ((int) pv (PID::dlyDiv)) * 60.0 / bpm;

    // ---------------- Entrada -> mono
    float* mono = monoBuf.getWritePointer (0);
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numIn; ++ch) s += buffer.getSample (ch, start + i);
        s /= (float) juce::jmax (1, numIn);

        // Guardar la senal original para Antes/Despues y la mezcla en paralelo
        for (int ch = 0; ch < 2; ++ch)
            dryDelay.setSample (ch, (dryWrite + i) & dryMask, buffer.getSample (juce::jmin (ch, numIn - 1), start + i));

        s *= inGainSm.getNextValue();
        mono[i] = s;
        inPeak = std::max (inPeak, std::abs (s));
        const int iw = inFifoWrite.load (std::memory_order_relaxed);
        inFifo[(size_t) iw] = s;
        inFifoWrite.store ((iw + 1) & (fifoSize - 1), std::memory_order_release);
    }
    inLevelDb.store (juce::Decibels::gainToDecibels (inPeak, -100.0f));

    // ---------------- Cadena vocal (mono)
    assist.feed (mono, n, tune);
    tune.process (mono, n);
    const bool toneOn = pv (PID::toneOn) > 0.5f, spaceOn = pv (PID::spaceOn) > 0.5f, charOn = pv (PID::charOn) > 0.5f;
    gate.process (mono, n);
    for (int i = 0; i < n; ++i)
    {
        float s = hpf2.processSample (hpf1.processSample (mono[i]));
        const float e = eqAir.processSample (eqPres.processSample (eqMud.processSample (eqLow.processSample (s))));
        mono[i] = toneOn ? e : s;
    }
    deesser.process (mono, n, pv (PID::deessOn) > 0.5f);   // siempre procesa (latencia constante)
    if (pv (PID::compOn) > 0.5f)  compressor.process (mono, n); else compressor.grDb.store (0.0f);

    // Color sobremuestreado x4
    {
        const int st = (int) pv (PID::satType);
        const float drv = charOn ? pv (PID::satDrive) : 0.0f;
        color.pre (mono, n, st, drv);
        float* chans[] = { mono };
        juce::dsp::AudioBlock<float> blk (chans, 1, (size_t) n);
        auto up = oversampler->processSamplesUp (blk);
        color.shape (up.getChannelPointer (0), (int) up.getNumSamples(), st, drv);
        oversampler->processSamplesDown (blk);
        color.post (mono, n, st, drv, charOn ? pv (PID::satMix) : 0.0f);
    }

    for (int i = 0; i < n; ++i)
    {
        const float t = tiltHigh.processSample (tiltLow.processSample (mono[i]));
        if (toneOn) mono[i] = t;
    }
    exciter.process (mono, n, toneOn ? pv (PID::toneExciter) : 0.0f);

    // ---------------- Estereo: doblador, reverb y delay
    float* wl = wetBuf.getWritePointer (0); float* wr = wetBuf.getWritePointer (1);
    juce::FloatVectorOperations::copy (wl, mono, n);
    juce::FloatVectorOperations::copy (wr, mono, n);
    doubler.process (mono, wl, wr, n, pv (PID::doublerOn) > 0.5f ? pv (PID::doubler) : 0.0f, (int) pv (PID::doublerVoices) == 1);

    float* rl = revBuf.getWritePointer (0); float* rr = revBuf.getWritePointer (1);
    float* dl = dlyBuf.getWritePointer (0); float* dr = dlyBuf.getWritePointer (1);
    reverb.process (mono, rl, rr, n, pv (PID::revPre));
    tempoDelay.process (mono, dl, dr, n, delaySec, pv (PID::dlyFeedback), pv (PID::dlyPingPong) > 0.5f);

    const float duck = pv (PID::spaceDuck) * 0.01f;
    const float duckAtt = avdsp::msCoeff (10.0f, sr), duckRel = avdsp::msCoeff (250.0f, sr);
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (mono[i]);
        duckEnv = a > duckEnv ? a + (duckEnv - a) * duckAtt : a + (duckEnv - a) * duckRel;
        const float envDb = juce::Decibels::gainToDecibels (duckEnv, -100.0f);
        const float dg = 1.0f - duck * 0.8f * juce::jlimit (0.0f, 1.0f, (envDb + 40.0f) / 30.0f);
        const float rv = revSm.getNextValue() * 0.7f * (spaceOn ? 1.0f : 0.0f), dv = dlySm.getNextValue() * 0.85f * (spaceOn ? 1.0f : 0.0f);
        wl[i] += (rl[i] * rv + dl[i] * dv) * dg;
        wr[i] += (rr[i] * rv + dr[i] * dv) * dg;
    }

    // Ancho estereo (M/S) del modulo Caracter
    const float wdt = charOn ? pv (PID::width) * 0.01f : 1.0f;
    if (std::abs (wdt - 1.0f) > 0.001f)
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (wl[i] + wr[i]), sd = 0.5f * (wl[i] - wr[i]) * wdt;
            wl[i] = m + sd; wr[i] = m - sd;
        }

    // ---------------- Mezcla, Antes/Despues, salida
    const bool before = pv (PID::before) > 0.5f;
    const float fadeStep = (float) (1.0 / (0.02 * sr));
    const bool limOn = pv (PID::limiter) > 0.5f;

    for (int i = 0; i < n; ++i)
    {
        const int rp = (dryWrite + i - totalLatency) & dryMask;
        const float dryL = dryDelay.getSample (0, rp), dryR = dryDelay.getSample (1, rp);
        const float m = mixSm.getNextValue(), og = outGainSm.getNextValue();
        const float gIn = inGainSm.getCurrentValue();
        float outL = (dryL * gIn * (1.0f - m) + wl[i] * m) * og;
        float outR = (dryR * gIn * (1.0f - m) + wr[i] * m) * og;
        wl[i] = outL; wr[i] = outR;
        rl[i] = dryL; rr[i] = dryR; // reutilizo el buffer para la senal "Antes"
    }
    if (limOn) limiter.process (wl, wr, n);

    float pkL = 0, pkR = 0;
    for (int i = 0; i < n; ++i)
    {
        beforeFade += before ? fadeStep : -fadeStep;
        beforeFade = juce::jlimit (0.0f, 1.0f, beforeFade);
        const float l = wl[i] * (1.0f - beforeFade) + rl[i] * beforeFade;
        const float r = wr[i] * (1.0f - beforeFade) + rr[i] * beforeFade;

        if (numOut >= 2)
        {
            buffer.setSample (0, start + i, l);
            buffer.setSample (1, start + i, r);
        }
        else buffer.setSample (0, start + i, 0.5f * (l + r));
        pkL = std::max (pkL, std::abs (l)); pkR = std::max (pkR, std::abs (r));
        const int fw = fifoWrite.load (std::memory_order_relaxed);
        fifo[(size_t) fw] = 0.5f * (l + r);
        fifoWrite.store ((fw + 1) & (fifoSize - 1), std::memory_order_release);
    }
    dryWrite = (dryWrite + n) & dryMask;

    outLevelL.store (juce::Decibels::gainToDecibels (pkL, -100.0f));
    outLevelR.store (juce::Decibels::gainToDecibels (pkR, -100.0f));
}

// ============================================================================
//  Parametros, presets y Vocal Assist
// ============================================================================
void ArtisVoxOneProcessor::setParamValue (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void ArtisVoxOneProcessor::loadFactoryPreset (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& pr = list[(size_t) index];

    static const juce::StringArray keep { PID::tuneKey, PID::tuneScale, PID::inGain, PID::before, PID::tuneMode };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (rp->getParameterID()))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }

    for (auto& kv : pr.values) setParamValue (kv.first, kv.second);
    currentPresetName = pr.name;
    apvts.state.setProperty ("preset", currentPresetName, nullptr);
}

juce::File ArtisVoxOneProcessor::getUserPresetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("ArtiVox").getChildFile ("ArtisVox One").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> ArtisVoxOneProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.avpreset");
    files.sort();
    return files;
}

bool ArtisVoxOneProcessor::saveUserPreset (const juce::String& name)
{
    auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    auto state = apvts.copyState();
    state.setProperty ("presetName", name.trim(), nullptr);
    if (auto xml = state.createXml())
    {
        auto file = getUserPresetFolder().getChildFile (safe + ".avpreset");
        if (xml->writeTo (file))
        {
            currentPresetName = name.trim();
            apvts.state.setProperty ("preset", currentPresetName, nullptr);
            return true;
        }
    }
    return false;
}

bool ArtisVoxOneProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return false;

    for (auto child : tree)
    {
        if (! child.hasType ("PARAM")) continue;
        const auto id = child.getProperty ("id").toString();
        if (id == PID::before || id == PID::inGain) continue;
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
            p->endChangeGesture();
        }
    }
    currentPresetName = file.getFileNameWithoutExtension();
    apvts.state.setProperty ("preset", currentPresetName, nullptr);
    return true;
}

void ArtisVoxOneProcessor::startAssist (int genre)
{
    if (genre >= 0) assistGenre = genre;
    apvts.state.setProperty ("assistGenre", assistGenre, nullptr);
    assist.start();
}

bool ArtisVoxOneProcessor::pollAssistFinished (VocalAssist::Result& out)
{
    // Ajuste fino terminado: aplicar los valores afinados
    if (refineStage.load() == 2)
    {
        for (auto& kv : refined) setParamValue (kv.first, kv.second);
        refined.clear();
        refineStage.store (0);
        if (refineThread != nullptr) { refineThread->stopThread (2000); refineThread.reset(); }
        out = pendingResult;
        return true;
    }
    if (refineStage.load() == 1) return false;
    if (! assist.done.load()) return false;
    assist.done.store (false);

    out = assist.compute (pv (PID::inGain));
    if (! out.ok) return true;

    // Guardar estado para "Deshacer Assist"
    assistSnapshot.clear();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            assistSnapshot.push_back ({ rp->getParameterID(), rp->getValue() });

    const auto& genres = assistGenres();
    const auto& G = genres[(size_t) juce::jlimit (0, (int) genres.size() - 1, assistGenre)];
    out.genre = assistGenre;

    // 1) Cadena profesional del genero elegido
    loadFactoryPreset (G.preset);
    auto pget = [this] (const char* id) { return pv (id); };

    // 2) Ganancia de entrada: la voz a ~ -18 dBFS RMS (nivel de trabajo estandar)
    setParamValue (PID::inGain, out.values["inGain"]);

    // 3) Limpieza segun la toma
    setParamValue (PID::gateThresh, out.values["gateThresh"]);
    setParamValue (PID::hpfFreq, juce::jmax (out.hpf, pget (PID::hpfFreq) * 0.85f));

    const auto& prof = G.profile;
    if (prof.refs > 0)
    {
        // 4) EQ por coincidencia con el perfil medido en acapellas profesionales del genero
        const float k = juce::jmin (0.75f, 0.5f + 0.06f * (prof.refs - 1));
        auto match = [&] (const char* id, float target, float measured, float lo, float hi)
        { setParamValue (id, juce::jlimit (lo, hi, (target - measured) * k)); };
        match (PID::lowGain,  prof.low,  out.absLow,  -6.0f, 6.0f);
        match (PID::mudGain,  prof.mud,  out.absMud,  -8.0f, 3.0f);
        match (PID::presGain, prof.pres, out.absPres, -4.0f, 7.0f);
        match (PID::airGain,  prof.air,  out.absAir,  -3.0f, 8.0f);

        // 5) De-esser: sibilancia prevista despues del EQ frente a la del perfil
        const float predicted = out.absSib + pget (PID::presGain) * 0.3f + pget (PID::airGain) * 0.5f;
        setParamValue (PID::deessAmount, juce::jlimit (15.0f, 85.0f, pget (PID::deessAmount) + (predicted - prof.sib) * 5.0f));
    }
    else
    {
        // 4) EQ: el perfil del preset + correccion de lo que le sobra o falta a esta voz
        auto eq = [&] (const char* id, float dev, float kk, float lo, float hi)
        { setParamValue (id, juce::jlimit (lo, hi, pget (id) - dev * kk)); };
        eq (PID::lowGain,  out.lowRel,  0.5f, -6.0f, 6.0f);
        eq (PID::mudGain,  out.mudRel,  0.6f, -8.0f, 2.0f);
        eq (PID::presGain, out.presRel, 0.5f, -4.0f, 6.0f);
        eq (PID::airGain,  out.airRel,  0.5f, -3.0f, 7.0f);

        // 5) De-esser segun la energia sibilante
        setParamValue (PID::deessAmount, juce::jlimit (15.0f, 85.0f, pget (PID::deessAmount) + out.sibRel * 4.0f));
    }

    // 6) Compresion: umbral calculado para lograr la reduccion del genero
    //    (con perfil: la necesaria para llevar tu rango dinamico al de las referencias)
    {
        float targetGr = G.targetGr;
        if (prof.refs > 0) targetGr = juce::jlimit (1.5f, 9.0f, (out.dynRange - prof.dynRange) * 1.5f);
        const int style = (int) pget (PID::compStyle);
        const float ratio = pget (PID::compRatio);
        const float slope = style == 3 ? 0.92f : (1.0f - 1.0f / juce::jmax (1.5f, style == 1 ? 3.0f : ratio));
        const float thr = out.p90Db - targetGr / slope;
        setParamValue (PID::compAmount, juce::jlimit (10.0f, 95.0f, -thr / 0.42f));
    }

    // 7) Reverb: que la cola no pise el siguiente compas (max ~ 1 compas de 4/4)
    {
        const float bar = 240.0f / juce::jmax (60.0f, hostBpm.load());
        setParamValue (PID::revDecay, juce::jmin (pget (PID::revDecay), juce::jmax (0.6f, bar)));
    }

    // 8) Tonalidad detectada
    if (out.key >= 0)
    {
        setParamValue (PID::tuneKey, (float) out.key);
        setParamValue (PID::tuneScale, (float) out.scale);
    }
    currentPresetName = juce::String ("Assist - ") + juce::String::fromUTF8 (factoryPresets()[(size_t) G.preset].name.toRawUTF8());
    apvts.state.setProperty ("preset", currentPresetName, nullptr);

    // 9) Ajuste fino por simulacion: se procesa la toma con la cadena propuesta,
    //    se mide el resultado y se corrige hasta parecerse al perfil del genero.
    if (prof.refs > 0 && assist.getRecording().size() > (size_t) (getSampleRate() * 4))
    {
        pendingResult = out;
        refineStage.store (1);
        struct Worker : juce::Thread
        {
            Worker (std::function<void()> f) : juce::Thread ("ArtisVoxAssist"), fn (std::move (f)) {}
            void run() override { fn(); }
            std::function<void()> fn;
        };
        auto st = apvts.copyState();
        auto audio = assist.getRecording();
        // la grabacion ya lleva la ganancia de entrada anterior: quitarla (la simulacion aplica la nueva)
        const float oldGain = juce::Decibels::decibelsToGain (-(out.values["inGain"] - out.gainDelta));
        for (auto& v : audio) v *= oldGain;
        const double sr = getSampleRate();
        refineThread = std::make_unique<Worker> ([this, st, audio, prof, sr] { refineAssistWorker (st, audio, prof, sr); });
        refineThread->startThread();
        return false;
    }
    return true;
}

void ArtisVoxOneProcessor::refineAssistWorker (juce::ValueTree state, std::vector<float> audio, GenreProfile prof, double sr)
{
    ArtisVoxOneProcessor sim;
    sim.setPlayConfigDetails (2, 2, sr, 512);
    sim.prepareToPlay (sr, 512);
    sim.apvts.replaceState (state.createCopy());
    sim.setParamValue (PID::before, 0.0f);

    struct Band { const char* id; float GenreProfile::* target; float VocalAssist::Measure::* meas; float lo, hi; };
    const Band bands[] = { { PID::lowGain,  &GenreProfile::low,  &VocalAssist::Measure::low,  -8.0f, 6.0f },
                           { PID::mudGain,  &GenreProfile::mud,  &VocalAssist::Measure::mud,  -9.0f, 4.0f },
                           { PID::presGain, &GenreProfile::pres, &VocalAssist::Measure::pres, -5.0f, 8.0f },
                           { PID::airGain,  &GenreProfile::air,  &VocalAssist::Measure::air,  -4.0f, 9.0f } };
    // Usar los 10 s con mas voz de la toma (la grabacion puede empezar en silencio)
    {
        const int B = (int) (0.1 * sr);
        const int nb = (int) audio.size() / B, win = 100;
        if (nb > win)
        {
            std::vector<double> e ((size_t) nb, 0.0);
            for (int b = 0; b < nb; ++b) for (int i = 0; i < B; ++i) { const float v = audio[(size_t) (b * B + i)]; e[(size_t) b] += v * v; }
            double cur = 0, best = -1; int bestStart = 0;
            for (int b = 0; b < nb; ++b)
            {
                cur += e[(size_t) b];
                if (b >= win) cur -= e[(size_t) (b - win)];
                if (b >= win - 1 && cur > best) { best = cur; bestStart = b - win + 1; }
            }
            audio.erase (audio.begin(), audio.begin() + (long) bestStart * B);
        }
    }
    const int len = (int) juce::jmin ((size_t) (sr * 10.0), audio.size());
    std::vector<float> outMono ((size_t) len);
    const float k = juce::jmin (0.85f, 0.65f + 0.05f * (prof.refs - 1));

    for (int iter = 0; iter < 4 && ! juce::Thread::currentThreadShouldExit(); ++iter)
    {
        sim.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer mb;
        for (int p = 0; p < len; p += 512)
        {
            const int nn = juce::jmin (512, len - p);
            b.clear();
            for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, audio.data() + p, nn);
            sim.processBlock (b, mb);
            for (int i = 0; i < nn; ++i) outMono[(size_t) (p + i)] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        }
        const auto m = VocalAssist::measure (outMono.data(), len, sr);
       #if ARTISVOX_DEBUG_ASSIST
        fprintf (stderr, "iter %d ok %d low %.1f mud %.1f pres %.1f sib %.1f air %.1f dyn %.1f | presGain %.2f\n", iter, (int) m.ok, m.low, m.mud, m.pres, m.sib, m.air, m.dyn, sim.pv (PID::presGain));
       #endif
        if (! m.ok) break;

        for (auto& bd : bands)
        {
            const float cur = sim.pv (bd.id);
            sim.setParamValue (bd.id, juce::jlimit (bd.lo, bd.hi, cur + (prof.*(bd.target) - m.*(bd.meas)) * k));
        }
        // Si el brillo esta al maximo y aun falta aire, bajar la frecuencia del estante
        if (m.air < prof.air - 1.0f && sim.pv (PID::airGain) >= 8.5f)
            sim.setParamValue (PID::airFreq, juce::jmax (7000.0f, sim.pv (PID::airFreq) * 0.85f));
        sim.setParamValue (PID::deessAmount, juce::jlimit (10.0f, 90.0f, sim.pv (PID::deessAmount) + (m.sib - prof.sib) * 4.0f));
        sim.setParamValue (PID::compAmount, juce::jlimit (10.0f, 95.0f, sim.pv (PID::compAmount) + (m.dyn - prof.dynRange) * 3.0f));
    }

    std::vector<std::pair<juce::String, float>> vals;
    for (auto* id : { PID::lowGain, PID::mudGain, PID::presGain, PID::airGain, PID::airFreq, PID::deessAmount, PID::compAmount })
        vals.push_back ({ id, sim.pv (id) });
    refined = std::move (vals);
    refineStage.store (2);
}

void ArtisVoxOneProcessor::undoAssist()
{
    for (auto& kv : assistSnapshot)
        if (auto* p = apvts.getParameter (kv.first))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (kv.second);
            p->endChangeGesture();
        }
    assistSnapshot.clear();
}

void ArtisVoxOneProcessor::applyStyle (int module, int index)
{
    index = ((index % 4) + 4) % 4;
    using V = std::vector<std::pair<const char*, float>>;
    V v;
    switch (module)
    {
        case 0: // Afinacion: Rapido, Natural, Suave, Robot
        {
            static const V s[4] = { { { PID::tuneSpeed, 4 },  { PID::tuneHuman, 10 }, { PID::tuneAmount, 100 } },
                                    { { PID::tuneSpeed, 25 }, { PID::tuneHuman, 40 }, { PID::tuneAmount, 90 } },
                                    { { PID::tuneSpeed, 70 }, { PID::tuneHuman, 60 }, { PID::tuneAmount, 75 } },
                                    { { PID::tuneSpeed, 0 },  { PID::tuneHuman, 0 },  { PID::tuneAmount, 100 } } };
            v = s[index]; setParamValue (PID::tuneStyle, (float) index); break;
        }
        case 1: // Tono: Brillante, Calido, Oscuro, Neutro
        {
            static const V s[4] = { { { PID::toneTilt, 2 },  { PID::toneExciter, 30 }, { PID::airGain, 3 } },
                                    { { PID::toneTilt, -1.5f }, { PID::toneExciter, 5 }, { PID::lowGain, 2 } },
                                    { { PID::toneTilt, -3 }, { PID::toneExciter, 0 }, { PID::airGain, -2 } },
                                    { { PID::toneTilt, 0 },  { PID::toneExciter, 0 } } };
            v = s[index]; setParamValue (PID::toneStyle, (float) index); break;
        }
        case 2: // Espacio: Nube, Plate, Sala, Cuarto
        {
            // Valores tipicos de la industria: plate 1-2.5 s, hall 2-4 s (pre-delay 25-45 ms), room 0.4-0.9 s
            static const V s[4] = { { { PID::revSize, 85 }, { PID::revDamp, 45 }, { PID::revPre, 45 }, { PID::revDecay, 3.8f } },
                                    { { PID::revSize, 50 }, { PID::revDamp, 25 }, { PID::revPre, 20 }, { PID::revDecay, 1.8f } },
                                    { { PID::revSize, 70 }, { PID::revDamp, 50 }, { PID::revPre, 30 }, { PID::revDecay, 2.6f } },
                                    { { PID::revSize, 35 }, { PID::revDamp, 45 }, { PID::revPre, 8 },  { PID::revDecay, 0.7f } } };
            v = s[index]; setParamValue (PID::spaceStyle, (float) index); break;
        }
        case 3: // Caracter: Cinta, Valvula, Drive, Lo-Fi
            setParamValue (PID::satType, (float) index); break;
        default: break;
    }
    for (auto& kv : v) setParamValue (kv.first, kv.second);
}

void ArtisVoxOneProcessor::toggleAB()
{
    abSlots[abSlot] = apvts.copyState();
    abSlot = 1 - abSlot;
    if (abSlots[abSlot].isValid())
    {
        for (auto child : abSlots[abSlot])
        {
            if (! child.hasType ("PARAM")) continue;
            const auto id = child.getProperty ("id").toString();
            if (id == PID::before) continue;
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
        }
    }
}

int ArtisVoxOneProcessor::getLanguage() const       { return (int) apvts.state.getProperty ("lang", 0); }
void ArtisVoxOneProcessor::setLanguage (int l)      { apvts.state.setProperty ("lang", l, nullptr); }
bool ArtisVoxOneProcessor::getAdvancedView() const  { return (bool) apvts.state.getProperty ("advanced", false); }
void ArtisVoxOneProcessor::setAdvancedView (bool a) { apvts.state.setProperty ("advanced", a, nullptr); }

void ArtisVoxOneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ArtisVoxOneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPresetName = apvts.state.getProperty ("preset", "Por defecto").toString();
            assistGenre = (int) apvts.state.getProperty ("assistGenre", 0);
        }
}

juce::AudioProcessorEditor* ArtisVoxOneProcessor::createEditor() { return new ArtisVoxOneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArtisVoxOneProcessor(); }
