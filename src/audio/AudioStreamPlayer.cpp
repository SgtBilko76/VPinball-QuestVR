// license:GPLv3+

#include "core/stdafx.h"
#include "AudioStreamPlayer.h"
#include "utils/denormals.h"

namespace VPX
{

std::unique_ptr<AudioStreamPlayer> AudioStreamPlayer::Create(SDL_AudioDeviceID sdlDevice, int frequency, int channels, bool isFloat)
{
   SDL_AudioSpec streamSpec;
   streamSpec.freq = frequency;
   streamSpec.format = isFloat ? SDL_AUDIO_F32 : SDL_AUDIO_S16;
   streamSpec.channels = channels;
   SDL_AudioSpec deviceSpec;
   SDL_GetAudioDeviceFormat(sdlDevice, &deviceSpec, nullptr);
   SDL_AudioStream* stream = SDL_CreateAudioStream(&streamSpec, &deviceSpec);
   if (stream)
   {
      SDL_BindAudioStream(sdlDevice, stream);
      SDL_ResumeAudioStreamDevice(stream);
      return std::make_unique<AudioStreamPlayer>(stream);
   }
   else
   {
      PLOGE << "Failed to create stream: " << SDL_GetError();
      return nullptr;
   }
}

AudioStreamPlayer::AudioStreamPlayer(SDL_AudioStream* stream)
   : m_stream(stream)
   #ifdef ENABLE_DX9
   , m_startTimestamp(msec())
   #else
   , m_startTimestamp(SDL_GetTicks())
   #endif
{
   assert(stream != nullptr);
   SDL_GetAudioStreamFormat(m_stream, &m_audioSpec, nullptr);
   SDL_SetAudioStreamGetCallback(m_stream, &AudioStreamCallback, this);
}

AudioStreamPlayer::~AudioStreamPlayer()
{
   SDL_DestroyAudioStream(m_stream);
}

void AudioStreamPlayer::Enqueue(const uint8_t* buffer, int length)
{
   // If we are really late and decided to resync, do it when pushing new data (limit silence time, handle situation where no more data are coming)
   if (m_resync)
   {
      SDL_ClearAudioStream(m_stream);
      #ifdef ENABLE_DX9
      m_startTimestamp = msec();
      #else
      m_startTimestamp = SDL_GetTicks();
      #endif
      m_streamedTotal = 0;
      m_resync = false;
      PLOGI << "Audio stream sync was lost and reseted";
   }
   // Adjust the gain applied to quiet sources (ROM audio is usually far below the level of the table samples) before enqueueing the data it was computed from
   if (m_autoGain)
      UpdateAutoGain(buffer, length);

   // Just enqueue, syncing and compensation is done on a regular basis when data is requested for playing
   SDL_PutAudioStreamData(m_stream, buffer, length);
   m_streamedTotal += length;
}

void AudioStreamPlayer::FlushStream()
{
   SDL_FlushAudioStream(m_stream);
}

int AudioStreamPlayer::GetQueuedSize() const
{
   return SDL_GetAudioStreamQueued(m_stream);
}

void AudioStreamPlayer::SetStreamVolume(const float volume)
{
   if (m_streamVolume != volume)
   {
      m_streamVolume = volume;
      ApplyGain();
   }
}

void AudioStreamPlayer::SetMainVolume(const float volume)
{
   if (m_mainVolume != volume)
   {
      m_mainVolume = volume;
      ApplyGain();
   }
}

void AudioStreamPlayer::SetAutoGain(const bool enable)
{
   if (m_autoGain != enable)
   {
      m_autoGain = enable;
      m_autoGainValue = 1.f;
      m_autoGainPeak = 0.f;
      ApplyGain();
   }
}

void AudioStreamPlayer::ApplyGain()
{
   SDL_SetAudioStreamGain(m_stream, m_streamVolume * m_mainVolume * m_autoGainValue);
}

// Level the stream to a target peak, only boosting (never attenuating below the source level), to compensate for the
// very low output level of some ROMs which would otherwise be drowned out by the table samples.
void AudioStreamPlayer::UpdateAutoGain(const uint8_t* buffer, int length)
{
   constexpr float targetPeak = 0.89f; // Leave a little headroom below saturation
   constexpr float maxGain = 4.f; // Up to +12dB
   constexpr float silenceThreshold = 0.02f; // Below this, the source is considered silent and the gain is kept as is
   constexpr float peakReleaseTimeConstant = 3.f; // Peak envelope decay in seconds (the level is evaluated over the last few seconds)
   constexpr float riseDbPerSecond = 3.f; // Slowly boost to avoid audible pumping
   constexpr float fallDbPerSecond = 40.f; // Quickly back off to avoid saturating on a sudden loud part

   const int frameSize = SDL_AUDIO_FRAMESIZE(m_audioSpec);
   const int nFrames = (frameSize > 0) ? (length / frameSize) : 0;
   if (nFrames <= 0)
      return;

   float peak = 0.f;
   if (SDL_AUDIO_ISFLOAT(m_audioSpec.format))
   {
      const float* const samples = reinterpret_cast<const float*>(buffer);
      for (int i = 0; i < nFrames * m_audioSpec.channels; ++i)
         peak = max(peak, fabsf(samples[i]));
   }
   else
   {
      const int16_t* const samples = reinterpret_cast<const int16_t*>(buffer);
      for (int i = 0; i < nFrames * m_audioSpec.channels; ++i)
         peak = max(peak, static_cast<float>(abs(samples[i])) * (float)(1.0 / 32768.0));
   }

   const float elapsed = static_cast<float>(nFrames) / static_cast<float>(m_audioSpec.freq);
   m_autoGainPeak = max(peak, m_autoGainPeak * expf(-elapsed / peakReleaseTimeConstant));
   if (m_autoGainPeak > silenceThreshold)
   {
      const float target = clamp(targetPeak / m_autoGainPeak, 1.f, maxGain);
      m_autoGainValue = (target > m_autoGainValue) ? min(target, m_autoGainValue * powf(10.f, riseDbPerSecond * elapsed / 20.f))
                                                   : max(target, m_autoGainValue * powf(10.f, -fallDbPerSecond * elapsed / 20.f));
      ApplyGain();
   }
}

void AudioStreamPlayer::AudioStreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
   set_denormals_flush_to_zero_once(); // SDL resamples and mixes on this thread, which is created by SDL

   const auto me = static_cast<AudioStreamPlayer*>(userdata);
   const unsigned int nQueueSize = max(0, SDL_GetAudioStreamQueued(stream) - total_amount);
   const uint64_t nBytePerSec = me->m_audioSpec.freq * (uint64_t)SDL_AUDIO_FRAMESIZE(me->m_audioSpec);
   const uint64_t sourceTS = (1000 * me->m_streamedTotal) / nBytePerSec; // Total amount of music streamed (ms)
   const uint64_t playedTS = (1000 * (me->m_streamedTotal - nQueueSize)) / nBytePerSec; // Playing position (ms)
   #ifdef ENABLE_DX9
   const uint64_t nowTS = msec() - me->m_startTimestamp; // Where we should be in the music (ms)
   #else
   const uint64_t nowTS = SDL_GetTicks() - me->m_startTimestamp; // Where we should be in the music (ms)
   #endif
   float throttle = 1.f;
   //PLOGI << "Get stream data for " << me->m_name << " enqueued: " << ((float)SDL_GetAudioStreamQueued(stream) / SDL_AUDIO_FRAMESIZE(me->m_audioSpec)) << " samples enqueued";
   if (playedTS > nowTS)
   {
      // We have played ahead of the source: either the source is paused or it is having issues => just resync silently
      me->m_startTimestamp += playedTS - nowTS;
   }
   else if (nowTS > playedTS)
   {
      const uint64_t deltaTS = nowTS - playedTS;
      if (nQueueSize > nBytePerSec && deltaTS > 1000)
      {
         // We are really late, just resync on next stream update (don't change throttling to avoid adding some glitches to the already glitched stream)
         throttle = me->m_throttling;
         me->m_resync = true;
      }
      /* else if (nQueueSize > 200 * nBytePerSec && deltaTS > ((me->m_throttling == 1.f) ? 100 : 50))
      {
         // We are a bit late, try to catch up by slightly increasing the pitch
         throttle = 1.0f + static_cast<float>(min(deltaTS - 20, 500ull)) / 500.f;
      }*/
   }
   if (me->m_throttling != throttle)
   {
      me->m_throttling = throttle;
      SDL_SetAudioStreamFrequencyRatio(me->m_stream, throttle);
      PLOGI << "PlayedTS: " << playedTS << "ms / NowTS: " << nowTS << "ms / Delta: " << (nowTS - playedTS) << "ms / Buffer: " << (sourceTS - playedTS) << "ms / Frequency ratio : " << throttle;
   }
}

}
