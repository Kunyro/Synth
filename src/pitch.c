#include "synth/pitch.h"

#include <math.h>

// converts an equal-tempered musical note number to cycles per second (hz)
float synth_note_to_frequency(int note)
{
    // note 69 is a4 at 440 hz each 12-note step is an octave, which doubles frequency
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}
