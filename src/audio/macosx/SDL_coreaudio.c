/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include <AudioUnit/AudioUnit.h>

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_coreaudio.h"


/* Audio driver functions */

static int Core_OpenAudio(_THIS, SDL_AudioSpec * spec);
static void Core_WaitAudio(_THIS);
static void Core_PlayAudio(_THIS);
static Uint8 *Core_GetAudioBuf(_THIS);
static void Core_CloseAudio(_THIS);

/* Audio driver bootstrap functions */

static int
Audio_Available(void)
{
    return (1);
}

static void
Audio_DeleteDevice(SDL_AudioDevice * device)
{
    SDL_free(device->hidden);
    SDL_free(device);
}

static SDL_AudioDevice *
Audio_CreateDevice(int devindex)
{
    SDL_AudioDevice *this;

    /* Initialize all variables that we clean on shutdown */
    this = (SDL_AudioDevice *) SDL_malloc(sizeof(SDL_AudioDevice));
    if (this) {
        SDL_memset(this, 0, (sizeof *this));
        this->hidden = (struct SDL_PrivateAudioData *)
            SDL_malloc((sizeof *this->hidden));
    }
    if ((this == NULL) || (this->hidden == NULL)) {
        SDL_OutOfMemory();
        if (this) {
            SDL_free(this);
        }
        return (0);
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));

    /* Set the function pointers */
    this->OpenAudio = Core_OpenAudio;
    this->WaitAudio = Core_WaitAudio;
    this->PlayAudio = Core_PlayAudio;
    this->GetAudioBuf = Core_GetAudioBuf;
    this->CloseAudio = Core_CloseAudio;

    this->free = Audio_DeleteDevice;

    return this;
}

AudioBootStrap COREAUDIO_bootstrap = {
    "coreaudio", "Mac OS X CoreAudio",
    Audio_Available, Audio_CreateDevice
};

/* The CoreAudio callback */
static OSStatus
audioCallback(void *inRefCon,
              AudioUnitRenderActionFlags inActionFlags,
              const AudioTimeStamp * inTimeStamp,
              UInt32 inBusNumber, AudioBuffer * ioData)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *) inRefCon;
    UInt32 remaining, len;
    void *ptr;

    /* Only do anything if audio is enabled and not paused */
    if (!this->enabled || this->paused) {
        SDL_memset(ioData->mData, this->spec.silence, ioData->mDataByteSize);
        return 0;
    }

    /* No SDL conversion should be needed here, ever, since we accept
       any input format in OpenAudio, and leave the conversion to CoreAudio.
     */
    /*
       assert(!this->convert.needed);
       assert(this->spec.channels == ioData->mNumberChannels);
     */

    remaining = ioData->mDataByteSize;
    ptr = ioData->mData;
    while (remaining > 0) {
        if (bufferOffset >= bufferSize) {
            /* Generate the data */
            SDL_memset(buffer, this->spec.silence, bufferSize);
            SDL_mutexP(this->mixer_lock);
            (*this->spec.callback) (this->spec.userdata, buffer, bufferSize);
            SDL_mutexV(this->mixer_lock);
            bufferOffset = 0;
        }

        len = bufferSize - bufferOffset;
        if (len > remaining)
            len = remaining;
        SDL_memcpy(ptr, (char *) buffer + bufferOffset, len);
        ptr = (char *) ptr + len;
        remaining -= len;
        bufferOffset += len;
    }

    return 0;
}

/* Dummy functions -- we don't use thread-based audio */
void
Core_WaitAudio(_THIS)
{
    return;
}

void
Core_PlayAudio(_THIS)
{
    return;
}

Uint8 *
Core_GetAudioBuf(_THIS)
{
    return (NULL);
}

void
Core_CloseAudio(_THIS)
{
    OSStatus result;
    struct AudioUnitInputCallback callback;

    /* stop processing the audio unit */
    result = AudioOutputUnitStop(outputAudioUnit);
    if (result != noErr) {
        SDL_SetError("Core_CloseAudio: AudioOutputUnitStop");
        return;
    }

    /* Remove the input callback */
    callback.inputProc = 0;
    callback.inputProcRefCon = 0;
    result = AudioUnitSetProperty(outputAudioUnit,
                                  kAudioUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Input,
                                  0, &callback, sizeof(callback));
    if (result != noErr) {
        SDL_SetError
            ("Core_CloseAudio: AudioUnitSetProperty (kAudioUnitProperty_SetInputCallback)");
        return;
    }

    result = CloseComponent(outputAudioUnit);
    if (result != noErr) {
        SDL_SetError("Core_CloseAudio: CloseComponent");
        return;
    }

    SDL_free(buffer);
}

#define CHECK_RESULT(msg) \
    if (result != noErr) { \
        SDL_SetError("Failed to start CoreAudio: " msg); \
        return -1; \
    }


int
Core_OpenAudio(_THIS, SDL_AudioSpec * spec)
{
    OSStatus result = noErr;
    Component comp;
    ComponentDescription desc;
    struct AudioUnitInputCallback callback;
    AudioStreamBasicDescription desc;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(spec->format);
    int valid_datatype = 0;

    /* Setup a AudioStreamBasicDescription with the requested format */
    memset(&desc, '\0', sizeof (AudioStreamBasicDescription));
    desc.mFormatID = kAudioFormatLinearPCM;
    desc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    desc.mChannelsPerFrame = spec->channels;
    desc.mSampleRate = spec->freq;
    desc.mFramesPerPacket = 1;

    while ((!valid_datatype) && (test_format)) {
        spec->format = test_format;
        desc.mFormatFlags = 0;
        /* Just a list of valid SDL formats, so people don't pass junk here. */
        switch (test_format) {
            case AUDIO_U8:
            case AUDIO_S8:
            case AUDIO_U16LSB:
            case AUDIO_S16LSB:
            case AUDIO_U16MSB:
            case AUDIO_S16MSB:
            case AUDIO_S32LSB:
            case AUDIO_S32MSB:
            case AUDIO_F32LSB:
            case AUDIO_F32MSB:
                valid_datatype = 1;
                desc.mBitsPerChannel = SDL_AUDIO_BITSIZE(spec->format);

                if (SDL_AUDIO_ISFLOAT(spec->format))
                    desc.mFormatFlags |= kLinearPCMFormatFlagIsFloat;
                else if (SDL_AUDIO_ISSIGNED(spec->format))
                    desc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;

                if (SDL_AUDIO_ISBIGENDIAN(spec->format))
                    desc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
                break;
        }
    }

    if (!valid_datatype) {  /* shouldn't happen, but just in case... */
        SDL_SetError("Unsupported audio format");
        return (-1);
    }

    desc.mBytesPerFrame =
        desc.mBitsPerChannel * desc.mChannelsPerFrame / 8;
    desc.mBytesPerPacket =
        desc.mBytesPerFrame * desc.mFramesPerPacket;


    /* Locate the default output audio unit */
    desc.componentType = kAudioUnitComponentType;
    desc.componentSubType = kAudioUnitSubType_Output;
    desc.componentManufacturer = kAudioUnitID_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) {
        SDL_SetError
            ("Failed to start CoreAudio: FindNextComponent returned NULL");
        return -1;
    }

    /* Open & initialize the default output audio unit */
    result = OpenAComponent(comp, &outputAudioUnit);
    CHECK_RESULT("OpenAComponent")
        result = AudioUnitInitialize(outputAudioUnit);
    CHECK_RESULT("AudioUnitInitialize")
        /* Set the input format of the audio unit. */
        result = AudioUnitSetProperty(outputAudioUnit,
                                      kAudioUnitProperty_StreamFormat,
                                      kAudioUnitScope_Input,
                                      0,
                                      &desc, sizeof (desc));
    CHECK_RESULT("AudioUnitSetProperty (kAudioUnitProperty_StreamFormat)")
        /* Set the audio callback */
        callback.inputProc = audioCallback;
    callback.inputProcRefCon = this;
    result = AudioUnitSetProperty(outputAudioUnit,
                                  kAudioUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Input,
                                  0, &callback, sizeof(callback));
    CHECK_RESULT("AudioUnitSetProperty (kAudioUnitProperty_SetInputCallback)")
        /* Calculate the final parameters for this audio specification */
        SDL_CalculateAudioSpec(spec);

    /* Allocate a sample buffer */
    bufferOffset = bufferSize = this->spec.size;
    buffer = SDL_malloc(bufferSize);

    /* Finally, start processing of the audio unit */
    result = AudioOutputUnitStart(outputAudioUnit);
    CHECK_RESULT("AudioOutputUnitStart")
        /* We're running! */
        return (1);
}

/* vi: set ts=4 sw=4 expandtab: */
