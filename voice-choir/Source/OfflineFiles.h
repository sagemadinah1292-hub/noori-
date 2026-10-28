#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "OfflineVoice.h"
#include <atomic>
#include <functional>

namespace noori_choir {
struct VocalFile {
 juce::AudioBuffer<float> audio;
 double rate = 0;
 juce::String name;
 juce::String originalPath;juce::int64 sampleOffset=0;
};
// A bounded in-memory import. Reject rather than truncate unsupported/huge input.
inline VocalFile readWav(const juce::File& file, const std::atomic<bool>& cancelled) {
 if (!file.hasFileExtension("wav;aif;aiff")) throw std::runtime_error("Select a WAV or AIFF file.");
 juce::AudioFormatManager formats;formats.registerBasicFormats();
 auto stream = file.createInputStream();
 if (!stream) throw std::runtime_error("Cannot open WAV file.");
 std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
 if (!reader || reader->sampleRate < 8000 || reader->sampleRate > 384000
     || reader->lengthInSamples <= 0 || reader->numChannels < 1 || reader->numChannels > 2)
  throw std::runtime_error("Use a valid mono or stereo WAV (8–384 kHz).");
 constexpr juce::int64 maxBytes = 512LL * 1024 * 1024;
 if (reader->lengthInSamples > maxBytes / (reader->numChannels * sizeof(float)))
  throw std::runtime_error("WAV exceeds the 512 MiB decoded audio limit. Split the source first.");
 VocalFile result;
 result.rate = reader->sampleRate; result.name = file.getFileNameWithoutExtension();
 result.originalPath=file.getFullPathName();result.sampleOffset=reader->metadataValues.getValue("bwav time reference","0").getLargeIntValue();
 result.audio.setSize(int(reader->numChannels), int(reader->lengthInSamples));
 for (int offset = 0; offset < result.audio.getNumSamples(); offset += 8192) {
  if (cancelled) throw std::runtime_error("Cancelled.");
  const int n = std::min(8192, result.audio.getNumSamples() - offset);
  if (!reader->read(&result.audio, offset, n, offset, true, true)) throw std::runtime_error("WAV decode failed.");
  for (int ch = 0; ch < result.audio.getNumChannels(); ++ch)
   for (int i = offset; i < offset + n; ++i)
    if (!std::isfinite(result.audio.getSample(ch, i))) throw std::runtime_error("WAV contains non-finite audio.");
 }
 return result;
}

inline juce::StringArray exportWavs(const VocalFile& source, const juce::File& project,
 float cents, float variationMs, const std::atomic<bool>& cancelled, std::atomic<float>& progress) {
 if (!project.isDirectory()) throw std::runtime_error("Select an existing Cubase project directory.");
 if (source.audio.getNumSamples() <= 0) throw std::runtime_error("Import a WAV first.");
 const auto destination = project.getChildFile("NOORI CHOIR");
 if (destination.createDirectory().failed()) throw std::runtime_error("Cannot create NOORI CHOIR directory.");
 const auto id = juce::Uuid().toString();
 const auto staging = destination.getChildFile(".noori-" + id);
 if (staging.createDirectory().failed()) throw std::runtime_error("Cannot create export staging directory.");
 juce::StringArray published;
 try {
  const int frames = source.audio.getNumSamples(), channels = source.audio.getNumChannels();
  juce::AudioBuffer<float> block(channels, 8192);
  juce::WavAudioFormat format;
  for (int v = 0; v < OfflineVoice::count; ++v) {
   const auto name = juce::File::createLegalFileName(source.name).substring(0, 60) + "_" + id + "_Voice_" + juce::String(v + 1) + ".wav";
   auto stream = staging.getChildFile(name).createOutputStream();
   if (!stream) throw std::runtime_error("Cannot open output WAV.");
   std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), source.rate, unsigned(channels), 24, {}, 0));
   if (!writer) throw std::runtime_error("Cannot create 24-bit WAV writer.");
   stream.release(); // writer owns the stream after successful construction
   for (int offset = 0; offset < frames; offset += block.getNumSamples()) {
    if (cancelled) throw std::runtime_error("Cancelled.");
    const int n = std::min(block.getNumSamples(), frames - offset);
    for (int ch = 0; ch < channels; ++ch)
     for (int i = 0; i < n; ++i)
      block.setSample(ch, i, OfflineVoice::sample(source.audio.getReadPointer(ch), frames, offset + i, source.rate, v, cents, variationMs));
    if (!writer->writeFromAudioSampleBuffer(block, 0, n)) throw std::runtime_error("WAV write failed. Check free disk space.");
    progress = float((v + double(offset + n) / frames) / OfflineVoice::count);
   }
   if (!writer->flush()) throw std::runtime_error("WAV flush failed.");
   writer.reset();
  }
  // Publish only complete renders. Unique names never overwrite a previous take.
  juce::Array<juce::File> files;
  staging.findChildFiles(files, juce::File::findFiles, false, "*.wav");
  if (files.size() != OfflineVoice::count) throw std::runtime_error("Incomplete render.");
  for (const auto& file : files) {
   if (cancelled) throw std::runtime_error("Cancelled.");
   const auto target = destination.getChildFile(file.getFileName());
   if (target.exists() || !file.moveFileTo(target)) throw std::runtime_error("Cannot publish WAV files.");
   published.add(target.getFullPathName());
  }
  staging.deleteRecursively(); progress = 1.f; return published;
 } catch (...) {
  for (const auto& path : published) juce::File(path).deleteFile();
  staging.deleteRecursively(); throw;
 }
}
}
