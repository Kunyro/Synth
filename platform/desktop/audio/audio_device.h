#ifndef DESKTOP_AUDIO_DEVICE_H
#define DESKTOP_AUDIO_DEVICE_H

typedef void (*audio_device_render_fn)(
    void *user_data,
    float *output,
    unsigned int frame_count,
    unsigned int channel_count);

#endif
