#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace noori_choir {
// Non-cumulative, centred, overlapping grains: output frame i always represents
// source frame i. No leading delay, trimming, resampling or added reverb tail.
class OfflineVoice {
public:
 static constexpr int count = 8;
 static float sample(const float* source, int frames, int frame, double rate,
                     int voice, float cents, float variationMs) {
  if (frames <= 0 || frame < 0 || frame >= frames || voice < 0 || voice >= count
      || !std::isfinite(rate) || rate < 8000) throw std::invalid_argument("Invalid offline voice input");
  constexpr double pi = 3.14159265358979323846;
  const double spread = (voice % 2 ? 1.0 : -1.0) * (0.35 + 0.65 * (voice / 2) / 3.0);
  const double window = std::round(rate * .080);
  const double hop = window * .5;
  const double offset = std::fmod((voice + 1) * .61803398875, 1.0) * hop;
  const double base = std::floor((frame + offset) / hop);
  double sum = 0, weight = 0;
  for (int grain = -1; grain <= 1; ++grain) {
   const double centre = (base + grain) * hop - offset;
   const double local = frame - centre;
   if (std::abs(local) >= hop) continue;
   const double w = .5 + .5 * std::cos(pi * local / hop);
   const double drift = std::sin(centre / rate * (1.17 + voice * .13) + voice * 1.91);
   const double ratio = std::pow(2.0, spread * std::clamp(cents, 0.f, 20.f) * (1.0 + .12 * drift) / 1200.0);
   // Timing control varies grain pitch phase, without moving the vocal event.
   const double position = frame + (ratio - 1.0) * (local + drift * std::clamp(variationMs, 0.f, 30.f) * rate * .001);
   const double bounded = std::clamp(position, 0.0, double(frames - 1));
   const int a = int(bounded), b = std::min(a + 1, frames - 1);
   const double x = source[a] + (bounded - a) * (source[b] - source[a]);
   sum += w * x; weight += w;
  }
  // Retain original attacks and exact digital silence; avoid softened consonants.
  const float dry = source[frame];
  if (dry == 0.f) return 0.f;
  const float previous = source[std::max(0, frame - 1)];
  const double attack = std::clamp(std::abs(dry - previous) * 5.0, 0.0, 1.0);
  const double wet = weight > 0 ? sum / weight : dry;
  const double amplitude = .78 + .025 * voice + .012 * std::sin(frame / rate * (2.0 + voice * .17) + voice);
  return float(std::clamp(amplitude * (wet * (1.0 - attack) + dry * attack), -1.0, 1.0));
 }
};
}
