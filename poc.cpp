#pragma leco tool

import siaudio;
import sitime;
import sith;

extern "C" float sinf(float);

namespace adsr {
struct params {
  float attack_time;
  float decay_time;
  float sustain_time;
  float sustain_level;
  float release_time;
};
constexpr float vol_at(float t, const params &p) noexcept {
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
  float start_freq;
  float slide;
  float delta_slide;
};
constexpr float at(float t, const params &p) noexcept { return p.start_freq; }
} // namespace freq

class player : siaudio::timed_streamer {
  constexpr float vol_at(float t) const noexcept {
    constexpr const adsr::params p{
        .attack_time = 0.1,
        .decay_time = 1.0,
        .sustain_time = 1.8,
        .sustain_level = 0.2,
        .release_time = 1.0,
    };
    constexpr const freq::params fp{
        .start_freq = 1000.0,
    };
    constexpr const auto main_vol = 1.0;
    return sinf(t * freq::at(t, fp)) * main_vol * adsr::vol_at(t, p);
  }
};

void play(auto) {
  player p{};
  sitime::sleep(5);
}

int main() {
  sith::stateless_thread t{play};
  t.start();

  sitime::sleep(1);
}
