#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include <string>
namespace noori {
using Sample = std::int64_t;
struct Audio {
 double sampleRate = 48000;
 std::vector<std::vector<float>> channels;
 Sample length() const { return channels.empty() ? 0 : (Sample) channels[0].size(); }
 void validate() const;
};
struct TempoPoint { Sample sample; double bpm; double quarter; };
class Timeline {
public:
 explicit Timeline(double sampleRate = 48000, std::vector<TempoPoint> points = {{0,80,0}});
 double quarterAt(Sample sample) const;
 Sample sampleAt(double quarter) const;
 const std::vector<TempoPoint>& points() const { return map; }
 double sampleRate() const { return rate; }
private:
 double rate;
 std::vector<TempoPoint> map;
};
struct Settings {
 int division = 8;
 enum class Rhythm { straight, triplet, dotted } rhythm = Rhythm::straight;
 double strength = 1, swing = 0, offsetMs = 0, humanizeMs = 0, toleranceMs = 0;
 double sensitivity = .55, maxStretch = 1.3333333333;
 bool breathProtection = true, consonantProtection = true, vowelProtection = true;
 bool emotionalProtection = false, naturalNaat = true, sampleAccurate = true;
};
struct Attack { Sample source = 0, target = 0; float confidence = 0, voicing = 0; bool selected = false; };
using Attacks = std::vector<Attack>;
using Progress = std::function<bool(float)>; // false cancels
std::vector<Sample> detectClicks(const Audio&);
Attacks detect(const Audio&, const Settings&, Progress = {});
Attacks quantize(const Attacks&, Sample length, Sample projectStart, const Timeline&, const Settings&, bool selectedOnly = false, const std::vector<Sample>& reference = {}, bool exactBpm = false);
Attacks moveAttack(const Attacks&, std::size_t index, Sample target, Sample length, double maxStretch);
Audio render(const Audio&, const Attacks&, const Settings&, Progress = {});
std::string serializeAttacks(const Attacks&);
Attacks deserializeAttacks(const std::string&, Sample length);
}
