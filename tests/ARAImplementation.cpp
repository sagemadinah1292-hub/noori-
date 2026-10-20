// The internal executable has no VST3 wrapper, so it links the same official
// ARA SDK implementation units that JUCE's VST3 wrapper normally supplies.
#include <juce_audio_processors/juce_audio_processors.h>
#if JucePlugin_Enable_ARA
#include <ARA_Library/PlugIn/ARAPlug.cpp>
#include <ARA_Library/Dispatch/ARAPlugInDispatch.cpp>
#include <ARA_Library/Utilities/ARAPitchInterpretation.cpp>
#include <ARA_Library/Utilities/ARAChannelArrangement.cpp>
#endif
