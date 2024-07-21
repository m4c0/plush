#pragma leco tool
#include "math.h"

import dotz;
import rng;
import siaudio;
import sitime;

namespace adsr {
// Notes:
//
// Envelope in SFXR is "attack-sustain-decay", while here we have
// "attack-decay-sustain-release". In a nutshell, they convert as:
// * attack = attack
// * decay = sustain
// * sustain = N/A
// * release = decay
//
// In SFXR, the sustein stage is an upward volume from 1 to (1 + 2 * punch), so
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
} // namespace adsr

namespace freq {
struct params {
  float start_freq{};
  float min_freq{};
  float slide{};
  float delta_slide{};
};
constexpr float at(float t, const params &p) {
  // s = s0 + v0t + at2/2
  auto s0 = p.start_freq;
  auto v0 = p.slide;
  auto a = p.delta_slide;

  auto f = s0 + v0 * t + a * t * t / 2.0;
  return f > p.min_freq ? f : 0.0;
}
} // namespace freq

namespace arpeggio {
static constexpr const float null = 1e38;

struct params {
  float limit{null}; // "change speed"
  float mod{};       // "change amount"
};
constexpr float at(float t, const params &p) {
  return t > p.limit ? p.mod : 1.0;
}
} // namespace arpeggio

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

struct params {
  adsr::params p{};
  freq::params fp{};
  arpeggio::params ap{};
};
float vol_at(float t, const params &p, const auto &wave_fn) {
  float tt = t * freq::at(t, p.fp) / arpeggio::at(t, p.ap);
  return wave_fn(tt) * adsr::vol_at(t, p.p);
}

namespace sfxr {
// Magic constants that are very specific to sfxr's UI

constexpr const float audio_rate = 44100;
constexpr const float subsmp_rate = 8.0 * audio_rate;

// master * (2.0 * sound) vols in sfxr
constexpr const auto main_volume = 0.05 * 2.0 * 0.5;

float frnd(float n) { return rng::randf() * n; }
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
    return arpeggio::null;

  auto ip = 1.0f - n;
  auto limit_frame_count = (int)(ip * ip * 20000 + 32);
  return (float)limit_frame_count / audio_rate;
}
} // namespace sfxr

const params g_p{
    .p{
        .attack_time{},
        .decay_time = 0.1,
        .sustain_time{},
        .sustain_level = sfxr::punch2level(0.6),
        .release_time = 0.4,
    },
    .fp{
        .start_freq = sfxr::freq2freq(0.5),
        .min_freq{},
        .slide{},
        .delta_slide{},
    },
    .ap{
        .limit = sfxr::arp_limit(0.5),
        .mod = sfxr::arp_mod(0.5),
    },
};
const auto wave_fn = sqr::vol_at;

volatile unsigned g_idx{};
void fill_buffer(float *buf, unsigned len) {
  auto idx = g_idx;
  for (auto i = 0; i < len; ++i, ++idx) {
    auto t = static_cast<float>(idx) / sfxr::audio_rate;
    *buf++ = sfxr::main_volume * vol_at(t, g_p, wave_fn);
  }
  g_idx = idx;
}

int main() {
  rng::seed();

  siaudio::filler(fill_buffer);
  siaudio::rate(sfxr::audio_rate);

  sitime::sleep(1);
}
