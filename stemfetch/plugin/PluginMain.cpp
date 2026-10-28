#include "StemFetch.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StemFetchProcessor();
}
