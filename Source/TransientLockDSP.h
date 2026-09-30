#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

// =============================================================================
//  TransientLock DSP
//
//  Separación Transient / Sustain en el DOMINIO DE GANANCIA (sin filtros ni
//  crossovers en la ruta de audio):
//    - Un detector diferencial (envolvente rápida vs. lenta) genera una señal
//      de control "t" (0..1) que indica cuánto transiente hay en cada muestra.
//    - El compresor calcula su reducción de ganancia normal (cuerpo/sustain).
//    - La reducción se atenúa por (1 - protección * t): durante el transiente
//      la señal pasa casi intacta; en el cuerpo se comprime completamente.
//  Ventajas: latencia 0, fase perfecta (no hay suma de bandas), sin smearing.
// =============================================================================

namespace tl
{
constexpr int maxChannels = 16;

inline float linToDb (float x) noexcept        { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
inline float dbToGain (float db) noexcept      { return std::exp (db * 0.11512925465f); }

inline float timeToCoeff (float ms, double sampleRate) noexcept
{
    ms = std::max (ms, 0.01f);
    return (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
}

//------------------------------------------------------------------------------
struct Params
{
    float inputDb     = 0.0f;
    float thresholdDb = -18.0f;
    float ratio       = 4.0f;
    float attackMs    = 10.0f;
    float releaseMs   = 150.0f;
    float kneeDb      = 6.0f;
    float makeupDb    = 0.0f;
    float mix         = 1.0f;   // 0..1
    float outputDb    = 0.0f;
    float protection  = 0.6f;   // 0..1  (Transient Protection)
    float bodyAmount  = 1.0f;   // 0..1  (Body Compression)
    bool  bypass      = false;
};

struct Meters
{
    float bodyGrDb    = 0.0f;   // reducción que sufriría el cuerpo (positivo)
    float appliedGrDb = 0.0f;   // reducción realmente aplicada (positivo)
    float transient   = 0.0f;   // actividad del detector 0..1
};

//------------------------------------------------------------------------------
class TransientDetector
{
public:
    void prepare (double sr)
    {
        fastA = timeToCoeff (0.5f, sr);   fastR = timeToCoeff (10.0f, sr);
        slowA = timeToCoeff (30.0f, sr);  slowR = timeToCoeff (200.0f, sr);
        outA  = timeToCoeff (0.2f, sr);   outR  = timeToCoeff (35.0f, sr);
        reset();
    }

    void reset() { fast = slow = t = 0.0f; }

    // level = pico absoluto (lineal) de la muestra actual. Devuelve t en 0..1.
    float process (float level) noexcept
    {
        fast = level > fast ? fastA * fast + (1.0f - fastA) * level
                            : fastR * fast + (1.0f - fastR) * level;
        slow = level > slow ? slowA * slow + (1.0f - slowA) * level
                            : slowR * slow + (1.0f - slowR) * level;

        float raw = 0.0f;
        if (fast > 1.0e-4f)                                  // gate ~ -80 dBFS
        {
            const float diffDb = 8.685889f * std::log ((fast + 1.0e-9f) / (slow + 1.0e-9f));
            const float x = juce::jlimit (0.0f, 1.0f, (diffDb - loDb) / (hiDb - loDb));
            raw = x * x * (3.0f - 2.0f * x);                 // smoothstep
        }

        t = raw > t ? outA * t + (1.0f - outA) * raw
                    : outR * t + (1.0f - outR) * raw;
        return t;
    }

private:
    static constexpr float loDb = 2.5f, hiDb = 9.0f;
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0, outA = 0, outR = 0;
    float fast = 0, slow = 0, t = 0;
};

//------------------------------------------------------------------------------
// Curva estática con soft-knee. Devuelve reducción en dB (<= 0).
inline float computeGainReductionDb (float xDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float slope = 1.0f / ratio - 1.0f;
    const float over  = xDb - thresholdDb;

    if (kneeDb > 0.0f)
    {
        if (2.0f * over < -kneeDb)             return 0.0f;
        if (2.0f * std::abs (over) <= kneeDb)
        {
            const float a = over + 0.5f * kneeDb;
            return slope * a * a / (2.0f * kneeDb);
        }
    }
    else if (over <= 0.0f)
        return 0.0f;

    return slope * over;
}

//------------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detector.prepare (sr);
        const double ramp = 0.02;
        inGain.reset (sr, ramp);  outGain.reset (sr, ramp);
        makeup.reset (sr, ramp);  mixV.reset (sr, ramp);
        bypassV.reset (sr, 0.01);
        snap = true;
        reset();
    }

    void reset()
    {
        detector.reset();
        grState = 0.0f;
    }

    Meters process (juce::AudioBuffer<float>& buffer, const Params& p) noexcept
    {
        Meters m;
        const int numSamples = buffer.getNumSamples();
        const int nch = std::min (buffer.getNumChannels(), maxChannels);
        if (nch <= 0 || numSamples <= 0) return m;

        float* ch[maxChannels];
        for (int c = 0; c < nch; ++c) ch[c] = buffer.getWritePointer (c);

        const float attCoef = timeToCoeff (p.attackMs, sr);
        const float relCoef = timeToCoeff (p.releaseMs, sr);
        const float ratio   = std::max (1.0f, p.ratio);
        const float boostAmt = juce::jlimit (0.0f, 1.0f, (p.protection - 0.75f) * 4.0f);

        setTargets (p);

        for (int i = 0; i < numSamples; ++i)
        {
            const float inG  = inGain.getNextValue();
            const float outG = outGain.getNextValue();
            const float mkG  = makeup.getNextValue();
            const float mx   = mixV.getNextValue();
            const float byp  = bypassV.getNextValue();

            float dry[maxChannels], x[maxChannels];
            float peak = 0.0f;
            for (int c = 0; c < nch; ++c)
            {
                dry[c] = ch[c][i];
                x[c]   = dry[c] * inG;
                peak   = std::max (peak, std::abs (x[c]));
            }
            if (! std::isfinite (peak)) peak = 0.0f;

            // --- 1) Detección de transiente (stereo-linked)
            const float t = detector.process (peak);

            // --- 2) Detector del compresor: el transiente "carga" menos el
            //        compresor, así no hay un lurch justo después del golpe.
            const float lvlDb = linToDb (peak) - p.protection * t * 6.0f;

            // --- 3) Gain computer + ballistics (dominio dB, feed-forward)
            const float target = computeGainReductionDb (lvlDb, p.thresholdDb, ratio, p.kneeDb);
            grState = target < grState ? attCoef * grState + (1.0f - attCoef) * target
                                       : relCoef * grState + (1.0f - relCoef) * target;

            // --- 4) Separación en dominio de ganancia
            const float bodyGr    = grState * p.bodyAmount;                 // <= 0
            const float appliedGr = bodyGr * (1.0f - p.protection * t);     // protegido
            const float boostDb   = boostAmt * t * 2.0f;                    // realce hasta +2 dB

            const float g = dbToGain (appliedGr + boostDb) * mkG;

            // --- 5) Mix paralelo, salida y bypass
            for (int c = 0; c < nch; ++c)
            {
                const float wet   = x[c] * g;
                const float mixed = (x[c] * (1.0f - mx) + wet * mx) * outG;
                ch[c][i] = mixed * (1.0f - byp) + dry[c] * byp;
            }

            m.bodyGrDb    = std::max (m.bodyGrDb,    -bodyGr);
            m.appliedGrDb = std::max (m.appliedGrDb, -appliedGr);
            m.transient   = std::max (m.transient,   t);
        }
        return m;
    }

private:
    void setTargets (const Params& p)
    {
        const float a = dbToGain (p.inputDb), b = dbToGain (p.outputDb),
                    c = dbToGain (p.makeupDb), d = p.mix, e = p.bypass ? 1.0f : 0.0f;
        if (snap)
        {
            inGain.setCurrentAndTargetValue (a);  outGain.setCurrentAndTargetValue (b);
            makeup.setCurrentAndTargetValue (c);  mixV.setCurrentAndTargetValue (d);
            bypassV.setCurrentAndTargetValue (e);
            snap = false;
        }
        else
        {
            inGain.setTargetValue (a);  outGain.setTargetValue (b);
            makeup.setTargetValue (c);  mixV.setTargetValue (d);
            bypassV.setTargetValue (e);
        }
    }

    double sr = 44100.0;
    bool snap = true;
    float grState = 0.0f;
    TransientDetector detector;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGain, outGain, makeup, mixV, bypassV;
};
} // namespace tl
