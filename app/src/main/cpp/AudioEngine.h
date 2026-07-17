#ifndef GUITARFX_AUDIOENGINE_H
#define GUITARFX_AUDIOENGINE_H

#include <oboe/Oboe.h>
#include <atomic>
#include <vector>
#include <thread>
#include <cstdio>

class AudioEngine : public oboe::AudioStreamDataCallback,
                    public oboe::AudioStreamErrorCallback {
public:
    bool start();
    void stop();

    // Parâmetros (0.0 a 1.0)
    std::atomic<float> inputGain{0.5f}; // pré-amp de entrada (NOVO)
    std::atomic<float> volume{0.7f};
    std::atomic<float> drive{0.5f};
    std::atomic<float> tone{0.6f};
    std::atomic<float> fx{0.4f};        // profundidade do delay/chorus (NOVO)
    std::atomic<int>   mode{0};         // 0 limpo, 1 overdrive, 2 modulação
    std::atomic<float> inputPeak{0};

    bool startRecording(const char *path);
    void stopRecording();
    std::atomic<bool> isRecording{false};

    double getLatencyMs();

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *outputStream, void *audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result result) override;

private:
    std::shared_ptr<oboe::AudioStream> mInputStream;
    std::shared_ptr<oboe::AudioStream> mOutputStream;
    std::vector<float> mInputBuffer;
    float mLowpassState = 0.0f;
    float mHpState = 0.0f; // estado do high-pass de graves do overdrive
    int32_t mSampleRate = 48000;
    std::atomic<bool> mRunning{false};

    // Linha de delay para o efeito de modulação (delay + chorus)
    static constexpr int kDelayMax = 96000; // até 2 s a 48 kHz
    std::vector<float> mDelayLine = std::vector<float>(kDelayMax, 0.0f);
    int mDelayPos = 0;
    float mLfoPhase = 0.0f;

    // Ring buffer da gravação
    static constexpr size_t kRingSize = 1u << 19;
    std::vector<float> mRing = std::vector<float>(kRingSize);
    std::atomic<size_t> mRingWrite{0};
    std::atomic<size_t> mRingRead{0};

    FILE *mFile = nullptr;
    std::thread mWriterThread;
    std::atomic<bool> mWriterRun{false};
    uint32_t mDataBytes = 0;

    bool openStreams();
    void closeStreams();
    void writerLoop();
    void writeWavHeader(uint32_t dataBytes);
};

#endif
