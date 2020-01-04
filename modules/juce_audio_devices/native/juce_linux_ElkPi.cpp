/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
class ElkPiAudioIODevice : public AudioIODevice
{
public:
    ElkPiAudioIODevice()
        : AudioIODevice("ElkPi", "ElkPi")
    {
        DBG("- Runnning ElkPiAudioIODevice constructor");

        deviceIsOpen = false;
        isRunning = false;
        lastError = "";
        xruns = -1;
        numberOfInputs = 0;
        numberOfOutputs = 0;
        callback = nullptr;
        bufferSize = 512;

        if (raspa_init() != 0)
        {
            DBG("Error running raspa_init()");
        }   
    }

    ~ElkPiAudioIODevice()
    {
        DBG("- Runnning ElkPiAudioIODevice destructor");
        close();
    }

    StringArray getOutputChannelNames() override
    {
        StringArray result;

        for (int i = 1; i <= numberOfOutputs; i++)
            result.add("Out #" + std::to_string(i));

        return result;
    }

    StringArray getInputChannelNames() override
    {
        StringArray result;

        for (int i = 1; i <= numberOfInputs; i++)
            result.add("In #" + std::to_string(i));

        return result;
    }

    Array<double> getAvailableSampleRates() override
    {
    }

    Array<int> getAvailableBufferSizes() override
    {
        Array<int> availableBufferSizes;
        availableBufferSizes.push_back(bufferSize);
        return availableBufferSizes;
    }

    int getDefaultBufferSize() override 
    {  
        return bufferSize;
    }

    String open(const BigInteger &inputChannels, const BigInteger &outputChannels,
                double sampleRate, int bufferSizeSamples) override
    {
        DBG("- Calling ElkPiAudioIODevice.open");

        // set number of input and output channels
        numberOfInputs = raspa_get_num_input_channels();
        numberOfOutputs = raspa_get_num_output_channels();

        // open device and configure raspa callback
        unsigned int debug_flags = 0;
        int returnCode = raspa_open(
            bufferSize,
            processCallback,
            this, // Pass a pointer to current ElkPiAudioIODevice that raspa will pass back to the callback function
            debug_flags);

        if (returnCode != 0)
        {
            DBG("Error runninf raspa_open(): " + raspa_get_error_msg(returnCode));
            lastError = raspa_get_error_msg(returnCode);
            return lastError;
        }

        deviceIsOpen = true;

        // Allocate memory for buffers (?)
        channelInBuffer.calloc(numberOfInputs);
        channelOutBuffer.calloc(numberOfOutputs);

        return {};
    }

    void close() override
    {
        DBG("- Calling ElkPiAudioIODevice.close");

        stop();
        
        lastError = "";
        xruns = -1;
        numberOfInputs = 0;
        numberOfOutputs = 0;
        deviceIsOpen = false;
        isRunning = false;

        // Free memory for buffers (?)
        channelInBuffer.free();
        channelOutBuffer.free();
    }

    bool isOpen() override { return deviceIsOpen; }

    void start(AudioIODeviceCallback *newCallback) override
    {
        DBG("- Calling ElkPiAudioIODevice.start");

        if (!deviceIsOpen)
            return;

        if (isRunning)
        {
            if (newCallback != callback)
            {
                if (newCallback != nullptr)
                    newCallback->audioDeviceAboutToStart(this);

                {
                    ScopedLock lock(callbackLock);
                    std::swap(callback, newCallback);
                }

                if (newCallback != nullptr)
                    newCallback->audioDeviceStopped();
            }
        }
        else
        {
            callback = newCallback;
            int returnCode = raspa_start_realtime();
            if (returnCode == 0)
            {
                isRunning = true;
            } else {
                DBG("Error running raspa_start_realtime(): " + raspa_get_error_msg(returnCode));
            }

            if (callback != nullptr)
            {
                if (isRunning)
                {
                    callback->audioDeviceAboutToStart(this);
                }
                else
                {
                    lastError = "raspa_start_realtime failed";
                    callback->audioDeviceError(lastError);
                }
            }
        }
    }

    void stop() override
    {
        DBG("- Calling ElkPiAudioIODevice.stop");

        AudioIODeviceCallback *oldCallback = nullptr;

        if (callback != nullptr)
        {
            ScopedLock lock(callbackLock);
            std::swap(callback, oldCallback);
        }

        int returnCode = raspa_close(); // NOTE: this also closes device (?)
        if (returnCode == 0){
            isRunning = false;
        } else {
            DBG("Error running raspa_close(): " + raspa_get_error_msg(returnCode));
        }

        if (oldCallback != nullptr)
            oldCallback->audioDeviceStopped(); // Why this?
    }

    
    bool isPlaying() override { return callback != nullptr; }
    
    String getLastError() override { return lastError; }

    int getCurrentBufferSizeSamples() override
    {
        return bufferSize; // ?
    }

    double getCurrentSampleRate() override
    {
    }

    int getCurrentBitDepth() override 
    {
    }

    BigInteger getActiveOutputChannels() const override 
    {
    }
    
    BigInteger getActiveInputChannels() const override 
    {
    }

    int getOutputLatencyInSamples() override
    {
    }

    int getInputLatencyInSamples() override
    {
    }

    void hasControlPanel () override
    {
        return false;
    }

    bool setAudioPreprocessingEnabled(bool shouldBeEnabled)
    {
        return false;
    }

    int getXRunCount() const noexcept override
    {
        return xruns;
    }

private:

    int bufferSize;
    bool deviceIsOpen;
    bool isRunning;
    String lastError;
    int xruns;
    int numberOfInputs;
    int numberOfOutputs;

    CriticalSection callbackLock;
    AudioIODeviceCallback *callback;

    HeapBlock<const float *> channelInBuffer;
    HeapBlock<float *> channelOutBuffer;

    void process(const int numSamples, float *input, float *output)
    {
        // TODO: Calculate xruns and update xruns

        const ScopedLock sl(callbackLock);

        if (callback != nullptr)
        {
            for (int i = 0; i < numberOfInputs; ++i)
            {
                // TODO: copy raspa input channel i to channelInBuffer
                // eg: channelInBuffer[ch] = &context.audioIn[ch * context.audioFrames];
            }

            for (int i = 0; i < numberOfOutputs; ++i)
            {
                // TODO: copy raspa output channel i to channelOutBuffer
                // eg: channelOutBuffer[ch] = &context.audioOut[ch * context.audioFrames];
            }

            callback->audioDeviceIOCallback(channelInBuffer.getData(), numberOfInputs,
                                            channelOutBuffer.getData(), numberOfOutputs,
                                            numSamples);
        }
    }

    static int processCallback(float *input, float *output, void *data)
    {
        // Call process function in ElkPiAudioIODevice
        static_cast<ElkPiAudioIODevice *>(data)->process(data->getCurrentBufferSizeSamples(), input, output);
    }
    
};


//==============================================================================
struct ElkPiAudioIODeviceType : public AudioIODeviceType
{
    ElkPiAudioIODeviceType() : AudioIODeviceType("ElkPi") {}

    StringArray getDeviceNames(bool) const override { return StringArray(ElkPiAudioIODevice::typeName); }
    void scanForDevices() override {}
    int getDefaultDeviceIndex(bool) const override { return 0; }
    int getIndexOfDevice(AudioIODevice *device, bool) const override { return device != nullptr ? 0 : -1; }
    bool hasSeparateInputsAndOutputs() const override { return false; }

    AudioIODevice *createDevice(const String &outputName, const String &inputName) override
    {
        DBG("- Creating ElkPiAudioIODevice device from type");

        if (outputName == ElkPiAudioIODevice::typeName || inputName == ElkPiAudioIODevice::typeName)
            return new ElkPiAudioIODevice();

        return nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ElkPiAudioIODeviceType)
};

//==============================================================================
AudioIODeviceType *AudioIODeviceType::createAudioIODeviceType_ElkPi()
{
    return new ElkPiAudioIODeviceType();
}

} // namespace juce
