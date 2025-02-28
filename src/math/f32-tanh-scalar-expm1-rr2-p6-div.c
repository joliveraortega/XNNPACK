// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>
#include <stddef.h>
#include <math.h>

#include <xnnpack/common.h>
#include <xnnpack/math.h>
#include <xnnpack/math-stubs.h>


void xnn_math_f32_tanh__scalar_expm1_rr2_p6_div(
    size_t n,
    const float* input,
    float* output)
{
  assert(n % sizeof(float) == 0);

  // Large number such that ulp(magic bias) == 0.5 and magic bias === 63.5 mod 2**21.
  const float vmagic_bias = 0x1.8000FEp+22f;
  const float vminus_log2e = -0x1.715476p+0f;
  // Last 4 bits are zeroes
  const float vln2_hi = 0x1.62E420p-1f;
  const float vln2_lo = 0x1.FDF474p-22f;
  // Coefficient of polynomial approximation
  //   exp(-2t) - 1 ~ t * (-2 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))))
  // on [-log(2)/4, log(2)/4]
  const float vc6 = 0x1.6b7338p-4f;
  const float vc5 = -0x1.12278Ep-2f;
  const float vc4 = 0x1.555716p-1f;
  const float vc3 = -0x1.5554B0p+0f;
  const float vc2 = 0x1.FFFFFEp+0f;
  const float vminus_two = -2.0f;
  const float vone = 1.0f;
  // The largest z for which tanhf(-z) is not saturated at -1.0f.
  const float vsat_cutoff = 0x1.205966p+3f;

  for (; n != 0; n -= sizeof(float)) {
    const float vx = *input++;

    // General structure of the algorithm:
    //
    //           / expm1(2x) / (2 + expm1(2x)) if x <= 0
    //   f[x] :=
    //           \ -f[-x] if x >= 0
    //
    // First we compute f[-z] := expm1(-2z) / (2 + expm1(-2z)) where z = abs(x),
    // then replace result with -f[-z] if x >= 0.
    const float vz = fabsf(vx);

    // Compute reduced argument n := round(-z / log(2), 1).
    // We do it by adding a large number (magic bias), which cause rounding of the result to integer, then subtracing
    // the large number back. The trick with adding large number is valid only within certain bounds
    // (|-z / log(2)| <= 2**21, i.e. |z| <= 0x1.62E43p+20 = 1453635.0), but that is acceptable, because inputs x
    // outside of [-9.010913, 9.010913] (i.e. z outsize [0, 9.010913]) saturate tanhf(x). We fixup the result for such
    // inputs at the very end of the algorithm.
    // Note that addition-subtraction of the large number doesn't cause overflow for inputs in this range.
    float vn = vz * vminus_log2e + vmagic_bias;

    // Create a floating-point number s (scale) such that s == 2**(2n) for inputs which don't cause underflow, i.e.
    // 0 <= z <= 9.010913, and -13 <= n <= 0 accordingly.
    const float vs = uint32_as_float(float_as_uint32(vn) << 23);

    // Subtract the large number back to get final n := round(-z / log(2), 1) as a floating-point number.
    vn -= vmagic_bias;

    // Compute reduced argument t := z + n * log(2). Note that -t = -z - n * log(2).
    // Use Cody-Waite range reduction method (note two constants to represent log(2)) to improve accuracy.
    float vt = vn * vln2_hi + vz;
    vt = vn * vln2_lo + vt;

    // Compute degree-6 polynomial approximation for exp(2t) - 1 on [-log(2)/4, log(2)/4].
    //   P(-2t) = t * (-2 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))))
    //          = t * p
    float vp = vc6 * vt + vc5;
    vp = vp * vt + vc4;
    vp = vp * vt + vc3;
    vp = vp * vt + vc2;
    vp = vp * vt + vminus_two;

    // Reconstruct the exp(x) - 1 value:
    //   exp(x) - 1 = s * (-2 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))) - 1
    //              = (s - 1) + s * t * p
    //              = (s - 1) + (t * s) * p
    const float vts = vt * vs;
    const float vsm1 = vs - vone;
    const float vem1 = vp * vts + vsm1;

    // Reconstruct tanh(-z) := expm1(-2z) / (2 + expm1(-2z))
    const float vep1 = vem1 - vminus_two;
    float vabsy = vem1 / vep1;

    // The function saturates at +-1 for large inputs: tanhf(z) == +-1.0f for z > sat_cutoff ~= 9.010913.
    // Note that we use 1.0f, because sign will be copied from the input right after.
    if XNN_UNPREDICTABLE(vz > vsat_cutoff) {
      vabsy = vone;
    }

    // Reconstruct tanh[x] = sign(x) * tanh[-abs(x)]
    const float vy = copysignf(vabsy, vx);

    *output++ = vy;
  }
}
