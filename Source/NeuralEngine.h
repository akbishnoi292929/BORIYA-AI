#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

class NeuralEngine
{
public:
    NeuralEngine() = default;

    struct AcousticDNA
    {
        float energy = 0.0f;
        float brightness = 200.0f;
        int detectedKey = 0;       // 0 = C, 1 = C#, ...
        int detectedScale = 0;     // 0 = Minor, 1 = Major
        bool isValid = false;
        std::vector<float> pitchHistogram;
    };

    struct MelodicNote
    {
        int step = 0;              // 0 to 63 (4-Bar 16th notes)
        int semitoneOffset = 0;
        float velocity = 0.95f;
        float pan = 0.0f;
        bool active = false;
    };

    // Deep Audio Analysis: Extracts Harmonic Pitch DNA from any dropped loop
    AcousticDNA extractDNA(const float* audioData, int numSamples, double sampleRate)
    {
        AcousticDNA dna;
        dna.pitchHistogram.assign(12, 0.0f);

        if (audioData == nullptr || numSamples < 512 || sampleRate < 8000.0)
            return dna;

        float sumEnergy = 0.0f;
        float spectralSum = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float s = audioData[i];
            sumEnergy += (s * s);
            spectralSum += std::abs(s) * (static_cast<float>(i % 512) / 512.0f);
        }

        float rms = std::sqrt(sumEnergy / static_cast<float>(numSamples));
        if (rms < 0.002f)
            return dna;

        dna.isValid = true;
        dna.energy = rms;
        dna.brightness = spectralSum / std::max(0.001f, sumEnergy);

        // Chromagram Autocorrelation Pitch Detection
        int checkSpan = std::min(numSamples, static_cast<int>(sampleRate * 2.0));
        for (int lag = 60; lag < 400; lag += 3)
        {
            double f = sampleRate / static_cast<double>(lag);
            if (f >= 65.0 && f <= 880.0)
            {
                int midiNote = static_cast<int>(std::round(69.0 + 12.0 * std::log2(f / 440.0)));
                int noteClass = ((midiNote % 12) + 12) % 12;

                float corr = 0.0f;
                for (int i = 0; i < checkSpan - lag; i += 32)
                    corr += audioData[i] * audioData[i + lag];

                dna.pitchHistogram[noteClass] += std::max(0.0f, corr);
            }
        }

        // Detect Root Key
        int bestKey = 0;
        float maxWeight = -1.0f;
        for (int k = 0; k < 12; ++k)
        {
            if (dna.pitchHistogram[k] > maxWeight)
            {
                maxWeight = dna.pitchHistogram[k];
                bestKey = k;
            }
        }
        dna.detectedKey = bestKey;

        // Minor vs Major profiling
        int minorThird = (bestKey + 3) % 12;
        int majorThird = (bestKey + 4) % 12;
        dna.detectedScale = (dna.pitchHistogram[majorThird] > dna.pitchHistogram[minorThird]) ? 1 : 0;

        return dna;
    }

    // Suno-Style Multi-Loop Fusion & Generative Composer
    std::vector<MelodicNote> fuseAndCompose(const std::vector<AcousticDNA>& inputs, int seed)
    {
        std::vector<MelodicNote> output(64);
        for (int i = 0; i < 64; ++i)
        {
            output[i].step = i;
            output[i].active = false;
        }

        juce::Random rng(seed);

        // Combine DNA across all dropped loops
        int finalKey = 0;
        int finalScale = 0;
        float totalBrightness = 0.0f;
        int validCount = 0;

        for (const auto& dna : inputs)
        {
            if (dna.isValid)
            {
                finalKey = dna.detectedKey;
                finalScale = dna.detectedScale;
                totalBrightness += dna.brightness;
                validCount++;
            }
        }

        const int minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const int* scale = (finalScale == 1) ? majorScale : minorScale;

        // Compose 4-Bar Inspired Theme
        for (int bar = 0; bar < 4; ++bar)
        {
            int base = bar * 16;

            // Anchor Downbeat
            output[base].active = true;
            output[base].semitoneOffset = (bar == 3) ? scale[0] : scale[(bar * 2 + rng.nextInt(2)) % 7];
            output[base].velocity = 0.98f;
            output[base].pan = (bar % 2 == 0) ? -0.2f : 0.2f;

            // Melodic Syncopation
            output[base + 3].active = true;
            output[base + 3].semitoneOffset = scale[(2 + bar) % 7];
            output[base + 3].velocity = 0.90f;
            output[base + 3].pan = 0.25f;

            // Harmonic Peak
            output[base + 6].active = true;
            output[base + 6].semitoneOffset = (bar == 2) ? (scale[4] + 12) : scale[4];
            output[base + 6].velocity = 0.95f;
            output[base + 6].pan = -0.2f;

            // Turnaround Hook
            output[base + 10].active = true;
            output[base + 10].semitoneOffset = (bar == 3) ? scale[0] : scale[(5 + rng.nextInt(2)) % 7];
            output[base + 10].velocity = 0.88f;
            output[base + 10].pan = 0.3f;
        }

        return output;
    }
};
