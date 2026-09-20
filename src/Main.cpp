#include <JuceHeader.h>

#include "PluginProcessor.h"

#if defined (JUCE_PROJUCER_VERSION) && JUCE_PROJUCER_VERSION < JUCE_VERSION
#error "This project is using an older version of the JUCE project config than the plugin includes"
#endif

int main (int argc, char* argv[])
{
    juce::JUCEApplicationBase::main (argc, argv);
    return 0;
}
