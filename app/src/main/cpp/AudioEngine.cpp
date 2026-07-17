#include "AudioEngine.h"
#include <cmath>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "GuitarFX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

bool AudioEngine::openStreams() {
    // ---- SAÍDA ----
    oboe::AudioStreamBuilder outBuilder;
    outBuilder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(oboe::ChannelCount::Mono)
            ->setUsage(oboe::Usage::Game)
            ->setDataCallback(this)
            ->setErrorCallback(this);
    if (outBuilder.openStream(mOutputStream) != oboe::Result::OK) return false;

    mSampleRate = mOutputStream->getSampleRate();
    int32_t burst = mOutputStream->getFramesPerBurst();
    mOutputStream->setBufferSizeInFrames(burst * 2);

    // ---- ENTRADA ----
    // Preset "Unprocessed" pede ao Android o sinal CRU, sem AEC/NS/AGC,
    // e em muitos aparelhos favorece o mic do headset com fio (o iRig).
    // Fallback para VoiceRecognition se Unprocessed não abrir.
    oboe::AudioStreamBuilder inBuilder;
    inBuilder.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(oboe::ChannelCount::Mono)
            ->setSampleRate(mSampleRate)
            ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
            ->setInputPreset(oboe::InputPreset::Unprocessed);

    oboe::Result r = inBuilder.openStream(mInputStream);
    if (r != oboe::Result::OK) {
        // Tenta de novo com VoiceRecognition
        inBuilder.setInputPreset(oboe::InputPreset::VoiceRecognition);
        r = inBuilder.openStream(mInputStream);
        if (r != oboe::Result::OK) {
            mOutputStream->close();
            return false;
        }
    }

    mInputBuffer.resize((size_t) mOutputStream->getBufferCapacityInFrames());
    std::fill(mDelayLine.begin(), mDelayLine.end(), 0.0f);
    mDelayPos = 0;
    mLfoPhase = 0.0f;
    LOGI("Streams: %d Hz, burst %d", mSampleRate, burst);
    return true;
}

void AudioEngine::closeStreams() {
    if (mOutputStream) { mOutputStream->stop(); mOutputStream->close(); mOutputStream.reset(); }
    if (mInputStream)  { mInputStream->stop();  mInputStream->close();  mInputStream.reset();  }
}

bool AudioEngine::start() {
    if (mRunning) return true;
    if (!openStreams()) return false;
    mInputStream->requestStart();
    mOutputStream->requestStart();
    mRunning = true;
    return true;
}

void AudioEngine::stop() {
    if (isRecording) stopRecording();
    mRunning = false;
    closeStreams();
}

double AudioEngine::getLatencyMs() {
    double total = 0;
    if (mOutputStream) { auto l = mOutputStream->calculateLatencyMillis(); if (l) total += l.value(); }
    if (mInputStream)  { auto l = mInputStream->calculateLatencyMillis();  if (l) total += l.value(); }
    return total;
}

// ---------------- GRAVAÇÃO ----------------
void AudioEngine::writeWavHeader(uint32_t dataBytes) {
    uint32_t sr = (uint32_t) mSampleRate;
    uint32_t byteRate = sr * 2;
    uint16_t blockAlign = 2, bitsPerSample = 16, channels = 1, fmt = 1;
    uint32_t riffSize = 36 + dataBytes, fmtSize = 16;
    fseek(mFile, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, mFile); fwrite(&riffSize, 4, 1, mFile);
    fwrite("WAVE", 1, 4, mFile); fwrite("fmt ", 1, 4, mFile);
    fwrite(&fmtSize, 4, 1, mFile); fwrite(&fmt, 2, 1, mFile);
    fwrite(&channels, 2, 1, mFile); fwrite(&sr, 4, 1, mFile);
    fwrite(&byteRate, 4, 1, mFile); fwrite(&blockAlign, 2, 1, mFile);
    fwrite(&bitsPerSample, 2, 1, mFile);
    fwrite("data", 1, 4, mFile); fwrite(&dataBytes, 4, 1, mFile);
}

bool AudioEngine::startRecording(const char *path) {
    if (isRecording || !mRunning) return false;
    mFile = fopen(path, "wb");
    if (!mFile) return false;
    writeWavHeader(0);
    mDataBytes = 0;
    mRingWrite.store(0); mRingRead.store(0);
    mWriterRun = true;
    mWriterThread = std::thread(&AudioEngine::writerLoop, this);
    isRecording = true;
    return true;
}

void AudioEngine::stopRecording() {
    if (!isRecording) return;
    isRecording = false;
    mWriterRun = false;
    if (mWriterThread.joinable()) mWriterThread.join();
    writeWavHeader(mDataBytes);
    fclose(mFile);
    mFile = nullptr;
}

void AudioEngine::writerLoop() {
    int16_t chunk[4096];
    for (;;) {
        size_t rd = mRingRead.load(std::memory_order_acquire);
        size_t wr = mRingWrite.load(std::memory_order_acquire);
        size_t avail = wr - rd;
        if (avail == 0) {
            if (!mWriterRun) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        size_t n = avail < 4096 ? avail : 4096;
        for (size_t i = 0; i < n; ++i) {
            float s = mRing[(rd + i) & (kRingSize - 1)];
            if (s > 1.0f) s = 1.0f; if (s < -1.0f) s = -1.0f;
            chunk[i] = (int16_t) lrintf(s * 32767.0f);
        }
        fwrite(chunk, sizeof(int16_t), n, mFile);
        mDataBytes += (uint32_t)(n * sizeof(int16_t));
        mRingRead.store(rd + n, std::memory_order_release);
    }
}

// ---------------- CALLBACK ----------------
oboe::DataCallbackResult AudioEngine::onAudioReady(
        oboe::AudioStream *, void *audioData, int32_t numFrames) {

    auto *out = static_cast<float *>(audioData);
    auto result = mInputStream->read(mInputBuffer.data(), numFrames, 0);
    int32_t framesRead = result ? result.value() : 0;

    const int   md  = mode.load(std::memory_order_relaxed);
    const float ig  = inputGain.load(std::memory_order_relaxed);
    const float vol = volume.load(std::memory_order_relaxed);
    const float drv = drive.load(std::memory_order_relaxed);
    const float tn  = tone.load(std::memory_order_relaxed);
    const float fxd = fx.load(std::memory_order_relaxed);
    const bool  rec = isRecording.load(std::memory_order_relaxed);

    // Pré-amp de entrada mais forte: 60 no slider já dá bastante nível.
    // range ~0.5x a ~40x
    const float preAmp = 0.5f + ig * ig * 40.0f;

    const float outGain = powf(vol, 1.5f) * 2.6f; // saída mais alta (limpo/chorus)
    const float k = 1.0f + drv * 24.0f;
    const float kNorm = 1.0f / tanhf(k);
    const float comp = 0.9f / (1.0f + drv * 0.6f);
    const float cutoff = 800.0f + tn * tn * 5200.0f;
    const float alpha = 1.0f - expf(-2.0f * (float) M_PI * cutoff / (float) mSampleRate);

    // High-pass ~120 Hz para limpar os graves antes da distorção valvulada
    const float hpAlpha = 1.0f - expf(-2.0f * (float) M_PI * 120.0f / (float) mSampleRate);

    // Modulação (delay + chorus): LFO varia o tempo de atraso
    const float lfoRate = 0.8f;                       // Hz
    const float lfoInc = 2.0f * (float) M_PI * lfoRate / (float) mSampleRate;
    const int   baseDelay = (int)(0.018f * mSampleRate); // ~18 ms
    const float depthSamp = 0.012f * mSampleRate * fxd;  // varredura do chorus
    const float feedback = 0.35f * fxd;               // repetições (delay)
    const float mix = 0.5f * fxd;                     // seco/molhado

    float peak = 0.0f;
    float lp = mLowpassState;
    size_t ringWr = mRingWrite.load(std::memory_order_relaxed);
    const size_t ringRd = mRingRead.load(std::memory_order_acquire);

    for (int32_t i = 0; i < numFrames; ++i) {
        float raw = (i < framesRead) ? mInputBuffer[(size_t) i] : 0.0f;
        float x = raw * preAmp; // pré-amplifica a entrada fraca do iRig

        float a = fabsf(x);
        if (a > peak) peak = a;

        float y;
        if (md == 0) {
            // LIMPO (agora com pré-amp, então realmente soa)
            y = x;
        } else if (md == 1) {
            // OVERDRIVE VALVULADO
            // 1) Corta graves ANTES de distorcer (evita "lama"): high-pass ~120 Hz
            mHpState += hpAlpha * (x - mHpState);
            float hp = x - mHpState;

            // 2) Empurra o sinal e satura de forma ASSIMÉTRICA (calor de válvula)
            float pre = hp * (2.5f + drv * 8.0f);
            float shaped;
            if (pre >= 0.0f) {
                shaped = tanhf(pre);                 // parte de cima: mais suave
            } else {
                shaped = tanhf(pre * 1.35f) * 0.85f; // parte de baixo: corta diferente
            }
            // toque de 2ª harmônica (o "brilho" valvulado)
            shaped += 0.12f * shaped * shaped;

            // 3) Filtro pós-distorção tira a aspereza digital (passa-baixas do Tom)
            lp += alpha * (shaped - lp);

            // 4) Compressão suave e recuperação de volume
            y = lp * (0.7f + drv * 0.3f) * 1.5f;
        } else {
            // MODULAÇÃO: delay com LFO (dá chorus em fx baixo, delay em fx alto)
            mLfoPhase += lfoInc;
            if (mLfoPhase > 2.0f * (float) M_PI) mLfoPhase -= 2.0f * (float) M_PI;
            float mod = (sinf(mLfoPhase) * 0.5f + 0.5f) * depthSamp;
            float dTime = baseDelay + mod + baseDelay * 3.0f * fxd;
            int di = (int) dTime;
            float frac = dTime - di;
            int r0 = (mDelayPos - di + kDelayMax) % kDelayMax;
            int r1 = (r0 - 1 + kDelayMax) % kDelayMax;
            float delayed = mDelayLine[r0] * (1.0f - frac) + mDelayLine[r1] * frac;
            mDelayLine[mDelayPos] = x + delayed * feedback;
            mDelayPos = (mDelayPos + 1) % kDelayMax;
            y = x * (1.0f - mix) + delayed * mix;
        }

        float sampleOut = y * outGain;
        // proteção contra clipping bruto
        if (sampleOut > 1.2f) sampleOut = 1.2f;
        if (sampleOut < -1.2f) sampleOut = -1.2f;
        out[i] = sampleOut;

        if (rec && (ringWr - ringRd) < kRingSize) {
            mRing[ringWr & (kRingSize - 1)] = sampleOut;
            ++ringWr;
        }
    }

    if (rec) mRingWrite.store(ringWr, std::memory_order_release);
    mLowpassState = lp;
    inputPeak.store(peak, std::memory_order_relaxed);
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream *, oboe::Result) {
    if (mRunning) {
        closeStreams();
        if (openStreams()) {
            mInputStream->requestStart();
            mOutputStream->requestStart();
        } else mRunning = false;
    }
}
