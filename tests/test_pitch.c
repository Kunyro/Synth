#include "synth/pitch.h"
#include <math.h>
#include <stdio.h>

// checks the a4 reference and octave doubling independently of any midi protocol parser
int main(void)
{
    if (fabsf(synth_note_to_frequency(69) - 440.0f) > 0.001f ||
        fabsf(synth_note_to_frequency(57) - 220.0f) > 0.001f) {
        fprintf(stderr, "Pitch conversion failed.\n");
        return 1;
    }
    return 0;
}
