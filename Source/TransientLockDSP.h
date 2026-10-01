#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

// =============================================================================
//  TransientLock DSP
//
//  Separacion Transient / Sustain en el DOMINIO DE GANANCIA (sin crossovers):
//    - Detector diferencial (envolvente rapida vs lenta) -> t (0..1)
//    - Compresor feed-forward en dB con soft-knee
//    - Reduccion aplicada = GR * BodyAmount * (1 - Protection * t)
//  Latencia 0 y fase intacta. Con oversampling la latencia la aporta el
//  oversampler (se reporta al DAW desde el procesador).
// =============================================================================

namespace tl
{
constexpr int maxChannels = 16;

inline float linToDb (float x) noexcept   { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); }

inline float timeToCoeff (float ms, double sampleRate) noexcept
{
    ms = std::max (ms, 0.01f);
    return (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
}

//------------------------------------------------------------------------------
struct Params
{
    float inputDb        = 0.0f;
    float thresholdDb    = -18.0f;
    float ratio          = 4.0f;
    float attackMs       = 10.0f;
    float releaseMs      = 150.0f;
    float kneeDb         = 6.0f;
    float makeupDb       = 0.0f;
    float mix            = 1.0f;    // 0..1
    float outputDb       = 0.0f;
    float protection     = 0.6f;    // 0..1  Transient Protection
    float bodyAmount     = 1.0f;    // 0..1  Body Compression
    float sidechainHpfHz = 20.0f;   // <= 21 Hz = desactivado
    float sensitivity    = 0.5f;    // 0..1  sensibilidad del detector
    bool  bypass         = false;
};

struct Meters
{
    float bodyGrDb    = 0.0f;
    float appliedGrDb = 0.0f;
    float transient   = 0.0f;

    void merge (const Meters& o) noexcept
    {
        bodyGrDb    = std::max (bodyGrDb,    o.bodyGrDb);
        appliedGrDb = std::max (appliedGrDb, o.appliedGrDb);
        transient   = std::max (transient,   o.transient);
    }
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

    float process (float level, float loDb, float hiDb) noexcept
    {
        fast = level > fast ? fastA * fast + (1.0f - fastA) * level
                            : fastR * fast + (1.0f - fastR) * level;
        slow = level > slow ? slowA * slow + (1.0f - slowA) * level
                            : slowR * slow + (1.0f - slowR) * level;

        float raw = 0.0f;
        if (fast > 1.0e-4f)                                   // gate ~ -80 dBFS
        {
            const float diffDb = 8.685889f * std::log ((fast + 1.0e-9f) / (slow + 1.0e-9f));
            const float x = juce::jlimit (0.0f, 1.0f, (diffDb - loDb) / (hiDb - loDb));
            raw = x * x * (3.0f - 2.0f * x);
        }

        t = raw > t ? outA * t + (1.0f - outA) * raw
                    : outR * t + (1.0f - outR) * raw;
        return t;
    }

private:
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0, outA = 0, outR = 0;
    float fast = 0, slow = 0, t = 0;
};

//------------------------------------------------------------------------------
// Biquad pasa-altos Butterworth 2o orden (RBJ), forma TDF-II
class HighPass
{
public:
    void setup (double sr, float freq) noexcept
    {
        const double w0    = 2.0 * juce::MathConstants<double>::pi * (double) freq / sr;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * 0.70710678118);
        const double a0    = 1.0 + alpha;
        b0 = ((1.0 + cosw) * 0.5) / a0;
        b1 = -(1.0 + cosw) / a0;
        b2 = b0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    float process (float x) noexcept
    {
        const double y = b0 * (double) x + z1;
        z1 = b1 * (double) x - a1 * y + z2;
        z2 = b2 * (double) x - a2 * y;
        return (float) y;
    }

private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

//------------------------------------------------------------------------------
inline float computeGainReductionDb (float xDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float slope = 1.0f / ratio - 1.0f;
    const float over  = xDb - thresholdDb;

    if (kneeDb > 0.0f)
    {
        if (2.0f * over < -kneeDb)               return 0.0f;
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
        for (auto& f : hpf) f.reset();
        hpfWasOn = false;
        lastHpf  = -1.0f;
        grState  = 0.0f;
    }

    // data: punteros por canal (modificados in-place)
    Meters process (float* const* ch, int nch, int numSamples, const Params& p) noexcept
    {
        Meters m;
        nch = std::min (nch, maxChannels);
        if (nch <= 0 || numSamples <= 0) return m;

        const float attCoef  = timeToCoeff (p.attackMs, sr);
        const float relCoef  = timeToCoeff (p.releaseMs, sr);
        const float ratio    = std::max (1.0f, p.ratio);
        const float boostAmt = juce::jlimit (0.0f, 1.0f, (p.protection - 0.75f) * 4.0f);
        const float loDb     = 4.0f - 3.0f * juce::jlimit (0.0f, 1.0f, p.sensitivity);
        const float hiDb     = loDb + 6.5f;

        const bool hpfOn = p.sidechainHpfHz > 21.0f;
        updateHpf (p.sidechainHpfHz, hpfOn);

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
                const float d = hpfOn ? hpf[c].process (x[c]) : x[c];
                peak = std::max (peak, std::abs (d));
            }
            if (! std::isfinite (peak)) peak = 0.0f;

            // 1) transiente
            const float t = detector.process (peak, loDb, hiDb);

            // 2) nivel para el compresor (el transiente "carga" menos)
            const float lvlDb = linToDb (peak) - p.protection * t * 6.0f;

            // 3) gain computer + ballistics
            const float target = computeGainReductionDb (lvlDb, p.thresholdDb, ratio, p.kneeDb);
            grState = target < grState ? attCoef * grState + (1.0f - attCoef) * target
                                       : relCoef * grState + (1.0f - relCoef) * target;

            // 4) separacion en dominio de ganancia
            const float bodyGr    = grState * p.bodyAmount;
            const float appliedGr = bodyGr * (1.0f - p.protection * t);
            const float boostDb   = boostAmt * t * 2.0f;
            const float g = dbToGain (appliedGr + boostDb) * mkG;

            // 5) mix, salida, bypass
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
    void updateHpf (float freq, bool on)
    {
        if (on)
        {
            if (! hpfWasOn)
                for (auto& f : hpf) f.reset();

            if (! hpfWasOn || std::abs (freq - lastHpf) > 0.01f)
            {
                const float f = std::min (freq, (float) (0.45 * sr));
                for (auto& h : hpf) h.setup (sr, f);
                lastHpf = freq;
            }
        }
        hpfWasOn = on;
    }

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
    bool snap = true, hpfWasOn = false;
    float grState = 0.0f, lastHpf = -1.0f;
    TransientDetector detector;
    HighPass hpf[maxChannels];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGain, outGain, makeup, mixV, bypassV;
};
} // namespace tl
