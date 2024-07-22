#pragma leco tool

import plush;
import rng;
import sitime;

int main() {
  plush::params p{
      .adsr{
          .attack_time{},
          .decay_time = 0.2,
          .sustain_time{},
          .sustain_level = 1.0,
          .release_time = 0.3,
      },
      .freq{
          .start_freq = 440,
          .min_freq{},
          .slide{},
          .delta_slide{},
      },
      .main_volume = 0.5,
  };

  rng::seed();

  /*
  p.wave_fn = plush::sqr::vol_at;
  plush::play(p);
  sitime::sleep(1);

  p.wave_fn = plush::noise::vol_at;
  plush::play(p);
  sitime::sleep(1);
  */

  p.wave_fn = plush::sine::vol_at;
  plush::play(p);
  sitime::sleep(1);
}
