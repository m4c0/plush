#pragma leco tool

import siaudio;
import sitime;
import sith;

extern "C" float sinf(float);

class player : siaudio::timed_streamer {
  constexpr float vol_at(float t) const noexcept {
    return sinf(t * 1000.0) * 0.3;
  }

public:
};

void play(auto) {
  player p{};
  sitime::sleep(2);
}

int main() {
  sith::stateless_thread t{play};
  t.start();

  sitime::sleep(2);
}
