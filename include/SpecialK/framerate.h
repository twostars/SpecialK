/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#ifndef __SK__FRAMERATE_H__
#define __SK__FRAMERATE_H__

#include <Windows.h>

#include <cstdint>
#include <cmath>

namespace SK
{
  namespace Framerate
  {
    void Init     (void);
    void Shutdown (void);

    void Tick     (double& dt, LARGE_INTEGER& now);

    class Limiter {
    public:
      Limiter (double target = 60.0);

      ~Limiter (void) {
      }

      void     init (double target);
      void     wait (void);
      bool try_wait (void); // No actual wait, just return
                            //  whether a wait would have occurred.

      void   set_limit (double target);
      double get_limit (void) { return fps; };

      double effective_frametime (void);

    private:
      double        ms, fps, effective_ms;
      LARGE_INTEGER start, last, next, time, freq;
      uint32_t      frames;
    };

    Limiter* GetLimiter (void);

    class Stats {
    public:
      static LARGE_INTEGER freq;

      Stats (void) {
        QueryPerformanceFrequency (&freq);
      }

    #define MAX_SAMPLES 120
      struct sample_t {
        double        val  = 0.0;
        LARGE_INTEGER when = { 0ULL };
      } data [MAX_SAMPLES];
      int    samples       = 0;

      void addSample (double sample, LARGE_INTEGER time) {
        data [samples % MAX_SAMPLES].val  = sample;
        data [samples % MAX_SAMPLES].when = time;

        samples++;
      }

      double calcMean (double seconds = 1.0);

      double calcMean (LARGE_INTEGER start) {
        double mean = 0.0;

        int samples_used = 0;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            ++samples_used;
            mean += data [i].val;
          }
        }

        return mean / (double)samples_used;
      }

      double calcSqStdDev (double mean, double seconds = 1.0);

      double calcSqStdDev (double mean, LARGE_INTEGER start) {
        double sd = 0.0;

        int samples_used = 0;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            sd += (data [i].val - mean) *
                  (data [i].val - mean);
            samples_used++;
          }
        }

        return sd / (double)samples_used;
      }

      double calcMin (double seconds = 1.0);

      double calcMin (LARGE_INTEGER start) {
        double min = INFINITY;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            if (data [i].val < min)
              min = data [i].val;
          }
        }

        return min;
      }

      double calcMax (double seconds = 1.0);

      double calcMax (LARGE_INTEGER start) {
        double max = -INFINITY;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            if (data [i].val > max)
              max = data [i].val;
          }
        }

        return max;
      }

      int calcHitches (double tolerance, double mean, double seconds = 1.0);

      int calcHitches (double tolerance, double mean, LARGE_INTEGER start) {
        int hitches = 0;

    #if 0
        for (int i = 1; i < MAX_SAMPLES; i++) {
          if (data [i    ].when.QuadPart >= start.QuadPart &&
              data [i - 1].when.QuadPart >= start.QuadPart) {
            if ((data [i].val + data [i - 1].val) / 2.0 > (tolerance * data [i - 1].val) ||
                (data [i].val + data [i - 1].val) / 2.0 > (tolerance * data [i].val))
              hitches++;
          }
        }

        // Handle wrap-around on the final sample
        if (data [0              ].when.QuadPart >= start.QuadPart &&
            data [MAX_SAMPLES - 1].when.QuadPart >= start.QuadPart &&
            data [0].when.QuadPart > data [MAX_SAMPLES -1].when.QuadPart) {
          if ((data [MAX_SAMPLES - 1].val - data [0].val) > (tolerance * data [MAX_SAMPLES - 1].val))
            hitches++;
        }
    #else
        bool last_late = false;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            if (data [i].val > tolerance * mean) {
              if (! last_late)
                hitches++;
              last_late = true;
            } else {
              last_late = false;
            }
          }
        }
    #endif

        return hitches;
      }

      int calcNumSamples (double seconds = 1.0);

      int calcNumSamples (LARGE_INTEGER start) {
        int samples_used = 0;

        for (int i = 0; i < MAX_SAMPLES; i++) {
          if (data [i].when.QuadPart >= start.QuadPart) {
            samples_used++;
          }
        }

        return samples_used;
      }
    };
  };
};

#endif /* __SK__FRAMERATE_H__ */