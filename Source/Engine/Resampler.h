#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace solomon
{
// Pull-based band-limited sample-rate converter (windowed sinc, Blackman-Harris).
//
// The synth core runs at a fixed rate; the host asks for output at its own rate. process() pulls
// input in chunks from a generator callback exactly when it is needed. With equal rates the input
// is passed through untouched (no latency, bit exact).
class Resampler
{
public:
    static constexpr int kHalfWidth = 32;    // taps per side at the nominal cutoff

    void prepare (double inRate, double outRate);
    void reset();

    // gen (float* dst, int count) must write `count` new input samples.
    template <class Generator>
    void process (float* out, int n, int chunk, Generator&& gen)
    {
        if (passthrough)
        {
            for (int i = 0; i < n; ++i)
            {
                if (readPos >= (int) buf.size())
                    refill (chunk, gen, 0);
                out[i] = buf[(size_t) readPos++];
            }
            return;
        }

        for (int i = 0; i < n; ++i)
        {
            // refill() may drop consumed samples and shift `pos`, so the centre is re-read each time
            int centre = (int) std::floor (pos);
            while (centre + taps >= (int) buf.size())
            {
                refill (chunk, gen, taps + 2);
                centre = (int) std::floor (pos);
            }

            const double frac = pos - (double) centre;
            float acc = 0.0f, norm = 0.0f;
            for (int k = -taps + 1; k <= taps; ++k)
            {
                const float w = kernel ((float) (k - frac));
                acc += w * buf[(size_t) (centre + k)];
                norm += w;
            }
            out[i] = norm > 1.0e-6f ? acc / norm : 0.0f;
            pos += step;
        }
    }

private:
    template <class Generator>
    void refill (int chunk, Generator& gen, int keepBehind)
    {
        // drop consumed samples, but keep what the kernel can still reach
        const int consumed = passthrough ? readPos : std::max (0, (int) std::floor (pos) - keepBehind);
        if (consumed > 0)
        {
            buf.erase (buf.begin(), buf.begin() + consumed);
            if (passthrough) readPos = 0; else pos -= consumed;
        }
        const size_t old = buf.size();
        buf.resize (old + (size_t) chunk);
        gen (buf.data() + old, chunk);
    }

    float kernel (float d) const;

    bool passthrough = true;
    double step = 1.0;       // input samples per output sample
    float cutoff = 1.0f;     // normalised to the input Nyquist (<= 1)
    int taps = kHalfWidth;   // taps per side actually used (grows when down-sampling)
    double pos = 0.0;        // read position in buf, in input samples
    int readPos = 0;
    std::vector<float> buf;
    std::vector<float> table;   // windowed sinc, kTableRes entries per unit of u = d * cutoff
};
} // namespace solomon
