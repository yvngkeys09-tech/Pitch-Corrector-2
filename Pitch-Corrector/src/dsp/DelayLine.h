#pragma once
#include <cstddef>
#include <vector>

/// Fixed integer delay. prepare() allocates; process() never does.
class DelayLine {
public:
    void prepare(int delaySamples)
    {
        delay = delaySamples;
        std::size_t sz = 1;
        while (sz < static_cast<std::size_t>(delaySamples) + 1) sz <<= 1;
        buf.assign(sz, 0.f);
        mask = sz - 1;
        pos = 0;
    }

    float process(float x) noexcept
    {
        buf[pos] = x;
        const float y = buf[(pos - static_cast<std::size_t>(delay)) & mask];
        pos = (pos + 1) & mask;
        return y;
    }

private:
    std::vector<float> buf;
    std::size_t mask = 0, pos = 0;
    int delay = 0;
};
