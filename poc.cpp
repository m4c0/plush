#pragma leco tool

import rng;
import siaudio;
import sitime;
import sith;

namespace adsr {
struct params {
  float attack_time{};
  float decay_time{};
  float sustain_time{};
  float sustain_level{};
  float release_time{};
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
  float start_freq{};
  float slide{};
  float delta_slide{};
};
constexpr float at(float t, const params &p) noexcept {
  // s = s0 + v0t + at2/2
  auto s0 = p.start_freq;
  auto v0 = p.slide;
  auto a = p.delta_slide;

  return s0 + v0 * t + a * t * t / 2.0;
}
} // namespace freq

// Notes:
//
// Envelope in SFXR is "attack-sustain-decay", while here we have
// "attack-decay-sustain-release". In a nutshell, they convert as:
// * attack = attack
// * decay = sustain
// * sustain = N/A
// * release = decay
//
namespace sfxr {
float frnd(float n) noexcept { return rng::randf() * n; }
float punch2level(float n) noexcept { return 1.0 + 2.0 * n; }

class coin {
  const adsr::params p{
      .attack_time = 0.0,
      .decay_time = frnd(0.1),
      .sustain_time = 0.0,
      .sustain_level = punch2level(0.3f + frnd(0.3f)),
      .release_time = 0.1f + frnd(0.4),
  };
  const freq::params fp{
      .start_freq = 1000.0,
      .slide = -1.04,
      .delta_slide = 1.004,
  };

  constexpr float sqr(float t) const noexcept {
    float fr = t - static_cast<int>(t);
    return fr > 0.5 ? 1.0 : -1.0;
  }

public:
  float vol_at(float t) const noexcept {
    return sqr(t * freq::at(t, fp)) * adsr::vol_at(t, p);
  }
};
} // namespace sfxr

class player : siaudio::timed_streamer {
  const sfxr::coin m_c{};
  float vol_at(float t) const noexcept {
    constexpr const auto main_vol = 0.5;
    return main_vol * m_c.vol_at(t);
  }
};

void play(auto) {
  player p{};
  // TODO: implement min-freq cutoff
  sitime::sleep(1);
}

int main() {
  rng::seed();
  sith::stateless_thread t{play};
  t.start();

  sitime::sleep(1);
}
