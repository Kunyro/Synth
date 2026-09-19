#ifndef SYNTH_DSP_HISTORY_H
#define SYNTH_DSP_HISTORY_H
#include <math.h>
#include <stddef.h>
#include <string.h>
// history checks cover whole buffers, so gaps between echoes cannot retire a tail
static inline int synth_history_active(const float *samples, size_t count)
{
    if (samples == NULL) return 0;
    for (size_t i = 0; i < count; ++i)
        if (fabsf(samples[i]) > 1e-7f) return 1;
    return 0;
}
static inline void synth_history_clear(float *samples, size_t count)
{
    if (samples != NULL) memset(samples, 0, count * sizeof(*samples));
}
#endif
