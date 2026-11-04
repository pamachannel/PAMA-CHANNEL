// TOPVOX AI STUDIO - moteur de traitement vocal (C++ standard, sans dependance JUCE)
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace tv
{
constexpr double kPi = 3.14159265358979323846;
inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float x)  { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    static double w (double fs, double f) { return 2.0 * kPi * std::min (f, fs * 0.45) / fs; }
    void makeHighPass (double fs, double f, double q)
    {
        const double w0 = w (fs, f), c = std::cos (w0), al = std::sin (w0) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void makeLowPass (double fs, double f, double q)
    {
        const double w0 = w (fs, f), c = std::cos (w0), al = std::sin (w0) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void makePeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = w (fs, f), c = std::cos (w0), al = std::sin (w0) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void makeLowShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = w (fs, f), c = std::cos (w0);
        const double al = std::sin (w0) / std::sqrt (2.0), t = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void makeHighShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = w (fs, f), c = std::cos (w0);
        const double al = std::sin (w0) / std::sqrt (2.0), t = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
};

struct VoiceSettings
{
    bool autoMix = false, autoEq = false, autoGain = false;
    float inGain = 0, gate = -80, eqLow = 0, eqMid = 0, eqAir = 0, reson = 0, comp = 0, deess = 0, sat = 0;
    float dlyMix = 0, dlyTime = 250, dlyFb = 25, limit = -1, outGain = 0;
};

class VoiceEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; fsf = (float) sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            hpf[c].reset(); hpf[c].makeHighPass (fs, 80.0, 0.7071);
            deHP[c].reset(); deHP[c].makeHighPass (fs, 5500.0, 0.7071);
            resA[c].reset(); resB[c].reset(); eqL[c].reset(); eqM[c].reset(); eqA[c].reset();
            dlyBuf[c].assign ((size_t) (fs * 1.2) + 8, 0.0f);
            dlyLp[c] = 0.0f;
        }
        anLP.reset(); anLP.makeLowPass (fs, 250.0, 0.7071);
        anHP.reset(); anHP.makeHighPass (fs, 5000.0, 0.7071);
        gainLin = 1.0f; autoGainDb = 0.0f; avgPower = 1.0e-4f; avgSeeded = false; noiseFloorDb = -70.0f;
        gateEnv = 0.0f; gateGain = 1.0f; gateOpen = true; compGr = 0.0f; deEnv = 0.0f; limG = 1.0f;
        aLow = aMid = aAir = 0.0f; pT = pL = pH = pTs = pLs = pHs = 0.0;
        dlyW = 0; curDelay = 0.25f * fsf;
    }

    // Chaine : gate -> gain auto -> filtre grave -> resonances -> EQ -> de-esser -> compresseur -> saturation -> delay
    void processChain (float* l, float* r, int n, const VoiceSettings& s, float& inPeakOut)
    {
        inPeakOut = 0.0f;
        if (n <= 0 || dlyBuf[0].empty()) return;
        const int nch = r != nullptr ? 2 : 1;
        float* ch[2] = { l, r };
        const float blockSec = (float) n / fsf;
        auto coefT = [blockSec] (float tau) { return 1.0f - std::exp (-blockSec / tau); };

        // ---- analyse du bloc d'entree
        double sumSq = 0.0; float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < nch; ++c) { m += ch[c][i]; pk = std::max (pk, std::abs (ch[c][i])); }
            m /= (float) nch;
            sumSq += (double) m * m;
        }
        inPeakOut = pk;
        const float blockPower = (float) (sumSq / n);
        const float blockDb = 10.0f * std::log10 (blockPower + 1.0e-12f);

        const bool autoMix = s.autoMix;
        const bool autoEqOn = autoMix || s.autoEq;
        const bool autoGainOn = autoMix || s.autoGain;

        if (blockDb > -55.0f)
        {
            if (! avgSeeded) { avgPower = blockPower; avgSeeded = true; }
            else avgPower += (blockPower - avgPower) * coefT (2.0f);
        }
        const float avgDb = 10.0f * std::log10 (avgPower + 1.0e-12f);
        if (blockDb > -85.0f)
        {
            if (blockDb < noiseFloorDb) noiseFloorDb = blockDb; else noiseFloorDb += 1.0f * blockSec;
        }
        noiseFloorDb = clampf (noiseFloorDb, -85.0f, -45.0f);

        // ---- gain automatique
        const float agTarget = (autoGainOn && avgSeeded) ? clampf (-18.0f - avgDb, -12.0f, 18.0f) : 0.0f;
        autoGainDb += (agTarget - autoGainDb) * coefT (1.0f);
        const float totalGainDb = s.inGain + autoGainDb;
        const float gStep = (dbToLin (totalGainDb) - gainLin) / (float) n;

        // ---- gate
        const bool gateOn = autoMix || s.gate > -79.5f;
        const float gateThrLin = dbToLin (autoMix ? clampf (noiseFloorDb + 8.0f, -70.0f, -38.0f) : s.gate);
        const float gAtk = 1.0f - std::exp (-1.0f / (0.002f * fsf));
        const float gRel = 1.0f - std::exp (-1.0f / (0.120f * fsf));
        const float gEnvRel = std::exp (-1.0f / (0.060f * fsf));
        const float gateFloor = 0.05f;

        // ---- EQ + resonances
        const float rA = clampf (s.reson, 0.0f, 100.0f) / 100.0f;
        const float eqLowDb = clampf (s.eqLow + aLow, -10.0f, 10.0f);
        const float eqMidDb = clampf (s.eqMid + aMid, -10.0f, 10.0f);
        const float eqAirDb = clampf (s.eqAir + aAir, -10.0f, 10.0f);
        for (int c = 0; c < 2; ++c)
        {
            resA[c].makePeak (fs, 300.0, 2.0, -5.0 * rA);
            resB[c].makePeak (fs, 2800.0, 3.0, -4.0 * rA);
            eqL[c].makeLowShelf (fs, 120.0, eqLowDb);
            eqM[c].makePeak (fs, 3000.0, 0.8, eqMidDb);
            eqA[c].makeHighShelf (fs, 9000.0, eqAirDb);
        }

        // ---- de-esser
        const float dAmt = clampf (s.deess, 0.0f, 100.0f) / 100.0f;
        const bool deOn = dAmt > 0.005f;
        const float deThr = -18.0f - 14.0f * dAmt, deRatio = 4.0f + 4.0f * dAmt;
        const float deAtk = 1.0f - std::exp (-1.0f / (0.0003f * fsf));
        const float deRel = 1.0f - std::exp (-1.0f / (0.030f * fsf));

        // ---- compresseur
        const float cAmt = clampf (s.comp, 0.0f, 100.0f) / 100.0f;
        const bool compOn = cAmt > 0.005f;
        const float ratio = 1.5f + 4.5f * cAmt;
        float thrDb = -12.0f - 12.0f * cAmt;
        if (autoMix && avgSeeded) thrDb = clampf (avgDb + totalGainDb - 3.0f - 4.0f * cAmt, -40.0f, -6.0f);
        const float knee = 6.0f;
        const float makeup = dbToLin (6.0f * cAmt * (1.0f - 1.0f / ratio));
        const float cAtk = 1.0f - std::exp (-1.0f / (0.008f * fsf));
        const float cRel = 1.0f - std::exp (-1.0f / (0.120f * fsf));

        // ---- saturation
        const float sAmt = clampf (s.sat, 0.0f, 100.0f) / 100.0f;
        const bool sOn = sAmt > 0.005f;
        const float drive = 1.0f + 6.0f * sAmt;

        // ---- delay
        const float dMix = clampf (s.dlyMix, 0.0f, 100.0f) / 100.0f;
        const bool dOn = dMix > 0.005f;
        const float dWet = dMix * 0.7f;
        const float dFb = clampf (s.dlyFb, 0.0f, 85.0f) / 100.0f;
        const float dTarget = clampf (s.dlyTime, 50.0f, 800.0f) * 0.001f * fsf;
        const int dsize = (int) dlyBuf[0].size();

        for (int i = 0; i < n; ++i)
        {
            float inS[2] = { ch[0][i], nch > 1 ? ch[1][i] : 0.0f };

            float lvl = std::abs (inS[0]);
            if (nch > 1) lvl = std::max (lvl, std::abs (inS[1]));
            gateEnv = std::max (lvl, gateEnv * gEnvRel);
            if (gateOn)
            {
                if (! gateOpen && gateEnv > gateThrLin) gateOpen = true;
                else if (gateOpen && gateEnv < gateThrLin * 0.7f) gateOpen = false;
            }
            const float gT = (! gateOn || gateOpen) ? 1.0f : gateFloor;
            gateGain += (gT - gateGain) * (gT > gateGain ? gAtk : gRel);
            gainLin += gStep;
            const float g = gateGain * gainLin;

            double y[2] = { 0.0, 0.0 };
            for (int c = 0; c < nch; ++c) y[c] = hpf[c].process ((double) inS[c] * g);

            const double mono = nch > 1 ? 0.5 * (y[0] + y[1]) : y[0];
            const double lpv = anLP.process (mono), hpv = anHP.process (mono);
            pT += mono * mono; pL += lpv * lpv; pH += hpv * hpv;

            for (int c = 0; c < nch; ++c)
                y[c] = eqA[c].process (eqM[c].process (eqL[c].process (resB[c].process (resA[c].process (y[c])))));

            if (deOn)
            {
                double hp[2] = { 0.0, 0.0 }; float dl = 0.0f;
                for (int c = 0; c < nch; ++c) { hp[c] = deHP[c].process (y[c]); dl = std::max (dl, (float) std::abs (hp[c])); }
                deEnv += (dl - deEnv) * (dl > deEnv ? deAtk : deRel);
                const float over = linToDb (deEnv) - deThr;
                const float gDb = over > 0.0f ? -std::min (over * (1.0f - 1.0f / deRatio), 12.0f) : 0.0f;
                const double dg = dbToLin (gDb);
                for (int c = 0; c < nch; ++c) y[c] = y[c] - hp[c] + hp[c] * dg;
            }

            float cg = 1.0f;
            if (compOn)
            {
                const float sc = (float) std::max (std::abs (y[0]), nch > 1 ? std::abs (y[1]) : 0.0);
                const float over = linToDb (sc) - thrDb;
                float gr = 0.0f;
                if (2.0f * over > -knee)
                {
                    if (2.0f * over < knee) { const float t = over + knee * 0.5f; gr = (1.0f / ratio - 1.0f) * t * t / (2.0f * knee); }
                    else gr = (1.0f / ratio - 1.0f) * over;
                }
                compGr += (gr - compGr) * (gr < compGr ? cAtk : cRel);
                cg = dbToLin (compGr) * makeup;
            }

            curDelay += (dTarget - curDelay) * 0.0008f;
            for (int c = 0; c < nch; ++c)
            {
                float v = (float) (y[c] * cg);
                if (sOn) v = (1.0f - sAmt) * v + sAmt * std::tanh (drive * v) / drive;
                if (dOn)
                {
                    float rp = (float) dlyW - curDelay;
                    if (rp < 0.0f) rp += (float) dsize;
                    const int i0 = safeIndex ((int) rp, dsize);
                    const int i1 = (i0 + 1 >= dsize) ? 0 : i0 + 1;
                    const float fr = rp - (float) i0;
                    const float tap = dlyBuf[c][(size_t) i0] * (1.0f - fr) + dlyBuf[c][(size_t) i1] * fr;
                    dlyLp[c] += 0.35f * (tap - dlyLp[c]);
                    dlyBuf[c][(size_t) dlyW] = v + dlyLp[c] * dFb;
                    v += tap * dWet;
                }
                else dlyBuf[c][(size_t) dlyW] = v;
                ch[c][i] = v;
            }
            if (++dlyW >= dsize) dlyW = 0;
        }

        // ---- Auto EQ : analyse de l'equilibre spectral (bas / medium / aigu)
        if (autoEqOn && blockDb > -55.0f)
        {
            const double k = coefT (1.5f);
            pTs += (pT / n - pTs) * k; pLs += (pL / n - pLs) * k; pHs += (pH / n - pHs) * k;
        }
        pT = pL = pH = 0.0;
        float tLow = 0.0f, tMid = 0.0f, tAir = 0.0f;
        if (autoEqOn && pTs > 1.0e-9)
        {
            auto dbRatio = [] (double r, double target) { return (float) (10.0 * std::log10 (std::max (r, 1.0e-6) / target)); };
            const double lowR = pLs / pTs, highR = pHs / pTs, midR = std::max (1.0 - lowR - highR, 0.01);
            tLow = clampf (-0.8f * dbRatio (lowR, 0.50), -4.0f, 4.0f);
            tAir = clampf (-0.6f * dbRatio (highR, 0.03), -4.0f, 4.0f);
            tMid = clampf (-0.5f * dbRatio (midR, 0.47), -3.0f, 3.0f);
        }
        const float k2 = coefT (2.0f);
        aLow += (tLow - aLow) * k2; aMid += (tMid - aMid) * k2; aAir += (tAir - aAir) * k2;
    }

    // Sortie : gain de sortie + limiteur
    void processOutput (float* l, float* r, int n, const VoiceSettings& s, float& outPeakOut)
    {
        outPeakOut = 0.0f;
        if (n <= 0) return;
        const int nch = r != nullptr ? 2 : 1;
        float* ch[2] = { l, r };
        const float outLin = dbToLin (s.outGain);
        const float ceilLin = dbToLin (clampf (s.limit, -6.0f, 0.0f));
        const float lAtk = 1.0f - std::exp (-1.0f / (0.0003f * fsf));
        const float lRel = 1.0f - std::exp (-1.0f / (0.060f * fsf));
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float v[2] = { 0.0f, 0.0f }; float p = 0.0f;
            for (int c = 0; c < nch; ++c) { v[c] = ch[c][i] * outLin; p = std::max (p, std::abs (v[c])); }
            const float need = p > ceilLin ? ceilLin / p : 1.0f;
            limG += (need - limG) * (need < limG ? lAtk : lRel);
            for (int c = 0; c < nch; ++c)
            {
                const float o = clampf (v[c] * limG, -ceilLin, ceilLin);
                ch[c][i] = o; pk = std::max (pk, std::abs (o));
            }
        }
        outPeakOut = pk;
    }

private:
    static int safeIndex (int i, int size) { return i < 0 ? 0 : (i >= size ? size - 1 : i); }

    double fs = 44100.0; float fsf = 44100.0f;
    Biquad hpf[2], resA[2], resB[2], eqL[2], eqM[2], eqA[2], deHP[2], anLP, anHP;
    float gainLin = 1.0f, autoGainDb = 0.0f, avgPower = 1.0e-4f, noiseFloorDb = -70.0f;
    bool avgSeeded = false, gateOpen = true;
    float gateEnv = 0.0f, gateGain = 1.0f, compGr = 0.0f, deEnv = 0.0f, limG = 1.0f;
    float aLow = 0.0f, aMid = 0.0f, aAir = 0.0f;
    double pT = 0, pL = 0, pH = 0, pTs = 0, pLs = 0, pHs = 0;
    std::vector<float> dlyBuf[2];
    int dlyW = 0; float curDelay = 0.0f; float dlyLp[2] = { 0.0f, 0.0f };
};
} // namespace tv
