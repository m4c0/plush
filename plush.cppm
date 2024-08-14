module;
#include "math.h"

export module plush;
import hai;
import jup;
import rng;
import siaudio;

export namespace plush::adsr {
// Notes:
//
// Envelope in SFXR is "attack-sustain-decay", while here we have
// "attack-decay-sustain-release". In a nutshell, they convert as:
// * attack = attack
// * decay = sustain
// * sustain = N/A
// * release = decay
//
// In SFXR, the sustain stage is an upward volume from 1 to (1 + 2 * punch), so
// we need a sustain_level greater than one to simulate the same audio level
struct params {
  float attack_time{};
  float decay_time{};
  float sustain_time{};
  float sustain_level{};
  float release_time{};
};
constexpr float vol_at(float t, const params &p) {
  if (t < 0)
    return 0;
  if (t < p.attack_time)
    return t / p.attack_time;

  t -= p.attack_time;
  if (t < p.decay_time) {
    auto dt = (p.decay_time - t) / p.decay_time;
    return p.sustain_level + dt * (1.0 - p.sustain_level);
  }

  t -= p.decay_time;
  if (t < p.sustain_time)
    return p.sustain_level;

  t -= p.sustain_time;
  if (t < p.release_time) {
    auto dt = (p.release_time - t) / p.release_time;
    return p.sustain_level * dt;
  }

  return 0;
}
} // namespace plush::adsr

export namespace plush::freq {
// SFXR's "min frequency" cuts the audio off, not respecting the envelope.
struct params {
  float start_freq{};
  /// Sound stops when this frequency is reached
  float min_freq{};
  /// Speed of frequency change (positive increases)
  float slide{};
  // /Acceleration of frequency change (positive increases)
  float delta_slide{};
};
constexpr float at(float t, const params &p) {
  // s = s0 + v0t + at2/2
  auto s0 = p.start_freq;
  auto v0 = p.slide;
  auto a = p.delta_slide;

  auto f = s0 + v0 * t + a * t * t / 2.0;
  // TODO: keep SFXR behaviour or change this to min(f, min_freq)?
  return f > p.min_freq ? f : 0.0;
}
} // namespace plush::freq

/// Roughly speaking, arpeggio is a music concept of playing a note before the
/// previous one finishes
export namespace plush::arpeggio {
constexpr const float null = 1e38;

struct params {
  /// Instant when the next note plays
  float limit{null}; // "change speed"
  /// Frequency modification applied to the note. Applies 1/mod to the frequence
  /// after the limit is reached
  float mod{};       // "change amount"
};
constexpr float at(float t, const params &p) {
  return t > p.limit ? p.mod : 1.0;
}
} // namespace plush::arpeggio

export namespace plush::vibrato {
struct params {
  float depth{};
  float speed{};
};
constexpr float at(float t, const params &p) {
  return t * (1.0 + p.depth * sin(p.speed * t));
}
} // namespace plush::vibrato

export namespace plush::sqr {
// TODO square duty
// TODO duty sweep
constexpr float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return fr > 0.5 ? 1.0 : -1.0;
}
} // namespace plush::sqr

export namespace plush::saw {
constexpr float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return fr * 2.0f - 1.0f;
}
}; // namespace plush::saw

export namespace plush::sine {
float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return sin(fr * 2.0f * 3.14159265358979323f);
}
}; // namespace plush::sine

// TODO: use a pre-gen random table to avoid rng shenanigans?
export namespace plush::noise {
float vol_at(float t) {
  // This was arbitrarily defined to make a noise sound at the same level as
  // another wave form with the same parameters. Somehow there was a higher
  // "punch" in SFXR's noise
  static constexpr const auto magic_noise_volume = 5.0f;
  return (rng::rand(64) - 32.0f) / magic_noise_volume;
}
}; // namespace plush::noise

namespace plush {
export struct params {
  adsr::params adsr{};
  freq::params freq{};
  arpeggio::params arp{};
  vibrato::params vib{};

  float main_volume{1.0};

  float (*wave_fn)(float) = [](float) { return 0.0f; };

  unsigned sample_index{};
};
float vol_at(float t, const params &p) {
  float tt = t * freq::at(t, p.freq) / arpeggio::at(t, p.arp);
  tt = vibrato::at(tt, p.vib);
  return p.wave_fn(tt) * adsr::vol_at(t, p.adsr);
}

// Using a pointer allows a semi-atomic parameter swap
extern hai::uptr<params> g_params;

void fill_buffer(float *buf, unsigned len) {
  static constexpr const auto subsample_count = 8;
  auto subsample_rate = subsample_count * jup::rate;

  // Using a 8x subsampling generates more pleasing sounds
  auto idx = g_params->sample_index * subsample_count;
  for (auto i = 0; i < len; ++i) {
    float smp{};
    for (auto is = 0; is < subsample_count; is++, idx++) {
      auto t = static_cast<float>(idx) / subsample_rate;
      smp += vol_at(t, *g_params);
    }
    *buf++ = g_params->main_volume * smp / static_cast<float>(subsample_count);
  }
  g_params->sample_index = idx / subsample_count;
}

extern hai::array<float> g_buffer;
export void play(const params &p) {
  g_params.reset(new params{p});

  fill_buffer(g_buffer.begin(), g_buffer.size());
  jup::play(g_buffer.begin(), g_buffer.size());
}
} // namespace plush

module :private;
hai::uptr<plush::params> plush::g_params{new params{}};
hai::array<float> plush::g_buffer{jup::rate * 2};
