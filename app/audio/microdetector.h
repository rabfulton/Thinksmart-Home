#pragma once
#include <QDir>
#include <memory>
#include <cstddef>
#include <cstdint>

// Streaming INT8 microWakeWord, using the same fixed-point frontend as training.
// TensorFlow Lite's C ABI is loaded at runtime; no Python or GPU is used here.
class MicroDetector {
public:
    MicroDetector(const QDir &directory,float threshold);
    ~MicroDetector();
    bool put(const int16_t *samples,size_t count);
    void reset();
    double probability()const;
    uint64_t inferences()const;
private:
    struct Impl;
    std::unique_ptr<Impl> state;
};
