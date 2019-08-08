// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "CaptureEngine.h"

ActivatableStaticOnlyFactory(CaptureEngineStaticsImpl);

typedef IAsyncOperationCompletedHandler<HSTRING*> IDeviceInformationOperationCompletedHandler;

inline HRESULT FindResolutionsFromMediaProperties(
    _In_ IVectorView<ABI::Windows::Media::MediaProperties::IMediaEncodingProperties*>* propertiesList,
    _Out_ ResolutionList *resolutionList) throw()
{
    NULL_CHK(propertiesList);
    NULL_CHK(resolutionList);

    UINT32 uSize = 0;
    IFR(propertiesList->get_Size(&uSize));
    for (UINT32 uIndex = 0; uIndex < uSize; uIndex++)
    {
        ComPtr<IMediaEncodingProperties> spProperties;
        ComPtr<IVideoEncodingProperties> spVideoProperties;
        ComPtr<IImageEncodingProperties> spImageProperties;
        UINT32 uWidth = 0;
        UINT32 uHeight = 0;
        UINT64 uint64PackedResolution;

        IFR(propertiesList->GetAt(uIndex, &spProperties));
        if (SUCCEEDED(spProperties.As(&spVideoProperties)))
        {
            IFR(spVideoProperties->get_Width(&uWidth));
            IFR(spVideoProperties->get_Height(&uHeight));
        }
        else if (SUCCEEDED(spProperties.As(&spImageProperties)))
        {
            IFR(spImageProperties->get_Width(&uWidth));
            IFR(spImageProperties->get_Height(&uHeight));
        }
        else
        {
            IFR(E_UNEXPECTED);
        }

        // Pack the width and height into an uint64
        uint64PackedResolution = Pack2UINT32AsUINT64(uWidth, uHeight);

        // If we haven't seen this resolution yet, add it to our resolution list
        ResolutionList::iterator itResolutionList;
        itResolutionList = resolutionList->find(uint64PackedResolution);
        if (itResolutionList == resolutionList->end())
        {
            auto success = resolutionList->emplace(uint64PackedResolution).second;
            IFR(success ? S_OK : E_OUTOFMEMORY);
            Log(Log_Level_Info, L"Resolution: %d x %d", uWidth, uHeight);
        }
    }

    return S_OK;
}

inline HRESULT FindFrameRateRangeFromMediaProperties(
    _In_ IVectorView<ABI::Windows::Media::MediaProperties::IMediaEncodingProperties*>* propertiesList,
    _Out_ UINT32* puMin,
    _Out_ UINT32* puMax) throw()
{
    NULL_CHK(propertiesList);
    NULL_CHK(puMin);
    NULL_CHK(puMax);

    UINT32 uSize = 0;
    IFR(propertiesList->get_Size(&uSize));
    if (uSize > 0)
    {
        UINT32 uMax = 0;
        for (UINT32 i = 0; i < uSize; ++i)
        {
            ComPtr<IMediaEncodingProperties> spProperties;
            IFR(propertiesList->GetAt(i, &spProperties));

            // Must be video properties
            ComPtr<IVideoEncodingProperties> spVideoProperties;
            IFR(spProperties.As(&spVideoProperties));

            ComPtr<IMediaRatio> spFrameRate;
            IFR(spVideoProperties->get_FrameRate(&spFrameRate));

            UINT32 uFrameRateNum = 0;
            IFR(spFrameRate->get_Numerator(&uFrameRateNum));
            UINT32 uFrameRateDenom = 0;
            IFR(spFrameRate->get_Denominator(&uFrameRateDenom));
            if (uFrameRateDenom > 0)
            {
                uFrameRateNum /= uFrameRateDenom;
            }

            if (uFrameRateNum > uMax)
            {
                uMax = uFrameRateNum;
            }
        }

       * puMin = 1;
       * puMax = uMax;
    }

    return S_OK;
}

inline HRESULT FindDeviceId(_In_ DeviceClass deviceClass, _Out_ HSTRING* deviceId)
{
    ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformationStatics> spDeviceInformationStatics;
    IFR(Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(),
        &spDeviceInformationStatics));

    ComPtr<IAsyncOperation<DeviceInformationCollection*>> spAsyncOperation;
    IFR(spDeviceInformationStatics->FindAllAsyncDeviceClass(deviceClass, &spAsyncOperation));

    // waiting until find all completes
    IFR(SyncWait<DeviceInformationCollection*>(spAsyncOperation.Get(), 1000));

    // get the list as an enumerable type
    ComPtr<IVectorView<DeviceInformation*>> devices;
    IFR(spAsyncOperation->GetResults(&devices));

    // get the list of devices
    UINT32 count = 0;
    IFR(devices->get_Size(&count));
    IFR(count > 0 ? S_OK : HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED));

    // get the first device info
    ComPtr<IDeviceInformation> deviceInfo;
    IFR(devices->GetAt(0, &deviceInfo));

    HString str;
    IFR(deviceInfo->get_Id(str.GetAddressOf()));

    *deviceId = str.Detach();

    return S_OK;
}

CaptureEngineImpl::CaptureEngineImpl()
    : _isInitialized(true)
    , _enableAudio(false)
    , _enableMrc(false)
    , _videoEffectAdded(false)
    , _audioEffectAdded(false)
    , _captureStarted(false)
    , _mediaCapture(nullptr)
    , _spSpatialCoordinateSystem(nullptr)
{
}

CaptureEngineImpl::~CaptureEngineImpl()
{
    Uninitialize();
}

HRESULT CaptureEngineImpl::RuntimeClassInitialize()
{
    return S_OK;
}

// IModule
_Use_decl_annotations_
HRESULT CaptureEngineImpl::get_IsInitialized(
    _Out_ boolean *initialized)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::get_IsInitialized()\n");

    NULL_CHK(initialized);

    auto lock = _lock.Lock();

    *initialized = _isInitialized;

    return S_OK;
};

_Use_decl_annotations_
HRESULT CaptureEngineImpl::Uninitialize(void)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::Uninitialize()\n");

    auto lock = _lock.Lock();

    if (!_isInitialized)
    {
        return S_OK;
    }

    _isInitialized = false;

    return Close();
}

// ICloseable
_Use_decl_annotations_
HRESULT CaptureEngineImpl::Close(void)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::Close()\n");

    if (nullptr == _mediaCapture)
    {
        return S_OK;
    }

    _captureStarted = false;

    LOG_RESULT(_mediaCapture->remove_Failed(_failedEventToken));
    LOG_RESULT(_mediaCapture->remove_RecordLimitationExceeded(_recordLimitExceededEventToken));

    ComPtr<IAsyncAction> spRemoveVideo;
    if (_videoEffectAdded)
    {
        LOG_RESULT(_mediaCapture->ClearEffectsAsync(MediaStreamType::MediaStreamType_VideoRecord, &spRemoveVideo));
    }

    ComPtr<IAsyncAction> spRemoveAudio;
    if (_audioEffectAdded)
    {
        LOG_RESULT(_mediaCapture->ClearEffectsAsync(MediaStreamType::MediaStreamType_Audio, &spRemoveAudio));
    }

    ComPtr<IClosable> spClosable;
    LOG_RESULT(_mediaCapture.As(&spClosable));

    if (nullptr != spClosable)
    {
        LOG_RESULT(spClosable->Close());
    }

    _mediaCapture.Reset();
    _mediaCapture = nullptr;

    return S_OK;
}

// ICaptureEngine
_Use_decl_annotations_
HRESULT CaptureEngineImpl::add_Closed(
    IClosedEventHandler* eventHandler,
    EventRegistrationToken* token)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::add_Closed()\n");

    NULL_CHK(eventHandler);
    NULL_CHK(token);

    auto lock = _lock.Lock();

    return _evtClosed.Add(eventHandler, token);
}
_Use_decl_annotations_
HRESULT CaptureEngineImpl::remove_Closed(
    EventRegistrationToken token)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::remove_Closed()\n");

    auto lock = _lock.Lock();

    return _evtClosed.Remove(token);
}


_Use_decl_annotations_
HRESULT CaptureEngineImpl::get_SpatialCoordinateSystem(
    ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem** ppCoordinateSystem)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::get_SpatialCoordinateSystem()\n");

    NULL_CHK(ppCoordinateSystem);

    auto lock = _lock.Lock();

    NULL_CHK_HR(_spSpatialCoordinateSystem.Get(), E_NOT_SET);

    return _spSpatialCoordinateSystem.CopyTo(ppCoordinateSystem);
}

_Use_decl_annotations_
HRESULT CaptureEngineImpl::put_SpatialCoordinateSystem(
    ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem* pCoordinateSystem)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::put_SpatialCoordinateSystem()\n");

    NULL_CHK(pCoordinateSystem);

    auto lock = _lock.Lock();

    _spSpatialCoordinateSystem = pCoordinateSystem;

    if (nullptr != _networkMediaSink)
    {
        _networkMediaSink->put_SpatialCoordinateSystem(_spSpatialCoordinateSystem.Get());
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT CaptureEngineImpl::InitAsync(
    boolean enableAudio,
    IAsyncAction** action)
{
    Log(Log_Level_Info, L"CaptureEngineImpl::InitAsync()\n");

    NULL_CHK(action);

    _enableAudio = enableAudio;

    ComPtr<CaptureEngineImpl> spThis(this);
    auto workItem =
        Microsoft::WRL::Callback<ABI::Windows::System::Threading::IWorkItemHandler>(
            [this, spThis](IAsyncAction* asyncAction) -> HRESULT
    {
        Wrappers::HString videoDeviceId, audioDeviceId;

        IFR(FindDeviceId(
            ABI::Windows::Devices::Enumeration::DeviceClass::DeviceClass_VideoCapture,
            videoDeviceId.GetAddressOf()));

        if (_enableAudio)
        {
            IFR(FindDeviceId(
                ABI::Windows::Devices::Enumeration::DeviceClass::DeviceClass_AudioCapture,
                audioDeviceId.GetAddressOf()));
        }

        // InitializationSetting
        Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCaptureInitializationSettings> initSettings;
        IFR(Windows::Foundation::ActivateInstance(
            Wrappers::HStringReference(RuntimeClass_Windows_Media_Capture_MediaCaptureInitializationSettings).Get(),
            &initSettings));
        IFR(initSettings->put_VideoDeviceId(videoDeviceId.Get()));
        IFR(initSettings->put_AudioDeviceId(_enableAudio ? audioDeviceId.Get() : nullptr));
        IFR(initSettings->put_StreamingCaptureMode(_enableAudio ? StreamingCaptureMode::StreamingCaptureMode_AudioAndVideo : StreamingCaptureMode::StreamingCaptureMode_Video));

        Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCaptureInitializationSettings2> initSettings2;
        IFR(initSettings.As(&initSettings2));
        IFR(initSettings2->put_MediaCategory(ABI::Windows::Media::Capture::MediaCategory::MediaCategory_Communications));

        // IMediaCapture activation
        Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCapture> mediaCapture;
        IFR(Windows::Foundation::ActivateInstance(
            Wrappers::HStringReference(RuntimeClass_Windows_Media_Capture_MediaCapture).Get(),
            &mediaCapture));

        // initialize mediaCapture
        ComPtr<IAsyncAction> initAsync;
        IFR(mediaCapture->InitializeWithSettingsAsync(initSettings.Get(), &initAsync));
        IFR(SyncWait<void>(initAsync.Get()));

        // set controller hints
        ComPtr<ABI::Windows::Media::Devices::IVideoDeviceController> videoDeviceController;
        IFR(mediaCapture->get_VideoDeviceController(&videoDeviceController));

        // lower latency over quality
        ComPtr<ABI::Windows::Media::Devices::IAdvancedVideoCaptureDeviceController4> advVideoController4;
        IFR(videoDeviceController.As(&advVideoController4));
        IFR(advVideoController4->put_DesiredOptimization(ABI::Windows::Media::Devices::MediaCaptureOptimization::MediaCaptureOptimization_LatencyThenQuality));

        // device control to get resolutions
        ComPtr<ABI::Windows::Media::Devices::IMediaDeviceController> mediaDeviceController;
        IFR(videoDeviceController.As(&mediaDeviceController));

        // get list of source resolutionss
        ComPtr<IVectorView<ABI::Windows::Media::MediaProperties::IMediaEncodingProperties*>> encProperties;
        IFR(mediaDeviceController->GetAvailableMediaStreamProperties(MediaStreamType::MediaStreamType_VideoRecord, &encProperties));
        IFR(FindResolutionsFromMediaProperties(encProperties.Get(), &_videoResolutions));

        // store the capture engine and return
        return (mediaCapture.As(&_mediaCapture));
    });

    return PluginManagerStaticsImpl::GetThreadPool()->RunAsync(workItem.Get(), action);
}

_Use_decl_annotations_
HRESULT CaptureEngineImpl::StartAsync(
    boolean enableMrc,
    IConnection *connection,
    IAsyncAction** action)
{
    NULL_CHK(connection);

    ComPtr<ICaptureEngine> spThis(this);
    ComPtr<IConnection> spConnection(connection);
    auto workItem = Callback<ABI::Windows::System::Threading::IWorkItemHandler>(
        [this, spThis, spConnection, enableMrc](IAsyncAction* asyncAction) -> HRESULT
    {
        // encoding profile activation
        ComPtr<IMediaEncodingProfileStatics> spEncodingProfileStatics;
        IFR(Windows::Foundation::GetActivationFactory(
            Wrappers::HStringReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingProfile).Get(),
            &spEncodingProfileStatics));

        ComPtr<IMediaEncodingProfile> mediaEncodingProfile;
        IFR(spEncodingProfileStatics->CreateMp4(
            ABI::Windows::Media::MediaProperties::VideoEncodingQuality_HD720p,
            &mediaEncodingProfile));

        // remove unwanted parts of the profile
        if (!_enableAudio)
        {
            mediaEncodingProfile->put_Audio(nullptr);
        }
        mediaEncodingProfile->put_Container(nullptr);

        // get the audio/video profiles
        ComPtr<IAudioEncodingProperties> audioEncodingProperties;
        IFR(mediaEncodingProfile->get_Audio(&audioEncodingProperties));

        ComPtr<IVideoEncodingProperties> videoEncodingProperties;
        IFR(mediaEncodingProfile->get_Video(&videoEncodingProperties));

        // create the custome sink
        ComPtr<NetworkMediaSinkImpl> networkSink;
        IFR(Microsoft::WRL::Details::MakeAndInitialize<NetworkMediaSinkImpl>(
            &networkSink,
            audioEncodingProperties.Get(),
            videoEncodingProperties.Get(),
            spConnection.Get()));

        // if we want MRC, enable that now
        if (enableMrc)
        {
            ComPtr<IMediaCapture4> spMediaCapture4;
            IFR(_mediaCapture.As(&spMediaCapture4));

            ComPtr<MixedRemoteViewCompositor::Media::MrcVideoEffectDefinitionImpl> mrcVideoEffectDefinition;
            IFR(MakeAndInitialize<MrcVideoEffectDefinitionImpl>(&mrcVideoEffectDefinition));

            // set properties
            IFR(mrcVideoEffectDefinition->put_StreamType(MediaStreamType_VideoRecord));
            IFR(mrcVideoEffectDefinition->put_HologramComposition(true));
            IFR(mrcVideoEffectDefinition->put_VideoStabilization(false));
            IFR(mrcVideoEffectDefinition->put_GlobalOpacityCoefficient(.9f));
            IFR(mrcVideoEffectDefinition->put_RecordingIndicatorEnabled(true));

            ComPtr<ABI::Windows::Media::Effects::IVideoEffectDefinition> videoEffectDefinition;
            IFR(mrcVideoEffectDefinition.As(&videoEffectDefinition));

            ComPtr<IAsyncOperation<IMediaExtension*>> asyncAddEffect;
            IFR(spMediaCapture4->AddVideoEffectAsync(videoEffectDefinition.Get(), MediaStreamType_VideoRecord, &asyncAddEffect));
            IFR(SyncWait<IMediaExtension*>(asyncAddEffect.Get()));

            ComPtr<IMediaExtension> mediaExtension;
            IFR(asyncAddEffect->GetResults(&mediaExtension));

            _videoEffectAdded = true;

            if (_enableAudio)
            {
                ComPtr<MixedRemoteViewCompositor::Media::MrcAudioEffectDefinitionImpl> mrcAudioEffectDefinition;
                MakeAndInitialize<MrcAudioEffectDefinitionImpl>(&mrcAudioEffectDefinition);

                IFR(mrcAudioEffectDefinition->put_MixerMode(AudioMixerMode_MicAndLoopback));

                ComPtr<ABI::Windows::Media::Effects::IAudioEffectDefinition> audioEffectDefinition;
                IFR(mrcAudioEffectDefinition.As(&audioEffectDefinition));
                IFR(spMediaCapture4->AddAudioEffectAsync(audioEffectDefinition.Get(), &asyncAddEffect));

                IFR(SyncWait<IMediaExtension*>(asyncAddEffect.Get(), 1000));

                ComPtr<IMediaExtension> mediaExtension;
                IFR(asyncAddEffect->GetResults(&mediaExtension));

                _audioEffectAdded = true;
            }

            _enableMrc = enableMrc;
        }

        // StartRecordToCustomSinkAsync
        ComPtr<IMediaExtension> spMediaExtension;
        IFR(networkSink.As(&spMediaExtension));

        // subscribe to events
        auto failedEventCallback =
            Callback<ABI::Windows::Media::Capture::IMediaCaptureFailedEventHandler, CaptureEngineImpl>(this, &CaptureEngineImpl::OnMediaCaptureFailed);
        EventRegistrationToken failedToken;
        IFR(_mediaCapture->add_Failed(failedEventCallback.Get(), &failedToken));

        auto recordLimiteExceededEventCallback =
            Callback<ABI::Windows::Media::Capture::IRecordLimitationExceededEventHandler, CaptureEngineImpl>(this, &CaptureEngineImpl::OnRecordLimitationExceeded);
        EventRegistrationToken recordLimitExceededToken;
        IFR(_mediaCapture->add_RecordLimitationExceeded(recordLimiteExceededEventCallback.Get(), &recordLimitExceededToken));

        auto startRecordAsync = Callback<IAsyncActionCompletedHandler>(
            [this, spThis, networkSink](_In_ IAsyncAction *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
        {
            _captureStarted = true;

            _networkMediaSink = networkSink;

            _networkMediaSink->put_SpatialCoordinateSystem(_spSpatialCoordinateSystem.Get());

            return S_OK;
        });

        ComPtr<IAsyncAction> spStartRecordOperation;
        IFR(_mediaCapture->StartRecordToCustomSinkAsync(mediaEncodingProfile.Get(), spMediaExtension.Get(), &spStartRecordOperation));
        IFR(spStartRecordOperation->put_Completed(startRecordAsync.Get()));

        _failedEventToken = failedToken;
        _recordLimitExceededEventToken = recordLimitExceededToken;

        return S_OK;
    });

    return PluginManagerStaticsImpl::GetThreadPool()->RunAsync(std::move(workItem).Get(), action);
}

_Use_decl_annotations_
HRESULT CaptureEngineImpl::StopAsync(
    IAsyncAction** action)
{
    if (!_captureStarted)
    {
        return S_OK;
    }

    ComPtr<CaptureEngineImpl> spThis(this);
    auto workItem = Callback<ABI::Windows::System::Threading::IWorkItemHandler>(
        [this, spThis](IAsyncAction* asyncAction) -> HRESULT
    {
        auto stopAsyncCB = Callback<IAsyncActionCompletedHandler>(
            [this, spThis](_In_ IAsyncAction *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
        {
            auto lock = _lock.Lock();

            return Close();
        });

        auto lock = _lock.Lock();

        if (nullptr != _mediaCapture)
        {
            ComPtr<IAsyncAction> spStopAsync;
            if SUCCEEDED(_mediaCapture->StopRecordAsync(&spStopAsync))
            {
                LOG_RESULT(spStopAsync->put_Completed(stopAsyncCB.Get()));
            }
        }

        return S_OK;
    });

    return PluginManagerStaticsImpl::GetThreadPool()->RunAsync(workItem.Get(), action);
}

_Use_decl_annotations_
HRESULT CaptureEngineImpl::OnMediaCaptureFailed(
    IMediaCapture *mediaCapture,
    IMediaCaptureFailedEventArgs* capturedFailedArgs)
{
    Wrappers::HString errMessage;
    IFR(capturedFailedArgs->get_Message(errMessage.GetAddressOf()));

    ComPtr<ICaptureEngine> spThis(this);
    return _evtClosed.InvokeAll(spThis.Get());
}

_Use_decl_annotations_
HRESULT CaptureEngineImpl::OnRecordLimitationExceeded(
    IMediaCapture *mediaCapture)
{
    ComPtr<ICaptureEngine> spThis(this);
    return _evtClosed.InvokeAll(spThis.Get());
}


// Factory method
_Use_decl_annotations_
HRESULT CaptureEngineStaticsImpl::CreateAsync(
    boolean enableAudio,
    IAsyncOperation<CaptureEngine*>** ppAsyncOp)
{
    NULL_CHK(ppAsyncOp);
 
    // create capture and kick off the init
    ComPtr<ICaptureEngine> spCaptureEngine;
    IFR(MakeAndInitialize<CaptureEngineImpl>(&spCaptureEngine));

    ComPtr<IAsyncAction> spInitAsync;
    IFR(spCaptureEngine->InitAsync(enableAudio, &spInitAsync));

    ComPtr<CreateCaptureEngineAsync> createEngineAsyncOp = Make<CreateCaptureEngineAsync>();
    IFR(createEngineAsyncOp.CopyTo(ppAsyncOp));

    ComPtr<CaptureEngineStaticsImpl> spThis(this);
    return StartAsyncThen(
        spInitAsync.Get(),
        [this, spThis, spCaptureEngine, createEngineAsyncOp](_In_ HRESULT hr, _In_ IAsyncAction *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
    {
        LOG_RESULT(createEngineAsyncOp->SetCaptureEngineComplete(hr, spCaptureEngine.Get()));

        return S_OK;
    });
}
