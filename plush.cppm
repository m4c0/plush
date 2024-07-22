export module plush;

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
// Notes:
//
// Some magic also happens here. SFXR translated frequency into "period" before
// applying its 8x super-sampling.
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
} // namespace plush::freq

export namespace plush::arpeggio {
constexpr const float null = 1e38;

struct params {
  float limit{null}; // "change speed"
  float mod{};       // "change amount"
};
constexpr float at(float t, const params &p) {
  return t > p.limit ? p.mod : 1.0;
}
} // namespace plush::arpeggio

export namespace plush {
struct params {
  adsr::params adsr{};
  freq::params freq{};
  arpeggio::params arp{};

  float (*wave_fn)(float) = [](float) { return 0.0f; };

  unsigned sample_index{};
};
float vol_at(float t, const params &p) {
  float tt = t * freq::at(t, p.freq) / arpeggio::at(t, p.arp);
  return p.wave_fn(tt) * adsr::vol_at(t, p.adsr);
}
} // namespace plush
