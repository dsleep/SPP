// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPXAudio2.h"

#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapofx.h>

#pragma comment(lib,"xaudio2.lib")

SPP_OVERLOAD_ALLOCATORS

//-----------------------------------------------------------------------------
// Global defines
//-----------------------------------------------------------------------------
#define INPUTCHANNELS 1  // number of source channels
#define OUTPUTCHANNELS 8 // maximum number of destination channels supported in this sample

#define NUM_PRESETS 30

namespace SPP
{
	uint32_t GetXAudio2Version()
	{
		return 1;
	}

    struct XAudio2Device
    {
    private:
        bool bInitialized;

        // XAudio2
        IXAudio2 *pXAudio2;
        IXAudio2MasteringVoice* pMasteringVoice;
        IXAudio2SourceVoice* pSourceVoice;
        IXAudio2SubmixVoice* pSubmixVoice;
        //Microsoft::WRL::ComPtr<IUnknown> pVolumeLimiter;
        //Microsoft::WRL::ComPtr<IUnknown> pReverbEffect;
        //std::unique_ptr<uint8_t[]> waveData;

        // 3D
        X3DAUDIO_HANDLE x3DInstance;
        int nFrameToApply3DAudio;

        DWORD dwChannelMask;
        UINT32 nChannels;

        X3DAUDIO_DSP_SETTINGS dspSettings;
        X3DAUDIO_LISTENER listener;
        X3DAUDIO_EMITTER emitter;
        X3DAUDIO_CONE emitterCone;

        DirectX::XMFLOAT3 vListenerPos;
        DirectX::XMFLOAT3 vEmitterPos;
        float fListenerAngle;
        bool  fUseListenerCone;
        bool  fUseInnerRadius;
        bool  fUseRedirectToLFE;

        FLOAT32 emitterAzimuths[INPUTCHANNELS];
        FLOAT32 matrixCoefficients[INPUTCHANNELS * OUTPUTCHANNELS];

        std::vector<IXAudio2SourceVoice*> _voices;

    public:

        void Initialize()
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            UINT32 flags = 0;
#if defined(_DEBUG)
            flags |= XAUDIO2_DEBUG_ENGINE;
#endif
            hr = XAudio2Create(&pXAudio2, flags);

#if defined(_DEBUG)
            XAUDIO2_DEBUG_CONFIGURATION debug = {};
            debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
            debug.BreakMask = XAUDIO2_LOG_ERRORS;
            pXAudio2->SetDebugConfiguration(&debug, 0);
#endif

            //
  // Create a mastering voice
  //
            IXAudio2MasteringVoice* pMasteringVoice = nullptr;

            if (FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice)))
            {
                wprintf(L"Failed creating mastering voice: %#X\n", hr);
                pXAudio2->Release();
                pXAudio2 = nullptr;
                CoUninitialize();                
            }
        }
    };
	
}