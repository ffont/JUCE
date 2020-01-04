/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
int main (int argc, char* argv[])
{

    auto input = File::createTempFile ("test.wav");
    input.replaceWithData (BinaryData::_31655_loop_wav, BinaryData::_31655_loop_wavSize);
    
    AudioFormatManager fmgr;
    fmgr.registerBasicFormats();
    std::unique_ptr<AudioFormatReaderSource> source;
    source.reset(new AudioFormatReaderSource (fmgr.createReaderFor(input), true));
    
    std::cout << "Initialise audio devices..." << std::endl;
    AudioDeviceManager devmgr;
    devmgr.initialiseWithDefaultDevices (0, 2);
    AudioIODevice* device = devmgr.getCurrentAudioDevice();
    if (device && source) {
        std::cout << "Playing audio file..." << std::endl;
        source->prepareToPlay (device->getDefaultBufferSize(),
                               device->getCurrentSampleRate());
        std::unique_ptr<AudioSourcePlayer> player;
        player.reset(new AudioSourcePlayer ());
        player->setSource (source.get());
        devmgr.addAudioCallback (player.get());
        while (source->getNextReadPosition() < source->getTotalLength())
            Thread::sleep(100);
        std::cout << "Done!" << std::endl;
        return 0;
    }
    std::cout << "Failed opening device :( :( :(" << std::endl;
    return -1;
}
