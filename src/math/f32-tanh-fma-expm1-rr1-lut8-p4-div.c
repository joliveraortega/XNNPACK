// Copyright 2023 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>
#include <stddef.h>
#include <math.h>

#include <xnnpack/common.h>
#include <xnnpack/math.h>
#include <xnnpack/math-stubs.h>


// Table of exp2(k / 8) values decremented (as integer) by (k << 20), k = 0..7
extern XNN_INTERNAL const uint32_t xnn_table_exp2minus_k_over_8[8];

void xnn_math_f32_tanh__fma_expm1_rr1_lut8_p4_div(
    size_t n,
    const float* input,
    float* output)
{
  assert(n % sizeof(float) == 0);

  // Large number such that ulp(magic bias) == exp2(-4)
  const float vmagic_bias = 0x1.800000p19f;
  const float vminus_log2e = -0x1.715476p+0f;
  // Mask for the lowest 3 bits
  const uint32_t vindex_mask = UINT32_C(0x7);
  const float vln2 = 0x1.62E430p-1f;
  // Coefficient of polynomial approximation
  //   exp(-2t) - 1 ~ t * (-2 + t * (c2 + t * (c3 + t * c4)))
  // on [-log(2)/32, log(2)/32]
  const float vc4 = 0x1.5558ECp-1f;
  const float vc3 = -0x1.555C20p+0f;
  const float vc2 = 0x1.000000p+1f;
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

    // Compute reduced argument n := round(-z / log(2), 4).
    // We do it by adding a large number (magic bias), which cause rounding of the result to integer, then subtracing
    // the large number back. The trick with adding large number is valid only within certain bounds
    // (|-z / log(2)| <= 2**18, i.e. |z| <= 0x1.62E43p+17 = 181704.375), but that is acceptable, because inputs x
    // outside of [-9.010913, 9.010913] (i.e. z outsize [0, 9.010913]) saturate tanhf(x). We fixup the result for such
    // inputs at the very end of the algorithm.
    // Note that addition-subtraction of the large number doesn't cause overflow for inputs in this range.
    float vn = fmaf(vz, vminus_log2e, vmagic_bias);

    // Create a floating-point number s (scale) such that s := 2**(2n) for valid inputs, i.e. -17.328680 <= x <= 0.0. As
    // n has 4 fractional bits, we split s == 2**(2n) = 2**int(2n) * 2**frac(2n). We create s in two steps:
    // 1. Fetch 2**frac(2n) from the table using the 3 low bits of n, as integer. Note that the fetched values are in
    //    the [1.0, 2.0) range, i.e. their unbiased floating-point exponent is 0.
    // 2. Adjust fetched value by addition of int(2n) to its floating-point exponent. The result is always a normalized
    //    number, because for 0 <= z <= 9.010913 we have -13 <= int(n) <= 0, and thus the adjusted exponent is not
    //    lower than -13.
    //
    // Shift bits 3:11 into 23:31 (position of floating-point exponent).
    const uint32_t ven = float_as_uint32(vn) << 20;

    // Use bits 0:3 bits of n, as integer, as an index for table lookup of l := 2**frac(n).
    const uint32_t vidx = float_as_uint32(vn) & vindex_mask;
    // Adjust exponent of the value l fetched from the table to get the final s value.
    float vs = uint32_as_float(xnn_table_exp2minus_k_over_8[vidx] + ven);

    // Subtract the large number back to get final n := round(-z / log(2), 4) as a floating-point number.
    vn -= vmagic_bias;

    // Compute reduced argument t := z + n * log(2). Note that -t = -z - n * log(2).
    float vt = fmaf(vn, vln2, vz);

    // Compute degree-4 polynomial approximation for exp(-2t) - 1 on [-log(2)/32, log(2)/32].
    //   P(-2t) = t * (-2 + t * (c2 + t * (c3 + t * c4)))
    //          = t * p
    float vp = fmaf(vc4, vt, vc3);
    vp = fmaf(vp, vt, vc2);
    vp = fmaf(vp, vt, vminus_two);

    // Reconstruct the exp(x) - 1 value:
    //   exp(x) - 1 = s * (-2 + t * (c2 + t * (c3 + t * c4))) - 1
    //              = (s - 1) + s * t * p
    //              = (s - 1) + (t * s) * p
    const float vts = vt * vs;
    const float vsm1 = vs - vone;
    const float vem1 = fmaf(vp, vts, vsm1);

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
