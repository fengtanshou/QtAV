/******************************************************************************
    AudioOutputOpenAL.cpp: description
    Copyright (C) 2012-2014 Wang Bin <wbsecg1@gmail.com>
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


#include "QtAV/AudioOutput.h"
#include "QtAV/private/AudioOutput_p.h"
#include "prepost.h"
#include <QtCore/QVector>

#if defined(HEADER_OPENAL_PREFIX)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#define UNQUEUE_QUICK 0
#define OLD_AO_API 0

namespace QtAV {

class AudioOutputOpenALPrivate;
class AudioOutputOpenAL : public AudioOutput
{
    DPTR_DECLARE_PRIVATE(AudioOutputOpenAL)
public:
    AudioOutputOpenAL();
    ~AudioOutputOpenAL();

    QString name() const;
    virtual bool open();
    virtual bool close();
    virtual bool isSupported(const AudioFormat& format) const;
    virtual bool isSupported(AudioFormat::SampleFormat sampleFormat) const;
    virtual bool isSupported(AudioFormat::ChannelLayout channelLayout) const;
    virtual AudioFormat::SampleFormat preferredSampleFormat() const;
    virtual AudioFormat::ChannelLayout preferredChannelLayout() const;

    virtual Feature supportedFeatures() const;
#if OLD_AO_API
    virtual void waitForNextBuffer();
#endif
    virtual bool play();
protected:
    virtual bool write(const QByteArray& data);
    virtual int getProcessed();
};

extern AudioOutputId AudioOutputId_OpenAL;
FACTORY_REGISTER_ID_AUTO(AudioOutput, OpenAL, "OpenAL")

void RegisterAudioOutputOpenAL_Man()
{
    FACTORY_REGISTER_ID_MAN(AudioOutput, OpenAL, "OpenAL")
}


#define AL_CHECK_RETURN_VALUE(RET) \
    do { \
        ALenum err = alGetError(); \
        if (err != AL_NO_ERROR) { \
            qWarning("AudioOutputOpenAL Error @%d  (%d) : %s", __LINE__, err, alGetString(err)); \
            return RET; \
        } \
    } while(0)

#define AL_CHECK_RETURN() AL_CHECK_RETURN_VALUE()
#define AL_CHECK() AL_CHECK_RETURN_VALUE(false)

#define AL_RUN_CHECK(FUNC) \
    do { \
        FUNC; \
        ALenum err = alGetError(); \
        if (err != AL_NO_ERROR) { \
            qWarning("AudioOutputOpenAL Error>>> " #FUNC " (%d) : %s", err, alGetString(err)); \
            return false; \
        } \
    } while(0)

// TODO: planar
static ALenum audioFormatToAL(const AudioFormat& fmt)
{
    ALenum format = 0;
    ALCcontext *ctx = alcGetCurrentContext(); //a context is required for al functions!
    if (fmt.sampleFormat() == AudioFormat::SampleFormat_Unsigned8) {
        if (fmt.channels() == 1)
            format = AL_FORMAT_MONO8;
        else if (fmt.channels() == 2)
            format = AL_FORMAT_STEREO8;
        else if (ctx) {
            if (alIsExtensionPresent("AL_EXT_MCFORMATS")) {
                if (fmt.channels() == 4)
                    format = alGetEnumValue("AL_FORMAT_QUAD8");
                else if (fmt.channels() == 6)
                    format = alGetEnumValue("AL_FORMAT_51CHN8");
                else if (fmt.channels() == 7)
                    format = alGetEnumValue("AL_FORMAT_71CHN8");
                else if (fmt.channels() == 8)
                    format = alGetEnumValue("AL_FORMAT_81CHN8");
            }
        }
    } else if (fmt.sampleFormat() == AudioFormat::SampleFormat_Signed16) {
        if (fmt.channels() == 1)
            format = AL_FORMAT_MONO16;
        else if (fmt.channels() == 2)
            format = AL_FORMAT_STEREO16;
        else if (ctx) {
            if (alIsExtensionPresent("AL_EXT_MCFORMATS")) {
                if (fmt.channels() == 4)
                    format = alGetEnumValue("AL_FORMAT_QUAD16");
                else if (fmt.channels() == 6)
                    format = alGetEnumValue("AL_FORMAT_51CHN16");
                else if (fmt.channels() == 7)
                    format = alGetEnumValue("AL_FORMAT_61CHN16");
                else if (fmt.channels() == 8)
                    format = alGetEnumValue("AL_FORMAT_71CHN16");
            }
        }
    } else if (ctx) {
        if (fmt.sampleFormat() == AudioFormat::SampleFormat_Float) {
            if (alIsExtensionPresent("AL_EXT_float32")) {
                if (fmt.channels() == 1)
                    format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
                else if (fmt.channels() == 2)
                    format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
                else if (alIsExtensionPresent("AL_EXT_MCFORMATS")) {
                    if (fmt.channels() == 4)
                        format = alGetEnumValue("AL_FORMAT_QUAD32");
                    else if (fmt.channels() == 6)
                        format = alGetEnumValue("AL_FORMAT_51CHN32");
                    else if (fmt.channels() == 7)
                        format = alGetEnumValue("AL_FORMAT_61CHN32");
                    else if (fmt.channels() == 8)
                        format = alGetEnumValue("AL_FORMAT_71CHN32");
                }
            }
        } else if (fmt.sampleFormat() == AudioFormat::SampleFormat_Double) {
            if (alIsExtensionPresent("AL_EXT_double")) {
                if (fmt.channels() == 1)
                    format = alGetEnumValue("AL_FORMAT_MONO_DOUBLE_EXT");
                else if (fmt.channels() == 2)
                    format = alGetEnumValue("AL_FORMAT_STEREO_DOUBLE_EXT");
            }
        }
    }
    ALCenum err = alGetError();
    if (err != AL_NO_ERROR) {
        qWarning("OpenAL audioFormatToAL error: %s", alGetString(err));
    }
    if (format == 0) {
        qWarning("AudioOutputOpenAL Error: No OpenAL format available for audio data format %s %s."
                 , qPrintable(fmt.sampleFormatName())
                 , qPrintable(fmt.channelLayoutName()));
    }
    qDebug("OpenAL audio format: %#x ch:%d, sample format: %s", format, fmt.channels(), qPrintable(fmt.sampleFormatName()));
    return format;
}

class  AudioOutputOpenALPrivate : public AudioOutputPrivate
{
public:
    AudioOutputOpenALPrivate()
        : AudioOutputPrivate()
        , format_al(AL_FORMAT_STEREO16)
        , state(0)
    {
    }
    ~AudioOutputOpenALPrivate() {
    }

    ALenum format_al;
    ALuint buffer[kBufferCount];
    ALuint source;
    ALint state;
    QMutex mutex;
    QWaitCondition cond;
};

AudioOutputOpenAL::AudioOutputOpenAL()
    :AudioOutput(*new AudioOutputOpenALPrivate())
{
    setFeature(GetPlayedIndices);
}

AudioOutputOpenAL::~AudioOutputOpenAL()
{
    close();
}

bool AudioOutputOpenAL::open()
{
    DPTR_D(AudioOutputOpenAL);
    d.available = false; // TODO: d.reset()
    d.resetBuffers();
    QVector<QByteArray> _devices;
    const char *p = NULL;
    // maybe defined in alext.h
#ifdef ALC_ALL_DEVICES_SPECIFIER
    p = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
#else
    if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT")) {
        // avoid using enum ALC_ALL_DEVICES_SPECIFIER directly
        ALenum param = alcGetEnumValue(NULL, "ALC_ALL_DEVICES_SPECIFIER");
        p = alcGetString(NULL, param);
    } else {
        //alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT")
        //ALenum param = alcGetEnumValue(NULL, "ALC_DEVICE_SPECIFIER");
        p = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    }
#endif
    while (p && *p) {
        _devices.push_back(p);
        p += _devices.last().size() + 1;
    }
    qDebug("OpenAL devices available: %d", _devices.size());
    for (int i = 0; i < _devices.size(); i++) {
        qDebug("device %d: %s", i, _devices[i].constData());
    }
    const ALCchar *default_device = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    qDebug("AudioOutputOpenAL Opening default device: %s", default_device);
    ALCdevice *dev = alcOpenDevice(NULL); //parameter: NULL or default_device
    if (!dev) {
        qWarning("AudioOutputOpenAL Failed to open sound device: %s", alcGetString(0, alcGetError(0)));
        return false;
    }
    qDebug("AudioOutputOpenAL creating context...");
    ALCcontext *ctx = alcCreateContext(dev, NULL);
    alcMakeContextCurrent(ctx);
    //alcProcessContext(ctx); //used when dealing witg multiple contexts
    ALCenum err = alcGetError(dev);
    if (err != ALC_NO_ERROR) {
        qWarning("AudioOutputOpenAL Error: %s", alcGetString(dev, err));
        return false;
    }
    //init params. move to another func?
    d.format_al = audioFormatToAL(audioFormat());

    alGenBuffers(kBufferCount, d.buffer);
    err = alGetError();
    if (err != AL_NO_ERROR) {
        qWarning("Failed to generate OpenAL buffers: %s", alGetString(err));
        goto fail;
    }
    alGenSources(1, &d.source);
    err = alGetError();
    if (err != AL_NO_ERROR) {
        qWarning("Failed to generate OpenAL source: %s", alGetString(err));
        alDeleteBuffers(kBufferCount, d.buffer);
        goto fail;
    }

    alSourcei(d.source, AL_LOOPING, AL_FALSE);
    alSourcei(d.source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcei(d.source, AL_ROLLOFF_FACTOR, 0);
    alSource3f(d.source, AL_POSITION, 0.0, 0.0, 0.0);
    alSource3f(d.source, AL_VELOCITY, 0.0, 0.0, 0.0);
    alListener3f(AL_POSITION, 0.0, 0.0, 0.0);

    //// Initial all buffers. TODO: move to open?
    //alSourcef(d.source, AL_GAIN, d.vol);

    static char init_data[kBufferSize];
    memset(init_data, 0, sizeof(init_data));
    for (int i = 1; i < kBufferCount; ++i) {
        AL_RUN_CHECK(alBufferData(d.buffer[i], d.format_al, init_data, sizeof(init_data), audioFormat().sampleRate()));
        AL_RUN_CHECK(alSourceQueueBuffers(d.source, 1, &d.buffer[i]));
        d.nextEnqueueInfo().data_size = sizeof(init_data);
        d.nextEnqueueInfo().timestamp = 0;
        d.bufferAdded();
    }
    // FIXME: Invalid Operation
    //AL_RUN_CHECK(alSourceQueueBuffers(d.source, sizeof(d.buffer)/sizeof(d.buffer[0]), d.buffer));
    alSourcePlay(d.source);

    d.state = 0;
    d.available = true;
    qDebug("AudioOutputOpenAL open ok...");
    return true;
fail:
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    return false;
}

bool AudioOutputOpenAL::close()
{
    if (!isAvailable())
        return true;
    DPTR_D(AudioOutputOpenAL);
    d.state = 0;
    d.available = false;
    d.resetBuffers();
    alSourceStop(d.source);
    do {
        alGetSourcei(d.source, AL_SOURCE_STATE, &d.state);
    } while (alGetError() == AL_NO_ERROR && d.state == AL_PLAYING);
    ALint processed;
    alGetSourcei(d.source, AL_BUFFERS_PROCESSED, &processed);
    ALuint buf;
    while (processed--) { alSourceUnqueueBuffers(d.source, 1, &buf); }
    alDeleteSources(1, &d.source);
    alDeleteBuffers(kBufferCount, d.buffer);

    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = alcGetContextsDevice(ctx);
    alcMakeContextCurrent(NULL);
    if (ctx) {
        qDebug("alcDestroyContext(%p)", ctx);
        alcDestroyContext(ctx);
        ALCenum err = alcGetError(dev);
        if (err != ALC_NO_ERROR) { //ALC_INVALID_CONTEXT
            qWarning("AudioOutputOpenAL Failed to destroy context: %s", alcGetString(dev, err));
            return false;
        }
    }
    if (dev) {
        qDebug("alcCloseDevice(%p)", dev);
        alcCloseDevice(dev);
        ALCenum err = alcGetError(dev);
        if (err != ALC_NO_ERROR) { //ALC_INVALID_DEVICE
            qWarning("AudioOutputOpenAL Failed to close device: %s", alcGetString(dev, err));
            return false;
        }
    }
    return true;
}

bool AudioOutputOpenAL::isSupported(const AudioFormat& format) const
{
    return !!audioFormatToAL(format);
}

bool AudioOutputOpenAL::isSupported(AudioFormat::SampleFormat sampleFormat) const
{
    return sampleFormat == AudioFormat::SampleFormat_Unsigned8 || sampleFormat == AudioFormat::SampleFormat_Signed16;
}

bool AudioOutputOpenAL::isSupported(AudioFormat::ChannelLayout channelLayout) const
{
    return channelLayout == AudioFormat::ChannelLayout_Mono || channelLayout == AudioFormat::ChannelLayout_Stero;
}

AudioFormat::SampleFormat AudioOutputOpenAL::preferredSampleFormat() const
{
    return AudioFormat::SampleFormat_Signed16;
}

AudioFormat::ChannelLayout AudioOutputOpenAL::preferredChannelLayout() const
{
    return AudioFormat::ChannelLayout_Stero;
}

QString AudioOutputOpenAL::name() const
{
    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = alcGetContextsDevice(ctx);
    const ALCchar *name = alcGetString(dev, ALC_DEVICE_SPECIFIER);
    return name;
}

AudioOutput::Feature AudioOutputOpenAL::supportedFeatures() const
{
    return GetPlayedIndices;
}
#if OLD_AO_API
void AudioOutputOpenAL::waitForNextBuffer()
{
    qDebug("waitForNextBuffer");
    DPTR_D(AudioOutputOpenAL);
    //don't return even if we can add buffer because we have to update dequeue index
    ALint queued = 0;
    alGetSourcei(d.source, AL_BUFFERS_QUEUED, &queued);
    if (queued == 0) { //TODO: why can be 0?
        qDebug("no queued buffer");
        return;
    }
    ALint processed = 0;
    alGetSourcei(d.source, AL_BUFFERS_PROCESSED, &processed);
    //if (processed <= 0 && d.canAddBuffer())
    //    return;
    while (processed <= 0) {
        unsigned long duration = d.format.durationForBytes(d.nextDequeueInfo().data_size)/1000LL;
        QMutexLocker lock(&d.mutex);
        Q_UNUSED(lock);
        d.cond.wait(&d.mutex, duration);
        alGetSourcei(d.source, AL_BUFFERS_PROCESSED, &processed);
    }
    while (processed--) {
        d.bufferRemoved();
    }
}
#endif
// http://kcat.strangesoft.net/openal-tutorial.html
bool AudioOutputOpenAL::write(const QByteArray& data)
{
    DPTR_D(AudioOutputOpenAL);
    if (data.isEmpty())
        return false;
    ALuint buf;
    //unqueues a set of buffers attached to a source
    AL_RUN_CHECK(alSourceUnqueueBuffers(d.source, 1, &buf));
    AL_RUN_CHECK(alBufferData(buf, d.format_al, data.constData(), data.size(), audioFormat().sampleRate()));
    AL_RUN_CHECK(alSourceQueueBuffers(d.source, 1, &buf));
    return true;
}

bool AudioOutputOpenAL::play()
{
    DPTR_D(AudioOutputOpenAL);
    alGetSourcei(d.source, AL_SOURCE_STATE, &d.state);
    if (d.state != AL_PLAYING) {
        qDebug("AudioOutputOpenAL: !AL_PLAYING alSourcePlay");
        alSourcePlay(d.source);
    }
    return true;
}

int AudioOutputOpenAL::getProcessed()
{
    DPTR_D(AudioOutputOpenAL);
    ALint processed = 0;
    alGetSourcei(d.source, AL_BUFFERS_PROCESSED, &processed);
    return processed;
}

} //namespace QtAV
