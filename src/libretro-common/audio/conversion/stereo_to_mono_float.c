/* Copyright  (C) 2010-2023 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mono_to_stereo.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdint.h>
#include <stddef.h>

#include <audio/conversion/dual_mono.h>

/* TODO: Use SIMD instructions to make this faster (or show that it's not needed) */
void convert_to_mono_float_left(float *out, const float *in, size_t frames)
{
   unsigned i = 0;

   if (!out || !in || !frames)
      return;

   for (; i < frames; i++)
   {
      out[i] = in[i * 2];
   }
}

/* Why is there no equivalent for int16_t samples?
 * No inherent reason, I just didn't need one.
 * If you do, open a pull request.
 * Same goes for the lack of a convert_to_mono_float_right;
 * I didn't need one, so I didn't write one. */