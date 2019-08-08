// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "PlaybackEngine.h"
#include <WindowsNumerics.h>

static HRESULT ConvertMFVideoInterlaceModeToD3D(
    UINT mode, 
    D3D11_VIDEO_FRAME_FORMAT& d3dFormat)
{
    HRESULT hr = S_OK;

    switch (mode)
    {
    case MFVideoInterlace_Unknown:
        hr = E_UNEXPECTED;
        break;
    case MFVideoInterlace_Progressive:
        d3dFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        break;
    case MFVideoInterlace_FieldInterleavedUpperFirst:
    case MFVideoInterlace_FieldSingleUpper:
        d3dFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
        break;
    case MFVideoInterlace_FieldInterleavedLowerFirst:
    case MFVideoInterlace_FieldSingleLower:
        d3dFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
        break;
    case MFVideoInterlace_MixedInterlaceOrProgressive:
        hr = E_UNEXPECTED;
        break;
    default:
        hr = E_UNEXPECTED;
    }

    return hr;
}

//ActivatableClass(PlaybackEngineImpl);
ActivatableStaticOnlyFactory(PlaybackEngineStaticsImpl);

_Use_decl_annotations_
PlaybackEngineImpl::PlaybackEngineImpl()
    : _isInitialized(false)
    , _playbackStarted(false)
    , _waitForFirstVideoSample(true)
    , _sourceReader(nullptr)
    , _dxgiManager(nullptr)
    , _videoDevice(nullptr)
    , _videoContext(nullptr)
    , _videoWidth(0)
    , _videoHeight(0)
{
}

_Use_decl_annotations_
PlaybackEngineImpl::~PlaybackEngineImpl()
{
    Uninitialize();
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::RuntimeClassInitialize(
    IConnection* pConnection)
{
    Log(Log_Level_Info, L"PlaybackEngineImpl::RuntimeClassInitialize()\n");

    NULL_CHK(pConnection);

    // get the created d3d devices
    ComPtr<PluginManagerImpl> spManager = PluginManagerStaticsImpl::GetInstance();
    NULL_CHK_HR(spManager, E_POINTER);

    ComPtr<IDirectXManager> spResources;
    IFR(spManager->get_DirectXManager(&spResources));
    if (nullptr == spResources)
    {
        IFR(MakeAndInitialize<DirectXManagerImpl>(&spResources, nullptr));
    }

    IFR(spResources.As(&_dxManager));

    IFR(Start());

    // create media source
    ComPtr<IConnection> spConnection(pConnection);

    // create source / asyncAction
    ComPtr<IMFMediaSource> spMediaSource;
    IFR(MakeAndInitialize<NetworkMediaSourceImpl>(&spMediaSource, spConnection.Get()));

    ComPtr<IAsyncAction> spInitSourceAction;
    IFR(spMediaSource.As(&spInitSourceAction));

    // setup callback, only when the source gets the describe message, is it valid
    ComPtr<IPlaybackEngine> spThis(this);
    auto sourceReady = Callback<IAsyncActionCompletedHandler>(
        [this, spThis, spMediaSource](_In_ IAsyncAction *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
    {
        NULL_CHK(asyncResult);

        HANDLE dxgiHandle = nullptr;
        ComPtr<ID3D11Device2> spMFDevice;
        ComPtr<ID3D11Device> d3dDevice;
        ComPtr<ID3D10Multithread> spMultithread;
        ComPtr<ID3D11DeviceContext> spD3DDeviceContext;
        ComPtr<ID3D11VideoContext> spVideoContext;
        ComPtr<IMFAttributes> spAttributes;
        ComPtr<IMFMediaType> videoMediaType;
        ComPtr<IUnknown> spThisUnk;

        HRESULT hr = S_OK;

        ComPtr<IAsyncAction> spAction(asyncResult);
        if (AsyncStatus::Completed != asyncStatus)
        {
            ComPtr<IAsyncInfo> spInfo;
            IFR(spAction.As(&spInfo));
            spInfo->get_ErrorCode(&hr);
            IFR(hr);
        }

        DirectXManagerImpl* pDxImpl = static_cast<DirectXManagerImpl*>(_dxManager.Get());
        NULL_CHK_HR(pDxImpl, E_POINTER);

        spMFDevice = pDxImpl->GetDevice2();
        IFC(nullptr != spMFDevice ? S_OK : E_UNEXPECTED);

        // create DXGI manager - DXVA
        UINT resetToken;
        IFC(MFCreateDXGIDeviceManager(&resetToken, &_dxgiManager));

        // Set the d3d device
        IFC(_dxgiManager->ResetDevice(spMFDevice.Get(), resetToken));

        IFC(_dxgiManager->OpenDeviceHandle(&dxgiHandle));

        IFC(_dxgiManager->GetVideoService(dxgiHandle, IID_PPV_ARGS(&_videoDevice)));

        IFC(_dxgiManager->GetVideoService(dxgiHandle, IID_PPV_ARGS(&d3dDevice)));

        if (SUCCEEDED(d3dDevice.As(&spMultithread)))
        {
            spMultithread->SetMultithreadProtected(TRUE);
        }

        d3dDevice->GetImmediateContext(&spD3DDeviceContext);
        if (nullptr == spD3DDeviceContext)
        {
            IFC(E_UNEXPECTED);
        }

        IFC(spD3DDeviceContext.As(&_videoContext));

        // setup attributes for reader
        IFC(MFCreateAttributes(&spAttributes, 1));

        IFC(spAttributes->SetUINT32(MF_LOW_LATENCY, TRUE));

        IFC(spAttributes->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, FALSE));

        IFC(spAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));

        IFC(spAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE));

        IFC(spAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE));

        IFC(spThis.As(&spThisUnk));

        IFC(spAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, spThisUnk.Get()));

        // support IMFSOURCEEX/EX2
        IFC(spAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, (IUnknown*)(_dxgiManager.Get())));

        IFC(spAttributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown*)_dxgiManager.Get()));

        // DXGI_FORMAT_R8G8_B8G8_UNORM;
        IFC(spAttributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM));

        // create source reader
        IFC(MFCreateSourceReaderFromMediaSource(spMediaSource.Get(), spAttributes.Get(), &_sourceReader));

        // config the source for Video only
        // TODO: support audio
        IFC(_sourceReader->GetNativeMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &videoMediaType));

        // convert output format to decode video frame
        IFC(videoMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));

        IFC(_sourceReader->SetCurrentMediaType(0, NULL, videoMediaType.Get()));

        UINT32 width, height = 0;
        IFC(MFGetAttributeSize(videoMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height));

        // store the size of frame
        _videoWidth = width;
        _videoHeight = height;

        _waitForFirstVideoSample = true;

        _isInitialized = true;

    done:
        // close the handle
        if (nullptr != dxgiHandle)
        {
            _dxgiManager->CloseDeviceHandle(dxgiHandle);
        }

        // if we didn't complete the initialization, clean-up
        if (!_isInitialized)
        {
            Uninitialize();
        }

        return CompleteAsyncAction(hr);
    });

    return spInitSourceAction->put_Completed(sourceReady.Get());
}


// IModule
_Use_decl_annotations_
HRESULT PlaybackEngineImpl::get_IsInitialized(
    boolean* initialized)
{
    Log(Log_Level_Info, L"PlaybackEngineImpl::get_IsInitialized()\n");

    NULL_CHK(initialized);

    auto lock = _lock.Lock();

    *initialized = _isInitialized;

    return S_OK;
};

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::Uninitialize(void)
{
    Log(Log_Level_Info, L"PlaybackEngineImpl::Uninitialize()\n");
    auto lock = _lock.Lock();

    if (!_isInitialized)
    {
        return S_OK;
    }

    if (nullptr != _sourceReader)
    {
        StopPlayback();
        _sourceReader = nullptr;
    }

    // DX11 texture
    _dxgiManager = nullptr;
    _videoDevice = nullptr;
    _videoContext = nullptr;

    _isInitialized = false;

    return S_OK;
}

HRESULT PlaybackEngineImpl::Close(void)
{
    return S_OK;
}


HRESULT PlaybackEngineImpl::OnReadSample(
    HRESULT hrStatus,
    DWORD dwStreamIndex,
    DWORD dwStreamFlags,
    LONGLONG llTimestamp,
    IMFSample* pSample)
{
    LOG_RESULT(hrStatus);

    if (dwStreamFlags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM))
    {
        _playbackStarted = false;
    }

    // since we don't render, store the data
    _latestMediaSample[dwStreamIndex].Sample = pSample;
    _latestMediaSample[dwStreamIndex].StreamFlags = dwStreamFlags;
    _latestMediaSample[dwStreamIndex].Timestamp = llTimestamp;

    if (_waitForFirstVideoSample)
    {
        ComPtr<FormatChangedEventArgsImpl> args = Make<FormatChangedEventArgsImpl>(_videoWidth, _videoHeight);

        NULL_CHK_HR(args, E_OUTOFMEMORY);

        IFR(_evtFormatChanged.InvokeAll(args.Get()));

        _waitForFirstVideoSample = false;
    }

    static USHORT retryCount = MaxRetryAmount;

done:
    if (_playbackStarted)
    {
        // Read another sample, whatever happens. Just keep reading.
        if (retryCount > 0)
        {
            HRESULT hr = RequestNextSampleAsync(dwStreamIndex);
            if (FAILED(hr))
            {
                --retryCount;
                LOG_RESULT(hr);
                goto done;
            }
        }
        else
        {
            StopPlayback();

            ComPtr<IPlaybackEngine> spThis(this);
            return _evtClosed.InvokeAll(spThis.Get());
        }
    }

    return S_OK;
}

HRESULT PlaybackEngineImpl::OnFlush(
    DWORD dwStreamIndex)
{
    return S_OK;
}

HRESULT PlaybackEngineImpl::OnEvent(
    DWORD dwStreamIndex,
    IMFMediaEvent* pEvent)
{
    return S_OK;
}


// IPlaybackEgnine 
_Use_decl_annotations_
HRESULT PlaybackEngineImpl::add_Closed(
    IClosedEventHandler* delegateInterface,
    EventRegistrationToken* token)
{
    NULL_CHK(delegateInterface);
    NULL_CHK(token);

    return _evtClosed.Add(delegateInterface, token);
}
_Use_decl_annotations_
HRESULT PlaybackEngineImpl::remove_Closed(
    EventRegistrationToken token)
{
    return _evtClosed.Remove(token);
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::add_FormatChanged(
    IFormatChangedEventHandler* pEventHandler,
    EventRegistrationToken* pToken)
{
    return _evtFormatChanged.Add(pEventHandler, pToken);
}
_Use_decl_annotations_
HRESULT PlaybackEngineImpl::remove_FormatChanged(
    EventRegistrationToken token)
{
    return _evtFormatChanged.Remove(token);
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::add_SampleUpdated(
    ISampleUpdatedEventHandler* pEventHandler,
    EventRegistrationToken* pToken)
{
    return _evtSampleUpdated.Add(pEventHandler, pToken);
}
_Use_decl_annotations_
HRESULT PlaybackEngineImpl::remove_SampleUpdated(
    EventRegistrationToken token)
{
    return _evtSampleUpdated.Remove(token);
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::InitializeAsync(
    IAsyncAction** ppAsyncAction)
{
    NULL_CHK(ppAsyncAction);

    auto lock = _lock.Lock();

   * ppAsyncAction = this;

    return S_OK;
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::StartPlayback()
{
    auto lock = _lock.Lock();

    NULL_CHK_HR(_sourceReader, E_NOT_SET);

	IFR(RequestNextSampleAsync(MF_SOURCE_READER_ANY_STREAM));
    //IFR(RequestNextSampleAsync(MF_SOURCE_READER_FIRST_VIDEO_STREAM));

    //IFR(RequestNextSampleAsync(MF_SOURCE_READER_FIRST_AUDIO_STREAM));

    _playbackStarted = true;

    return S_OK;
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::StopPlayback()
{
    auto lock = _lock.Lock();

    if (nullptr == _sourceReader)
    {
        return S_OK;
    }

    CompleteAsyncAction(MF_E_SHUTDOWN);

    _sourceReader.Reset();
    _sourceReader == nullptr;

    _playbackStarted = false;

    _waitForFirstVideoSample = true;

    return S_OK;
}


_Use_decl_annotations_
HRESULT PlaybackEngineImpl::CompleteAsyncAction(HRESULT hr)
{
    Log(Log_Level_Info, L"::CompleteAsyncAction()\n");

    ABI::Windows::Foundation::AsyncStatus status;
    IFC(get_Status(&status));

    if (AsyncStatus::Completed == status)
    {
        return S_OK;
    }

    if (AsyncStatus::Started != status)
    {
        IFC(hr);

        IFC(get_ErrorCode(&hr));
    }

done:
    if (FAILED(hr))
    {
        LOG_RESULT(TryTransitionToError(hr));
    }

    return FireCompletion();
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::RequestNextSampleAsync(DWORD streamId)
{
    if (nullptr == _sourceReader)
    {
        return MF_E_SHUTDOWN;
    }

    return _sourceReader->ReadSample(streamId, 0, NULL, NULL, NULL, NULL);
}

_Use_decl_annotations_
HRESULT PlaybackEngineImpl::GetFrameData(MediaSampleArgs* pSampleargs)
{
    NULL_CHK(pSampleargs);
    NULL_CHK(pSampleargs->videoTexture);

    HRESULT hr = MF_E_CAPTURE_NO_SAMPLES_IN_QUEUE;

    ComPtr<IMFSample> spSample;

    if (nullptr != _latestMediaSample[0].Sample)
    {
        auto lock = _lock.Lock();

        pSampleargs->timestamp = _latestMediaSample[0].Timestamp;
        IFR(_latestMediaSample[0].Sample.As(&spSample));
    }
    else
    {
        return E_NOT_SET;
    }

    if (!_isInitialized)
    {
        hr = E_NOT_VALID_STATE;
        goto done;
    }

    if (_waitForFirstVideoSample)
    {
        ComPtr<FormatChangedEventArgsImpl> args = Make<FormatChangedEventArgsImpl>(_videoWidth, _videoHeight);

        NULL_CHK_HR(args, E_OUTOFMEMORY);

        IFR(_evtFormatChanged.InvokeAll(args.Get()));

        _waitForFirstVideoSample = false;
    }

    if (nullptr != spSample)
    {
        DirectXManagerImpl* pDxImpl = static_cast<DirectXManagerImpl*>(_dxManager.Get());
        NULL_CHK_HR(pDxImpl, E_POINTER);

        ComPtr<ID3D11DeviceContext> context;
        context = pDxImpl->GetDeviceContext();
        if (nullptr == context)
        {
            hr = E_POINTER;
            goto done;
        }

        ComPtr<IMFMediaBuffer> spBuffer;
        hr = spSample.Get()->GetBufferByIndex(0, &spBuffer);
        if (FAILED(hr))
        {
            goto done;
        }

        byte* srcBuffer = nullptr;
        hr = spBuffer->Lock(&srcBuffer, nullptr, nullptr);
        if (SUCCEEDED(hr))
        {
            if (nullptr != srcBuffer)
            {
                ID3D11Texture2D* pTexture =
                    static_cast<ID3D11Texture2D*>(pSampleargs->videoTexture);
                if (nullptr != pTexture)
                {
                    D3D11_TEXTURE2D_DESC desc;
                    pTexture->GetDesc(&desc);

                    int stride = desc.Width * 4;

                    context->UpdateSubresource(pTexture, 0, nullptr, srcBuffer, stride, 0);
                }
            }

            spBuffer->Unlock();
        }

        if (SUCCEEDED(hr))
        {
            // pass data onto caller object
            pSampleargs->width = _videoWidth;
            pSampleargs->height = _videoHeight;

            LONGLONG timestamp;
            HRESULT hr = spSample->GetSampleTime(&timestamp);
            if (SUCCEEDED(hr))
            {
                pSampleargs->timestamp = timestamp;
            }

            UINT32 blobSize;

            // get sample matrix data
            using float4x4 = Windows::Foundation::Numerics::float4x4;

            if (SUCCEEDED(hr))
            {
                float4x4 cameraViewTransform;;
                hr = spSample->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraViewTransform, sizeof(cameraViewTransform), &blobSize);
                if (SUCCEEDED(hr))
                {
                    hr = spSample->GetBlob(Spatial_CameraTransform, (UINT8*)&pSampleargs->cameraCoordinate, sizeof(pSampleargs->cameraCoordinate), &blobSize);

                    float4x4 viewCameraTransform;
                    if (SUCCEEDED(hr) && Windows::Foundation::Numerics::invert(cameraViewTransform, &viewCameraTransform))
                    {
                        pSampleargs->cameraViewTransform = viewCameraTransform * pSampleargs->cameraCoordinate;
                    }
                }
            }

            if (SUCCEEDED(hr))
            {
                float4x4 projection;
                hr = spSample->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&projection, sizeof(projection), &blobSize);
                if (SUCCEEDED(hr))
                {
                    pSampleargs->cameraProjection = projection;
                }
            }

            if (FAILED(hr))
            {
                pSampleargs->timestamp = 0;
                ZeroMemory(&pSampleargs->cameraViewTransform, sizeof(pSampleargs->cameraViewTransform));
                ZeroMemory(&pSampleargs->cameraProjection, sizeof(pSampleargs->cameraProjection));
                ZeroMemory(&pSampleargs->cameraCoordinate, sizeof(pSampleargs->cameraCoordinate));
                ZeroMemory(&pSampleargs->cameraAffine, sizeof(pSampleargs->cameraAffine));
            }
        }
    }

done:
    _latestMediaSample[0].StreamFlags = 0;
    _latestMediaSample[0].Timestamp = 0;
    _latestMediaSample[0].Sample.Reset();

    return hr;
}

_Use_decl_annotations_
HRESULT PlaybackEngineStaticsImpl::Create(
    IConnection* pConnection, 
    IPlaybackEngine** ppPlaybackEngine)
{
    NULL_CHK(pConnection);
    NULL_CHK(ppPlaybackEngine);

    ComPtr<IConnection> spConnection(pConnection);
    return MakeAndInitialize<PlaybackEngineImpl>(ppPlaybackEngine, spConnection.Get());
}
