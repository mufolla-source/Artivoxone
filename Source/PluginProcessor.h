#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "Parameters.h"
#include "Presets.h"
#include "DSP/PitchCorrector.h"
#include "DSP/Dynamics.h"
#include "DSP/Effects.h"
#include "DSP/EqMath.h"
#include "DSP/VocalAssist.h"

class ArtisVoxOneProcessor : public juce::AudioProcessor
{
public:
    ArtisVoxOneProcessor();
    ~ArtisVoxOneProcessor() override { if (refineThread != nullptr) refineThread->stopThread (5000); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ArtisVox One"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // ---- Estilos por modulo (flechas de las tarjetas): 0 Afinacion, 1 Tono, 2 Espacio, 3 Caracter
    void applyStyle (int module, int index);

    // ---- A / B: dos memorias de ajustes
    void toggleAB();
    int  getABSlot() const { return abSlot; }

    // ---- Analizador de entrada (ventana de Vocal Assist)
    void copyInput (float* dest, int n) const
    {
        const int w = inFifoWrite.load();
        for (int i = 0; i < n; ++i) dest[i] = inFifo[(size_t) ((w - n + i) & (fifoSize - 1))];
    }

    // ---- Analizador (salida mono, para el EQ)
    static constexpr int fifoSize = 4096;
    void copyAnalyzer (float* dest, int n) const
    {
        const int w = fifoWrite.load();
        for (int i = 0; i < n; ++i) dest[i] = fifo[(size_t) ((w - n + i) & (fifoSize - 1))];
    }

    // ---- Presets
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    juce::String currentPresetName { "Por defecto" };

    // ---- Vocal Assist
    void startAssist (int genre = -1);
    int assistGenre = 0;
    bool pollAssistFinished (VocalAssist::Result& out);   // hilo de mensajes
    void undoAssist();
    bool isRefiningAssist() const { return refineStage.load() == 1; }
    bool canUndoAssist() const { return ! assistSnapshot.empty(); }
    VocalAssist assist;

    // ---- Medidores para la interfaz
    std::atomic<float> inLevelDb { -100.0f }, outLevelL { -100.0f }, outLevelR { -100.0f };
    std::atomic<float> hostBpm { 120.0f };
    float getCompGrDb() const   { return compressor.grDb.load(); }
    float getDeessGrDb() const  { return deesser.grDb.load(); }
    float getDetectedHz() const { return activeTune().detectedHz.load(); }
    float getCorrectionCents() const { return activeTune().correctionCents.load(); }
    int   getTargetNote() const { return activeTune().currentNoteIdx.load(); }
    double getCurrentSampleRate() const { return currentSR; }

    // Ajustes de la interfaz guardados en el proyecto
    int  getLanguage() const;
    void setLanguage (int l);
    bool getAdvancedView() const;
    void setAdvancedView (bool a);

    void setParamValue (const juce::String& id, float realValue);

private:
    double currentSR = 44100.0;
    int maxBlock = 512;

    // DSP
    PitchCorrector tuneStudio, tuneLive;
    int tuneModeActive = 0;
    const PitchCorrector& activeTune() const { return tuneModeActive == 0 ? tuneStudio : tuneLive; }

    avdsp::Gate gate;
    juce::dsp::IIR::Filter<float> hpf1, hpf2, eqLow, eqMud, eqPres, eqAir, tiltLow, tiltHigh;
    EqSettings lastEq; bool eqDirty = true;
    avdsp::DeEsser deesser;
    avdsp::Compressor compressor;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    avdsp::ColorProcessor color;
    avdsp::Exciter exciter;
    avdsp::Doubler doubler;
    avdsp::TempoDelay tempoDelay;
    avdsp::ProReverb reverb;
    avdsp::SafetyLimiter limiter;

    // Buffers de trabajo
    juce::AudioBuffer<float> monoBuf, wetBuf, revBuf, dlyBuf;
    juce::AudioBuffer<float> dryDelay; int dryWrite = 0; int dryMask = 0;
    int totalLatency = 0;

    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm, revSm, dlySm;
    float duckEnv = 0.0f;
    bool lastBefore = false; float beforeFade = 0.0f;

    std::vector<std::pair<juce::String, float>> assistSnapshot;
    std::atomic<int> refineStage { 0 };                 // 0 nada, 1 calculando, 2 listo
    std::vector<std::pair<juce::String, float>> refined;
    VocalAssist::Result pendingResult;
    std::unique_ptr<juce::Thread> refineThread;
    void refineAssistWorker (juce::ValueTree state, std::vector<float> audio, GenreProfile prof, double sr);
    std::array<float, fifoSize> fifo {};
    std::atomic<int> fifoWrite { 0 };
    std::array<float, fifoSize> inFifo {};
    std::atomic<int> inFifoWrite { 0 };
    juce::ValueTree abSlots[2];
    int abSlot = 0;

    void updateLatency();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n);
    float pv (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtisVoxOneProcessor)
};
