#ifndef AUDIO_DEVICE_OPENSLES_ANDROID_H
#define AUDIO_DEVICE_OPENSLES_ANDROID_H

#include "player_helper.h"

#include <jni.h> // For accessing AudioDeviceAndroid.java
#include <queue>
#include <stdio.h>
#include <stdlib.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>


const int N_MAX_INTERFACES = 3;
const int N_MAX_OUTPUT_DEVICES = 6;
const int N_MAX_INPUT_DEVICES = 3;

const int N_REC_SAMPLES_PER_SEC = 16000;//44000;  // Default fs
const int N_PLAY_SAMPLES_PER_SEC = 16000;//44000; // Default fs

const int N_REC_CHANNELS = 1; // default is mono recording
const int N_PLAY_CHANNELS = 1; // default is mono playout

const int REC_BUF_SIZE_IN_SAMPLES = 480; // Handle max 10 ms @ 48 kHz
const int PLAY_BUF_SIZE_IN_SAMPLES = 480;

// Number of the buffers in playout queue
const unsigned short N_PLAY_QUEUE_BUFFERS = 2;
// Number of buffers in recording queue
const unsigned short N_REC_QUEUE_BUFFERS = 2;
// Number of 10 ms recording blocks in rec buffer
const unsigned short N_REC_BUFFERS = 20;

const unsigned short N_PALYOUT_BUFFER_LEN = 3;

/*useage
AudioDeviceAndroidOpenSLES* outputDevice = new  AudioDeviceAndroidOpenSLES(0);
outputDevice->Init();
bool ok;
outputDevice->PlayoutIsAvailable( ok );
if(ok){
	//set data offer
	outputDevice->AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);
	outputDevice->StartPlayout();
	....
	outputDevice->StopPlayout();
	
	outputDevice->Terminate();
	delete outputDevice;
}
*/
class AudioDeviceAndroidOpenSLES 
{
public:
    AudioDeviceAndroidOpenSLES(const int id);
    ~AudioDeviceAndroidOpenSLES();

    // Main initializaton and termination
    virtual int Init();
    virtual int Terminate();
    virtual bool Initialized() const;

    // Audio transport initialization
    virtual int PlayoutIsAvailable(bool& available);
    virtual int InitPlayout();
    virtual bool PlayoutIsInitialized() const;

	// Attach audio buffer
    virtual void AttachAudioBuffer(  void (*callback)(void *opaque, char *stream, int len) , void* data); //virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);
	
    // Audio transport control
    virtual int StartPlayout();
    virtual int StopPlayout();
    virtual bool Playing() const;

    // Audio mixer initialization
    virtual int SpeakerIsAvailable(bool& available);
    virtual int InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
	virtual int SetSpeakerVolume(int volume);
    ////SLPlayItf playItf;  // not use

    // Delay information and control
    virtual int PlayoutDelay(unsigned short& delayMS) const;

    // Speaker audio routing
    virtual int SetLoudspeakerStatus(bool enable);
    virtual int GetLoudspeakerStatus(bool& enable) const;

private:
    // Lock
    void Lock();
    void UnLock();

    static void PlayerSimpleBufferQueueCallback(
            SLAndroidSimpleBufferQueueItf queueItf,
            void *pContext);
    void PlayerSimpleBufferQueueCallbackHandler(
            SLAndroidSimpleBufferQueueItf queueItf);
    

    // Delay updates
    void UpdatePlayoutDelay(int nSamplePlayed);

    // Init
    int InitSampleRate();
	
	int RequestPlayoutData(int sample,  char* playBuffer);
	int _playOutBufferIndex;
	char _playOutBuffer[(N_PLAY_SAMPLES_PER_SEC/100)*2*N_PALYOUT_BUFFER_LEN];
    void (*_ptrAudioBuffer)(void *opaque, char *stream, int len);//AudioDeviceBuffer* _ptrAudioBuffer;
	void *_opaque;
	
    LYH_Mutex* _critSect;//CriticalSectionWrapper& _critSect;
    int _id;

    // audio unit
    SLObjectItf _slEngineObject;
	
    // playout device
    SLObjectItf _slPlayer;
	SLEngineItf _slEngine;
    SLPlayItf _slPlayerPlay;
    SLAndroidSimpleBufferQueueItf _slPlayerSimpleBufferQueue;
    SLObjectItf _slOutputMixObject;
    SLVolumeItf _slSpeakerVolume;

    // Playout buffer
	///每个播放缓冲队列最大能放16位 10 ms @ 48 kHz的声音
    char _playQueueBuffer[N_PLAY_QUEUE_BUFFERS][2* PLAY_BUF_SIZE_IN_SAMPLES];
    int _playQueueSeq;

    // States
    bool _initialized;
    bool _playing;
    bool _playIsInitialized;
    bool _speakerIsInitialized;

    // Warnings and errors
    unsigned short _playWarning;
    unsigned short _playError;

    // Delay
    unsigned short _playoutDelay;

    // The sampling rate to use with Audio Device Buffer
    int _adbSampleRate;
    // Stored device properties
    int _samplingRateIn; // Sampling frequency for Mic
    int _samplingRateOut; // Sampling frequency for Speaker
    int _maxSpeakerVolume; // The maximum speaker volume value
    int _minSpeakerVolume; // The minimum speaker volume value
    bool _loudSpeakerOn;

};


#endif  // AUDIO_DEVICE_OPENSLES_ANDROID_H
