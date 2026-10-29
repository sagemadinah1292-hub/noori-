#pragma once
#include <algorithm>
#include <cstdint>
namespace noori_choir {
constexpr std::uint64_t renderMiB=1024ULL*1024;
// A 64-bit plugin may use more than the old fixed 512 MiB, but reserve at least
// half the currently available physical RAM for Cubase and the rest of the OS.
inline std::uint64_t renderBudgetForAvailable(std::uint64_t available,std::uint64_t resident=0){constexpr std::uint64_t cap=2ULL*1024*renderMiB;return resident>=cap?cap:resident+std::min(cap-resident,available/2);}
inline std::uint64_t renderWorkingBytes(std::uint64_t frames,int sourceChannels,int outputChannels,int newVoices,std::uint64_t retained=0){
 return frames*sizeof(float)*(std::uint64_t(sourceChannels)+2*std::uint64_t(outputChannels)*newVoices+2)+retained+64*renderMiB;
}
}
