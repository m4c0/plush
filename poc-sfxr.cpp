#pragma leco tool

import hai;
import plush;
import rng;
import siaudio;
import sitime;

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
float vib_speed(float n) { return 0.01f * n * n * audio_rate; }
float vib_depth(float n) { return 0.5f * n; }
} // namespace sfxr

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
      .vib{
          .depth = sfxr::vib_depth(0.5),
          .speed = sfxr::vib_speed(0.5),
      },
      .main_volume = sfxr::main_volume,
  };

  rng::seed();

  p.wave_fn = plush::sqr::vol_at;
  plush::play(p);
  sitime::sleep(1);

  p.wave_fn = plush::noise::vol_at;
  plush::play(p);
  sitime::sleep(1);

  p.wave_fn = plush::sine::vol_at;
  plush::play(p);
  sitime::sleep(1);
}
