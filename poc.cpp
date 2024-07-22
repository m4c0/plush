#pragma leco tool
#include "math.h"

import dotz;
import hai;
import plush;
import rng;
import siaudio;
import sitime;

namespace sqr {
// TODO square duty
// TODO duty sweep
constexpr float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return fr > 0.5 ? 1.0 : -1.0;
}
} // namespace sqr

namespace saw {
constexpr float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return fr * 2.0f - 1.0f;
}
}; // namespace saw

namespace sine {
float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return sin(fr * 2.0f * 3.14159265358979323f);
}
}; // namespace sine

namespace noise {
float vol_at(float t) {
  // This was arbitrarily defined to make a noise sound at the same level as
  // another wave form with the same parameters. Somehow there was a higher
  // "punch" in SFXR's noise
  static constexpr const auto magic_noise_volume = 5.0f;
  return (rng::rand(64) - 32.0f) / magic_noise_volume;
}
}; // namespace noise

namespace sfxr {
// Magic constants that are very specific to sfxr's UI

constexpr const float audio_rate = 44100;
constexpr const float subsmp_rate = 8.0 * audio_rate;

// master * (2.0 * sound) vols in sfxr
constexpr const auto main_volume = 0.05 * 2.0 * 0.5;

float frnd(float n) { return rng::randf() * n; }

// These translates the slider value in SFXR (either [0;1] or [-1;1]) to numbers
// we can use
float time2time(float n) { return n * n * 100000.0f / audio_rate; }
float punch2level(float n) { return 1.0 + 2.0 * n; }
float freq2freq(float n) { return subsmp_rate * (n * n) / 100.0f; }
float ramp2slide(float n) { return subsmp_rate / (1 - (n * n * n) * 0.01f); }
float dramp2dslide(float n) { return subsmp_rate / (-(n * n * n) * 0.000001f); }
float arp_mod(float n) {
  if (n >= 0.0f) {
    return 1.0 - n * n * 0.9;
  } else {
    return 1.0 + n * n * 10.0;
  }
}
float arp_limit(float n) {
  if (n == 1.0)
    return plush::arpeggio::null;

  auto ip = 1.0f - n;
  auto limit_frame_count = (int)(ip * ip * 20000 + 32);
  return (float)limit_frame_count / audio_rate;
}
} // namespace sfxr

// Using a pointer allows a semi-atomic parameter swap
static hai::uptr<plush::params> g_p{new plush::params{}};

void fill_buffer(float *buf, unsigned len) {
  static constexpr const auto subsample_count = 8;
  static constexpr const auto subsample_rate =
      subsample_count * sfxr::audio_rate;

  // Using a 8x subsampling generates more pleasing sounds
  auto idx = g_p->sample_index * subsample_count;
  for (auto i = 0; i < len; ++i) {
    float smp{};
    for (auto is = 0; is < subsample_count; is++, idx++) {
      auto t = static_cast<float>(idx) / subsample_rate;
      smp += plush::vol_at(t, *g_p);
    }
    *buf++ = sfxr::main_volume * smp / static_cast<float>(subsample_count);
  }
  g_p->sample_index = idx / subsample_count;
}

int main() {
  plush::params p{
      .adsr{
          .attack_time{},
          .decay_time = sfxr::time2time(0.1),
          .sustain_time{},
          .sustain_level = sfxr::punch2level(0.6),
          .release_time = sfxr::time2time(0.4),
      },
      .freq{
          .start_freq = sfxr::freq2freq(0.5),
          .min_freq{},
          .slide{},
          .delta_slide{},
      },
      .arp{
          .limit = sfxr::arp_limit(0.5),
          .mod = sfxr::arp_mod(0.5),
      },
  };

  rng::seed();

  siaudio::filler(fill_buffer);
  siaudio::rate(sfxr::audio_rate);

  p.wave_fn = sqr::vol_at;
  g_p.reset(new plush::params{p});
  sitime::sleep(1);

  p.wave_fn = noise::vol_at;
  g_p.reset(new plush::params{p});
  sitime::sleep(1);

  p.wave_fn = sine::vol_at;
  g_p.reset(new plush::params{p});
  sitime::sleep(1);
}
