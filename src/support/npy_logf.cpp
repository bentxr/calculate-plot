// Derived from NumPy 2.5.3: numpy/_core/src/umath/loops_exponent_log.dispatch.c.src (simd_log_FLOAT) and
// numpy/_core/src/umath/npy_simd_data.h (its coefficients).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md

#include "support/npy_logf.hpp"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace calculate_plot::support {

namespace {

std::uint32_t bitsOf(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

float floatOf(std::uint32_t u) {
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

}  // namespace

float npyLogf(float xIn) {
    // the coefficients of log(1 + x) = P(x) / Q(x), as single floats
    const float p0 = 0.000000000000000000000e+00f, p1 = 9.999999999999998702752e-01f, p2 = 2.112677543073053063722e+00f,
                p3 = 1.480000633576506585156e+00f, p4 = 3.808837741388407920751e-01f, p5 = 2.589979117907922693523e-02f;
    const float q0 = 1.000000000000000000000e+00f, q1 = 2.612677543073109236779e+00f, q2 = 2.453006071784736363091e+00f,
                q3 = 9.864942958519418960339e-01f, q4 = 1.546476374983906719538e-01f, q5 = 5.875095403124574342950e-03f;
    const float loge2 = 0.693147180559945309417232121458176568f, sqrt1_2 = 0.707106781186547524400844362104849039f;
    const bool negx = xIn < 0.0f, zero = xIn == 0.0f, inf = xIn == std::numeric_limits<float>::infinity(), nan = xIn != xIn;
    const float x0 = negx ? 0.0f : xIn;
    // the exponent and the mantissa in [0.5, 1), subnormals first multiplied by 2**100
    const bool denormal = x0 < FLT_MIN;
    const float scaled = denormal ? x0 * floatOf(0x71800000u) : x0;
    float exponent = float(std::int32_t(bitsOf(scaled) >> 23) - 0x7E);
    if (denormal) exponent = exponent - 100.0f;
    float x = floatOf((bitsOf(scaled) & 0x7fffffu) | (126u << 23));
    if (x <= sqrt1_2) {  // below sqrt(1/2): twice the mantissa, one less in the exponent
        x = x + x;
        exponent = exponent - 1.0f;
    }
    x = x - 1.0f;
    float num = std::fma(p5, x, p4);
    num = std::fma(num, x, p3);
    num = std::fma(num, x, p2);
    num = std::fma(num, x, p1);
    num = std::fma(num, x, p0);
    float den = std::fma(q5, x, q4);
    den = std::fma(den, x, q3);
    den = std::fma(den, x, q2);
    den = std::fma(den, x, q1);
    den = std::fma(den, x, q0);
    float poly = num / den;
    poly = std::fma(exponent, loge2, poly);
    if (nan) poly = std::numeric_limits<float>::quiet_NaN();
    if (negx) poly = -std::numeric_limits<float>::quiet_NaN();
    if (zero) poly = -std::numeric_limits<float>::infinity();
    if (inf) poly = std::numeric_limits<float>::infinity();
    return poly;
}

}  // namespace calculate_plot::support
