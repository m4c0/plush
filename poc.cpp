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

  return s0 + v0 * t + a * t * t / 2.0;
}
} // namespace freq

namespace arpeggio {
static constexpr const float null = 1e38;

struct params {
  float limit; // "change speed"
  float mod;   // "change amount"
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
constexpr float vol_at(float t) {
  float fr = t - static_cast<int>(t);
  return sin(fr * 2.0f * 3.14159265358979323f);
}
}; // namespace sine

namespace sfxr {
// Magic constants that are very specific to sfxr's UI

constexpr const float audio_rate = 44100;
constexpr const float subsmp_rate = 8.0 * audio_rate;

// master * (2.0 * sound) vols in sfxr
constexpr const auto main_volume = 0.05 * 2.0 * 0.5;

float frnd(float n) { return rng::randf() * n; }
float punch2level(float n) { return -(1.0 + 2.0 * n); }
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

class coin {
  adsr::params p{
      .attack_time = 0.0,
      .decay_time = frnd(0.1),
      .sustain_time = 0.0,
      .sustain_level = punch2level(0.3f + frnd(0.3f)),
      .release_time = 0.1f + frnd(0.4),
  };
  freq::params fp{
      .start_freq = freq2freq(0.4f + frnd(0.5f)),
      .slide = 0,
      .delta_slide = 0,
  };
  arpeggio::params ap{
      .limit = frnd(1) > 0.9 ? arpeggio::null : arp_limit(0.5f + frnd(0.2f)),
      .mod = arp_mod(0.2f + frnd(0.4f)),
  };

public:
  float vol_at(float t) const {
    float tt = t * freq::at(t, fp) / arpeggio::at(t, ap);
    return sqr::vol_at(tt) * adsr::vol_at(t, p);
  }
};

class laser {
  static freq::params freq_0() {
    float base_freq = 0.5f + frnd(0.5f);
    return {
        .start_freq = freq2freq(base_freq),
        .min_freq = freq2freq(dotz::min(0.2f, base_freq - 0.2f - frnd(0.6f))),
        .slide = ramp2slide(-0.15f - frnd(0.2f)),
    };
  }
  static freq::params freq_1() {
    return {
        .start_freq = freq2freq(0.3f + frnd(0.6f)),
        .min_freq = freq2freq(0.1f),
        .slide = ramp2slide(-0.35f - frnd(0.3f)),
    };
  }

  adsr::params p{
      .attack_time = 0.0,
      .decay_time = 0.1f + frnd(0.2f),
      .sustain_time = 0.0,
      .sustain_level = frnd(1) > 0.5 ? 0.0f : punch2level(0.3f),
      .release_time = frnd(0.4),
  };
  freq::params fp = frnd(1) > 0.33 ? freq_0() : freq_1();

  // TODO: wave type
  // TODO: square duty
  // TODO: phase
  // TODO: hpf
};
} // namespace sfxr

static sfxr::coin coin{};

volatile unsigned g_idx{};
void fill_buffer(float *buf, unsigned len) {
  auto idx = g_idx;
  for (auto i = 0; i < len; ++i, ++idx) {
    auto t = static_cast<float>(idx) / sfxr::audio_rate;
    *buf++ = sfxr::main_volume * coin.vol_at(t);
  }
  g_idx = idx;
}

int main() {
  // TODO: implement min-freq cutoff

  rng::seed();
  coin = sfxr::coin{};

  siaudio::filler(fill_buffer);
  siaudio::rate(sfxr::audio_rate);

  sitime::sleep(1);
}
