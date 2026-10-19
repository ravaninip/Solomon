#include "Resampler.h"

namespace solomon
{
namespace
{
constexpr int kTableRes = 512;
constexpr double kPi = 3.14159265358979323846;

double blackmanHarris (double x)   // x in [-1, 1]
{
    if (x <= -1.0 || x >= 1.0) return 0.0;
    const double t = kPi * (x + 1.0);                       // 0..2pi
    return 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
}
} // namespace

void Resampler::prepare (double inRate, double outRate)
{
    passthrough = std::abs (inRate - outRate) < 1.0e-6 || inRate <= 0.0 || outRate <= 0.0;
    step = passthrough ? 1.0 : inRate / outRate;
    // keep a small guard band below the output Nyquist when down-sampling
    cutoff = passthrough ? 1.0f : (float) std::min (1.0, 0.96 * outRate / inRate);
    taps = (int) std::ceil (kHalfWidth / cutoff);

    table.assign ((size_t) (kHalfWidth * kTableRes + 2), 0.0f);
    for (size_t i = 0; i < table.size(); ++i)
    {
        const double u = (double) i / kTableRes;            // distance in cut-off-scaled units
        const double s = u < 1.0e-9 ? 1.0 : std::sin (kPi * u) / (kPi * u);
        table[i] = (float) (s * blackmanHarris (u / kHalfWidth));
    }
    reset();
}

void Resampler::reset()
{
    buf.assign ((size_t) (passthrough ? 0 : taps + 2), 0.0f);   // silence before the first sample
    pos = (double) (passthrough ? 0 : taps + 2);   // first real sample
    readPos = 0;
}

float Resampler::kernel (float d) const
{
    const float u = std::abs (d) * cutoff * (float) kTableRes;
    const int i = (int) u;
    if (i >= (int) table.size() - 1) return 0.0f;
    const float f = u - (float) i;
    return cutoff * (table[(size_t) i] * (1.0f - f) + table[(size_t) i + 1] * f);
}
} // namespace solomon
