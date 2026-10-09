#pragma once
#include "roomverb.h"

void impulse_response_to_audio_buffer(AudioSource* speaker);

bool import_audio_buffer(const char* filepath, AudioBuffer* dest);
void output_audio_buffer(const char* filpath, AudioBuffer* buffer);
void free_audio_buffer(AudioBuffer* buffer);

// Applies convolution to an impulse response and an imported audio file
void convolute_buffer(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest);

void mix_audio_buffers(AudioBuffer** buffers, int buffer_count, AudioBuffer* dest);

void clear_audio_buffer(AudioBuffer* buffer);