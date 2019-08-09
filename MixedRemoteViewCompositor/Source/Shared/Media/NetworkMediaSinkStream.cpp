// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "NetworkMediaSinkStream.h"
#include "Media.h"

#define SET_SAMPLE_FLAG(dest, destMask, pSample, flagName) \
{ \
    UINT32 unValue; \
    if (SUCCEEDED(pSample->GetUINT32(MFSampleExtension_##flagName, &unValue))) \
    { \
        dest |= (unValue != FALSE) ? static_cast<DWORD>(SampleFlags_SampleFlag_##flagName) : 0; \
        destMask |= static_cast<DWORD>(SampleFlags_SampleFlag_##flagName); \
    } \
}

// If an entry is TRUE, the operation is valid from that state.
BOOL NetworkMediaSinkStreamImpl::ValidStateMatrix[SinkStreamState_Count][SinkStreamOperation_Count] =
{
    //            Operations:
    // States:    SetType  Start    Restart Pause   Stop    Sample  Marker
    /* NotSet */  TRUE,    FALSE,   FALSE,  FALSE,  FALSE,  FALSE,  FALSE,

    /* Ready */   TRUE,    TRUE,    FALSE,  TRUE,   TRUE,   FALSE,  TRUE,

    /* Start */   TRUE,    TRUE,    FALSE,  TRUE,   TRUE,   TRUE,   TRUE,

    /* Pause */   TRUE,    TRUE,    TRUE,   TRUE,   TRUE,   TRUE,   TRUE,

    /* Stop */    TRUE,    TRUE,    FALSE,  FALSE,  TRUE,   FALSE,  TRUE,
};

_Use_decl_annotations_
NetworkMediaSinkStreamImpl::AsyncOperation::AsyncOperation(SinkStreamOperation op)
    : _cRef(1)
    , _op(op)
{
}

_Use_decl_annotations_
NetworkMediaSinkStreamImpl::AsyncOperation::~AsyncOperation()
{
    assert(_cRef == 0);
}

_Use_decl_annotations_
ULONG NetworkMediaSinkStreamImpl::AsyncOperation::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

_Use_decl_annotations_
ULONG NetworkMediaSinkStreamImpl::AsyncOperation::Release()
{
    ULONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0)
    {
        delete this;
    }

    return cRef;
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::AsyncOperation::QueryInterface(REFIID iid, void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }
    if (iid == IID_IUnknown)
    {
       * ppv = static_cast<IUnknown*>(this);
    }
    else
    {
       * ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

_Use_decl_annotations_
NetworkMediaSinkStreamImpl::NetworkMediaSinkStreamImpl()
    : _dwStreamId(-1)
    , _state(SinkStreamState_NotSet)
    , _isShutdown(false)
    , _isPlayerConnected(false)
    , _fIsVideo(false)
    , _fGetFirstSampleTime(false)
    , _adjustedStartTime(0)
    , _spConnection(nullptr)
    , _spParentMediaSink(nullptr)
    , _workQueueId(0)
    , _workQueueCB(this, &NetworkMediaSinkStreamImpl::OnDispatchWorkItem)
{
    ZeroMemory(&_currentSubtype, sizeof(_currentSubtype));
}

_Use_decl_annotations_
NetworkMediaSinkStreamImpl::~NetworkMediaSinkStreamImpl()
{
    assert(_isShutdown);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::RuntimeClassInitialize(
    DWORD id, 
    IConnection* pConnection,
    INetworkMediaSink* pParentMediaSink)
{
    NULL_CHK(pConnection);
    NULL_CHK(pParentMediaSink);

    // Create the event queue helper.
    IFR(MFCreateEventQueue(&_eventQueue));
    IFR(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_STANDARD, &_workQueueId));

    _dwStreamId = id;
    _spConnection = pConnection;
    _spParentMediaSink = pParentMediaSink;

    return S_OK;
}

// IMFMediaEventGenerator methods.
// Note: These methods call through to the event queue helper object.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::BeginGetEvent(
    IMFAsyncCallback* pCallback, 
    IUnknown* punkState)
{
    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    return _eventQueue->BeginGetEvent(pCallback, punkState);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::EndGetEvent(
    IMFAsyncResult* pResult, 
    IMFMediaEvent** ppEvent)
{
    NULL_CHK(ppEvent);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    return _eventQueue->EndGetEvent(pResult, ppEvent);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetEvent(
    DWORD dwFlags, 
    IMFMediaEvent** ppEvent)
{
    NULL_CHK(ppEvent);

    // NOTE:
    // GetEvent can block indefinitely, so we don't hold the lock.
    // This requires some juggling with the event queue pointer.
    HRESULT hr = S_OK;

    ComPtr<IMFMediaEventQueue> spQueue;
    {
        auto lock = _lock.Lock();

        // Check shutdown
        IFR(CheckShutdown());

        // Get the pointer to the event queue.
        spQueue = _eventQueue;
    }

    // Now get the event.
    return spQueue->GetEvent(dwFlags, ppEvent);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::QueueEvent(
    MediaEventType met, 
    REFGUID guidExtendedType, 
    HRESULT hrStatus, 
    PROPVARIANT const* pvValue)
{
    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    return _eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}

// IMFStreamSink methods
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetMediaSink(
    IMFMediaSink** ppMediaSink)
{
    NULL_CHK(ppMediaSink);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    NULL_CHK_HR(_spParentMediaSink, E_NOT_SET);

    return _spParentMediaSink.CopyTo(ppMediaSink);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetIdentifier(
    DWORD* pdwIdentifier)
{
    NULL_CHK(pdwIdentifier);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

   * pdwIdentifier = _dwStreamId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetMediaTypeHandler(
    IMFMediaTypeHandler** ppHandler)
{
    NULL_CHK(ppHandler);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    // This stream object acts as its own type handler, so we QI ourselves.
    return QueryInterface(IID_IMFMediaTypeHandler, reinterpret_cast<void**>(ppHandler));
}

// We received a sample from an upstream component
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::ProcessSample(
    IMFSample* pSample)
{
    NULL_CHK(pSample);

    Log(Log_Level_All, L"NetworkMediaSinkStreamImpl::ProcessSample() begin...\n");

    auto lock = _lock.Lock();

    HRESULT hr = S_OK;

    IFC(CheckShutdown());

    // Validate the operation.
    IFC(ValidateOperation(SinkStreamOperation_ProcessSample));

    if(!_isPlayerConnected)
    {
        // we do not have a connected player yet
        Log(Log_Level_Info, L"Waiting for a connected player, ignoring sample...\n");

        //if (nullptr != _spConnection)
        //{
        //    IFR(_spConnection->SendPayloadType(PayloadType_State_CaptureReady));
        //}

        IFC(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, hr, nullptr));
    }
    else
    {
        // is this our first sample since connecting?
        if (_isPlayerConnected && _fGetFirstSampleTime)
        {
            // pulls the sample time to rebase timestamps
            IFC(pSample->GetSampleTime(&_adjustedStartTime));
            Log(Log_Level_Info, L"first sample timeStamp: %I64d\n", _adjustedStartTime);
            _fGetFirstSampleTime = false;
        }

        // Add the sample to the sample queue.
        IFC(_sampleQueue.InsertBack(pSample));

        // Unless we are paused, start an async operation to dispatch the next sample.
        if (SinkStreamState_Paused != _state)
        {
            // Queue the operation.
            IFC(QueueAsyncOperation(SinkStreamOperation_ProcessSample));
        }
    }

    //if (nullptr != _spParentMediaSink) // for local playback
    //{
    //    //_spParentMediaSink->OnSampleUpdated(pSample);
    //}

done:
    return hr;
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::PlaceMarker(
    MFSTREAMSINK_MARKER_TYPE eMarkerType, 
    PROPVARIANT const* pvarMarkerValue, 
    PROPVARIANT const* pvarContextValue)
{
    auto lock = _lock.Lock();

    IFR(ValidateOperation(SinkStreamOperation_PlaceMarker));

    ComPtr<IMarker> spMarker;
    IFR(MarkerImpl::Create(eMarkerType, pvarMarkerValue, pvarContextValue, &spMarker));

    _sampleQueue.InsertBack(spMarker.Get());

    if (SinkStreamState_Paused == _state)
    {
        return S_OK;
    }

    // Queue the operation.
    return QueueAsyncOperation(SinkStreamOperation_PlaceMarker); // Increments ref count on pOp.
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Flush()
{
    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    // Note: Even though we are flushing data, we still need to send
    // any marker events that were queued.
    return HandleError(ProcessSamplesFromQueue(true, nullptr));
}

/// IMFMediaTypeHandler methods
// Check if a media type is supported.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::IsMediaTypeSupported(
    IMFMediaType* pMediaType, 
    IMFMediaType** ppMediaType)
{
    NULL_CHK(pMediaType);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    GUID majorType = GUID_NULL;
    IFR(pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType));

    IFR((majorType == MFMediaType_Video || majorType == MFMediaType_Audio) ? S_OK : MF_E_INVALIDTYPE);

    if (nullptr != _currentType)
    {
        GUID guiNewSubtype = GUID_NULL;
        IFR(pMediaType->GetGUID(MF_MT_SUBTYPE, &guiNewSubtype));
        
        if(guiNewSubtype != _currentSubtype)
        {
            return MF_E_INVALIDTYPE;
        }
    }

    if (nullptr != ppMediaType)
    {
       * ppMediaType = nullptr;
    }

    return S_OK;
}

// Return the number of preferred media types.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetMediaTypeCount(
    DWORD* pdwTypeCount)
{
    NULL_CHK(pdwTypeCount);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    // We've have only one media type
   * pdwTypeCount = 1;

    return S_OK;
}

// Return a preferred media type by index.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetMediaTypeByIndex(
    DWORD dwIndex, 
    IMFMediaType** ppType)
{
    NULL_CHK(ppType);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    if (dwIndex > 0)
    {
        IFR(MF_E_NO_MORE_TYPES);
    }

    NULL_CHK_HR(_currentType, E_NOT_SET);

    return _currentType.CopyTo(ppType);
}

// Set the current media type.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::SetCurrentMediaType(
    IMFMediaType* pMediaType)
{
    NULL_CHK(pMediaType);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    // don't allow format changes after streaming starts.
    IFR(ValidateOperation(SinkStreamOperation_SetMediaType));

    // set media type already
    if (SinkStreamState_Ready <= _state)
    {
        IFR(IsMediaTypeSupported(pMediaType, nullptr));
    }

    GUID guiMajorType = GUID_NULL;
    IFR(pMediaType->GetMajorType(&guiMajorType));
    _fIsVideo = (guiMajorType == MFMediaType_Video);

    IFR(MFCreateMediaType(&_currentType));
    IFR(pMediaType->CopyAllItems(_currentType.Get()));
    IFR(_currentType->GetGUID(MF_MT_SUBTYPE, &_currentSubtype));
    if (_state < SinkStreamState_Ready)
    {
        _state = SinkStreamState_Ready;
    }
    else if (_state > SinkStreamState_Ready)
    {
        ComPtr<IMFMediaType> spType;
        IFR(MFCreateMediaType(&spType));
        IFR(pMediaType->CopyAllItems(spType.Get()));
        IFR(ProcessFormatChange(spType.Get()));
    }

    return S_OK;
}

// Return the current media type, if any.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetCurrentMediaType(
    IMFMediaType** ppMediaType)
{
    NULL_CHK(ppMediaType);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    if (nullptr == _currentType)
    {
        IFR(MF_E_NOT_INITIALIZED);
    }

    NULL_CHK_HR(_currentType, E_NOT_SET);

    return _currentType.CopyTo(ppMediaType);
}

// Return the major type GUID.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::GetMajorType(
    GUID* pguidMajorType)
{
    NULL_CHK(pguidMajorType);

    auto lock = _lock.Lock();

    IFR(CheckShutdown());

    if (nullptr == _currentType)
    {
        IFR(MF_E_NOT_INITIALIZED);
    }

   * pguidMajorType = (_fIsVideo) ? MFMediaType_Video : MFMediaType_Audio;

    return S_OK;
}

// Called when the presentation clock starts.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Start(MFTIME start)
{
    auto lock = _lock.Lock();

    IFR(ValidateOperation(SinkStreamOperation_Start));

    if (start != PRESENTATION_CURRENT_POSITION)
    {
        _adjustedStartTime = start; // Cache the start time
    }
    _state = SinkStreamState_Started;

    return QueueAsyncOperation(SinkStreamOperation_Start);
}

// Called when the presentation clock stops.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Stop()
{
    IFR(ValidateOperation(SinkStreamOperation_Stop));

    _state = SinkStreamState_Stopped;

    return QueueAsyncOperation(SinkStreamOperation_Stop);
}

// Called when the presentation clock pauses.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Pause()
{
    IFR(ValidateOperation(SinkStreamOperation_Pause));
    
    _state = SinkStreamState_Paused;
    
    return QueueAsyncOperation(SinkStreamOperation_Pause);
}

// Called when the presentation clock restarts.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Restart()
{
    IFR(ValidateOperation(SinkStreamOperation_Restart));

    _state = SinkStreamState_Started;

    return QueueAsyncOperation(SinkStreamOperation_Restart);
}

// Shuts down the stream sink.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::Shutdown()
{
    _isPlayerConnected = false;

    if (!_isShutdown)
    {
        if (_eventQueue)
        {
            _eventQueue->Shutdown();
        }

        MFUnlockWorkQueue(_workQueueId);

        _sampleQueue.Clear();

        _eventQueue.Reset();
        _currentType.Reset();

        _isShutdown = true;

        _spConnection.Reset();
        _spConnection = nullptr;
    }

    return S_OK;
}

// Set the information if we are connected to a client
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::ConnectedFunc(
    bool fConnected, 
    LONGLONG llCurrentTime)
{
    auto lock = _lock.Lock();

    HRESULT hr = S_OK;

    if (_currentType == nullptr)
    {
        IFC(MF_E_NOT_INITIALIZED);
    }

    _isPlayerConnected = fConnected;

    _adjustedStartTime = llCurrentTime;

    if (_isPlayerConnected)
    {
        _fGetFirstSampleTime = true;
    }

    if (fConnected)
    {
        Log(Log_Level_Info, L"SetConnected start=%I64d\n", _adjustedStartTime);
    }

done:
    return HandleError(hr);
}

// Puts an async operation on the work queue.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::QueueAsyncOperation(
    SinkStreamOperation op)
{
    ComPtr<IUnknown> spOp;
    spOp.Attach(new (std::nothrow) AsyncOperation(op)); // Created with ref count = 1
    NULL_CHK_HR(spOp.Get(), E_OUTOFMEMORY);

    return MFPutWorkItem2(_workQueueId, 0, &_workQueueCB, spOp.Get());
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::OnDispatchWorkItem(
    IMFAsyncResult* pAsyncResult)
{
    // Called by work queue thread. Need to hold the critical section.
    auto lock = _lock.Lock();

    Log(Log_Level_All, L"NetworkMediaSinkStreamImpl::OnDispatchWorkItem() begin...\n");

    HRESULT hr = S_OK;

    // The state object is a AsncOperation object.
    ComPtr<IUnknown> spState;
    IFR(pAsyncResult->GetState(&spState));

    AsyncOperation* pOp = static_cast<AsyncOperation *>(spState.Get());
    switch (pOp->_op)
    {
    case SinkStreamOperation_Start:
    case SinkStreamOperation_Restart:
        // Send MEStreamSinkStarted.
        IFC(QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, nullptr));

        // There might be samples queue from earlier (ie, while paused).
        boolean fRequestMoreSamples;
        if (!_isPlayerConnected)
        {
            // Just drop samples if we are not connected
            IFC(DropSamplesFromQueue(&fRequestMoreSamples));
        }
        else
        {
            IFC(SendSampleFromQueue(&fRequestMoreSamples));
        }

        // If false there is no samples in the queue now so request one
        if (fRequestMoreSamples)
        {
            IFC(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr));
        }
        break;

    case SinkStreamOperation_Stop:
        // Drop samples from queue.
        IFC(DropSamplesFromQueue(nullptr));

        // Send the event even if the previous call failed.
        IFC(QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, nullptr));
        break;

    case SinkStreamOperation_Pause:
        IFC(QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, nullptr));
        break;

    case SinkStreamOperation_ProcessSample:
    case SinkStreamOperation_PlaceMarker:
    case SinkStreamOperation_SetMediaType:
        IFC(DispatchProcessedSample(pOp));
        break;
    }

done:
    return HandleError(hr);
}

// Complete a ProcessSample or PlaceMarker request.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::DispatchProcessedSample(
    AsyncOperation* pOp)
{
    NULL_CHK(pOp);

    boolean fRequestMoreSamples = false;
    if (!_isPlayerConnected)
    {
        IFR(DropSamplesFromQueue(&fRequestMoreSamples));
    }
    else
    {
        IFR(SendSampleFromQueue(&fRequestMoreSamples));
    }

    // Ask for another sample
    if (fRequestMoreSamples)
    {
        if (pOp->_op == SinkStreamOperation_ProcessSample)
        {
            IFR(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr));
        }
    }

    return S_OK;
}

// Drop samples in the queue
HRESULT NetworkMediaSinkStreamImpl::DropSamplesFromQueue(
    boolean* fRequestMoreSamples)
{
    return ProcessSamplesFromQueue(true, fRequestMoreSamples);
}

// Send sample from the queue
HRESULT NetworkMediaSinkStreamImpl::SendSampleFromQueue(
    boolean* fRequestMoreSamples)
{
    return ProcessSamplesFromQueue(false, fRequestMoreSamples);
}

// Checks if an operation is valid in the current state.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::ValidateOperation(SinkStreamOperation op)
{
    assert(!_isShutdown);

    HRESULT hr = S_OK;

    if (NetworkMediaSinkStreamImpl::ValidStateMatrix[static_cast<int>(_state)][static_cast<int>(op)])
    {
        return S_OK;
    }
    else if (_state == SinkStreamState_NotSet)
    {
        return MF_E_NOT_INITIALIZED;
    }
    else
    {
        return MF_E_INVALIDREQUEST;
    }
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::ProcessSamplesFromQueue(
    boolean fFlush,
    boolean *fNeedMoreSamples)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::ProcessSamplesFromQueue()\n");

    if (FAILED(CheckShutdown()))
    {
        fFlush = true;
    }

    boolean fRequestSamples = false;
    boolean fSendSamples = true;
    boolean fSendEOS = false;

    ComPtr<IUnknown> spUnknown;
    if (FAILED(_sampleQueue.RemoveFront(&spUnknown)))
    {
        fRequestSamples = true;
        fSendSamples = false;
    }

    while (fSendSamples)
    {
        ComPtr<IDataBundle> spDataBundle;

        assert(spUnknown);

        bool fProcessingSample = false;

        // Determine if this is a marker or a sample.
        ComPtr<IMFSample> spMediaSample;
        if (SUCCEEDED(spUnknown.As(&spMediaSample)))
        {
            if (!fFlush)
            {
                IFR(PrepareSample(spMediaSample.Get(), false, &spDataBundle));
                fProcessingSample = true;
            }
        }
        else
        {
            // Check for marker
            ComPtr<IMarker> spMarker;
            if (SUCCEEDED(spUnknown.As(&spMarker)))
            {
                PROPVARIANT var;
                PropVariantInit(&var);

                MFSTREAMSINK_MARKER_TYPE markerType;
                HRESULT hr = spMarker->GetMarkerType(&markerType);
                if (SUCCEEDED(hr))
                {
                    // Get the context data.
                    hr = spMarker->GetContext(&var);
                    if (SUCCEEDED(hr))
                    {
                        hr = QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, &var);
                    }
                }

                PropVariantClear(&var);

                IFR(hr);

                switch (markerType)
                {
                case MFSTREAMSINK_MARKER_ENDOFSEGMENT:
                    fSendEOS = true;
                    break;
                case MFSTREAMSINK_MARKER_TICK:
                    if (!fFlush || (_isPlayerConnected && !_fGetFirstSampleTime))
                    {
                        PROPVARIANT var;
                        PropVariantInit(&var);

                        LARGE_INTEGER timeStamp;;
                        hr = spMarker->GetMarkerValue(&var);
                        if (SUCCEEDED(hr))
                        {
                            ZeroMemory(&timeStamp, sizeof(timeStamp));
                            if (var.vt == VT_I8)
                            {
                                timeStamp = var.hVal;
                            }
                        }

                        PropVariantClear(&var);

                        IFR(hr)

                        ComPtr<IMFSample> spSample;
                        IFR(MFCreateSample(&spSample));
                        IFR(spSample->SetSampleTime(timeStamp.QuadPart));
                        IFR(PrepareStreamTick(spSample.Get(), &spDataBundle));
                    }
                    break;
                }
            }
            else
            {
                ComPtr<IMFMediaType> spMediaType;
                IFR(spUnknown.As(&spMediaType));
                if (!fFlush && !_fGetFirstSampleTime)
                {
                    IFR(PrepareFormatChange(spMediaType.Get(), &spDataBundle));
                }
            }
        }

        if (nullptr != spDataBundle.Get())
        {
            ComPtr<IAsyncAction> spSendAction;
            if (FAILED(_spConnection->SendBundleAsync(spDataBundle.Get(), &spSendAction)))
            {
                fProcessingSample = false;
            }
            else
            {
                ComPtr<NetworkMediaSinkStreamImpl> spThis(this);
                IFR(StartAsyncThen(
                    spSendAction.Get(),
                    [this, spThis, fProcessingSample](_In_ HRESULT hr, _In_ IAsyncAction* pResult, _In_ AsyncStatus asyncStatus) -> HRESULT
                {
                    LOG_RESULT(hr);

                    if (_state == SinkStreamState_Started && fProcessingSample)
                    {
                        // If we are still in started state request another sample
                        IFR(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr));
                    }

                    return S_OK;
                }));
            }

            // We stop if we processed a sample otherwise keep looking
            fSendSamples = !fProcessingSample;
        }

        if (fSendSamples)
        {
            if (FAILED(_sampleQueue.RemoveFront(&spUnknown)))
            {
                fRequestSamples = true;
                fSendSamples = false;
            }
        }
    }

    if (fSendEOS)
    {
        ComPtr<NetworkMediaSinkStreamImpl> spThis(this);
        ComPtr<INetworkMediaSink> spParent(_spParentMediaSink.Get());
        concurrency::create_task([this, spThis, spParent]()
        {
            spParent->OnEndOfStream(_dwStreamId);
        });

        fSendSamples = false;
    }

    if (nullptr != fNeedMoreSamples)
    {
        if (FAILED(CheckShutdown()))
        {
            *fNeedMoreSamples = false;
        }
        else
        {
            *fNeedMoreSamples = !fSendSamples;
        }
    }

    return S_OK;
}

HRESULT NetworkMediaSinkStreamImpl::ProcessFormatChange(
    IMFMediaType* pMediaType)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::ProcessFormatChange()\n");

    NULL_CHK(pMediaType)

    // Add the media type to the sample queue.
    IFR(_sampleQueue.InsertBack(pMediaType));

    // Unless we are paused, start an async operation to dispatch the next sample.
    // Queue the operation.
    return QueueAsyncOperation(SinkStreamOperation_SetMediaType);
}

// Prepare bundle to send with frame sample data
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::PrepareSample(
    IMFSample* pSample, 
    bool fForce,
    IDataBundle** ppDataBundle)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::PrepareSample()\n");

    NULL_CHK(pSample);

    const size_t c_cPayloadHeader = sizeof(PayloadHeader);
    const size_t c_cMediaSampleHeader = sizeof(MediaSampleHeader);
    const size_t c_cbSampleHeaderSize = c_cPayloadHeader + c_cMediaSampleHeader;

    LONGLONG llSampleTime;
    IFR(pSample->GetSampleTime(&llSampleTime));

    LONGLONG llDuration;
    IFR(pSample->GetSampleDuration(&llDuration));

    llSampleTime -= _adjustedStartTime;

    if (llSampleTime < 0 && !fForce)
    {
        IFR(MF_E_LATE_SAMPLE);
    }

    if (llSampleTime < 0)
    {
        llSampleTime = 0;
    }

    DWORD cbTotalSampleLength = 0;
    IFR(pSample->GetTotalLength(&cbTotalSampleLength));

    // Create a bundle and initialize it with the sample
    ComPtr<IDataBundle> spBundle;
    IFR(MakeAndInitialize<DataBundleImpl>(&spBundle, pSample));

    // create a buffer for the media sample header
    ComPtr<IDataBuffer> spDataBuffer;
    IFR(MakeAndInitialize<DataBufferImpl>(&spDataBuffer, c_cbSampleHeaderSize));

    // Prepare the buffer
    ComPtr<IBuffer> spHeaderBuffer;
    IFR(spDataBuffer.As(&spHeaderBuffer));

    BYTE* pBuf = GetDataType<BYTE*>(spHeaderBuffer.Get());

    // populate the PayloadType header
    PayloadHeader* pOpHeader = reinterpret_cast<PayloadHeader*>(pBuf);
    ZeroMemory(pOpHeader, c_cPayloadHeader);

    // fill in the PayloadType header info
    pOpHeader->ePayloadType = PayloadType_SendMediaSample;
    pOpHeader->cbPayloadSize = c_cMediaSampleHeader + cbTotalSampleLength;

    // fill in the media sample header info
    MediaSampleHeader* pSampleHeader = reinterpret_cast<MediaSampleHeader *>(pBuf + c_cPayloadHeader);
    ZeroMemory(pSampleHeader, c_cMediaSampleHeader);
    GetIdentifier(&pSampleHeader->dwStreamId);
    pSampleHeader->hnsTimestamp = llSampleTime;
    pSampleHeader->hnsDuration = llDuration;

    // dwFlags and masks
    if (IsVideo())
    {
        Log(Log_Level_All, L"NetworkMediaSinkStreamImpl::PrepareSample: TS: %I64d, DUR: %I64d\n", pSampleHeader->hnsTimestamp, pSampleHeader->hnsDuration);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, BottomFieldFirst);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, CleanPoint);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, DerivedFromTopField);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, Discontinuity);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, Interlaced);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, RepeatFirstField);
        SET_SAMPLE_FLAG(pSampleHeader->dwFlags, pSampleHeader->dwFlagMasks, pSample, SingleField);

        pSampleHeader->cbCameraDataSize = 0;
        if (FAILED(ProcessCameraData(pSample, pSampleHeader)))
        {
            ZeroMemory(&pSampleHeader->worldToCameraMatrix, sizeof(Matrix4x4));
            ZeroMemory(&pSampleHeader->cameraProjectionTransform, sizeof(Matrix4x4));
            ZeroMemory(&pSampleHeader->cameraViewTransform, sizeof(Matrix4x4));
        }
    }

    // update the payload size to include additional buffer
    pOpHeader->cbPayloadSize += pSampleHeader->cbCameraDataSize;

    // set the size of the header buffer
    IFR(spDataBuffer->put_CurrentLength(c_cbSampleHeaderSize));

    // Put headers before the mediasample and camera data
    IFR(spBundle->InsertBuffer(0, spDataBuffer.Get()));

    NULL_CHK_HR(spBundle, E_NOT_SET);

    return spBundle.CopyTo(ppDataBundle);
}

_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::ProcessCameraData(
    IMFSample* pSample,
    MediaSampleHeader* pSampleHeader)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::ProcessCameraData()\n");

    NULL_CHK(pSample);
    NULL_CHK(pSampleHeader);
    NULL_CHK(_spParentMediaSink.Get());

    UINT32 blobSize = 0;

    using float4x4 = ABI::Windows::Foundation::Numerics::Matrix4x4;
    ABI::MixedRemoteViewCompositor::MFPinholeCameraIntrinsics cameraIntrinsics;
    float4x4 worldToCameraMatrix;
    float4x4 cameraProjectionTransform;
    float4x4 cameraViewTransform;

    // world coordinate system application is using
    using ISpatialCoordinateSystem = ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem;
    ISpatialCoordinateSystem* cameraCoordinateSystem = nullptr;
    HRESULT hr = _spParentMediaSink->get_SpatialCoordinateSystem(&cameraCoordinateSystem);
    if (SUCCEEDED(hr))
    {
        // cameraCoordinates
        ComPtr<ISpatialCoordinateSystem> spSampleCoordinateSystem;
        hr = pSample->GetUnknown(MFSampleExtension_Spatial_CameraCoordinateSystem, __uuidof(ISpatialCoordinateSystem), &spSampleCoordinateSystem);
        if (SUCCEEDED(hr))
        {
            ComPtr<ABI::Windows::Foundation::IReference<ABI::Windows::Foundation::Numerics::Matrix4x4>> transformReference;
            hr = spSampleCoordinateSystem->TryGetTransformTo(cameraCoordinateSystem, &transformReference);
            if (SUCCEEDED(hr) && transformReference != nullptr)
            {
                hr = transformReference->get_Value(
                reinterpret_cast<ABI::Windows::Foundation::Numerics::Matrix4x4*>(&worldToCameraMatrix));
            }
        }

        // intrinsics
        if (SUCCEEDED(hr))
        {
            hr = pSample->GetBlob(MFSampleExtension_PinholeCameraIntrinsics, (UINT8*)&cameraIntrinsics, sizeof(cameraIntrinsics), &blobSize);
        }

        // projection
        if (SUCCEEDED(hr))
        {
            hr = pSample->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&cameraProjectionTransform, sizeof(cameraProjectionTransform), &blobSize);
        }

        // camera view
        if (SUCCEEDED(hr))
        {
            hr = pSample->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraViewTransform, sizeof(cameraViewTransform), &blobSize);
        }
    }

    // store the value to the header
    if (FAILED(hr))
    {
        ZeroMemory(&cameraIntrinsics, sizeof(cameraIntrinsics));
        ZeroMemory(&worldToCameraMatrix, sizeof(worldToCameraMatrix));
        ZeroMemory(&cameraProjectionTransform, sizeof(cameraProjectionTransform));
        ZeroMemory(&cameraViewTransform, sizeof(cameraViewTransform));
    }

    // store the values to be sent
    pSampleHeader->worldToCameraMatrix = worldToCameraMatrix;
    pSampleHeader->cameraProjectionTransform = cameraProjectionTransform;
    pSampleHeader->cameraViewTransform = cameraViewTransform;

    return hr;
}

// Prepare bundle to notify missing sample for particular timestamp
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::PrepareStreamTick(
    IMFAttributes* pAttributes,
    IDataBundle** ppDataBundle)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::PrepareStreamTick()\n");

    NULL_CHK(pAttributes);
    NULL_CHK(ppDataBundle);

    const DWORD c_cbHeaderSize = sizeof(PayloadHeader) + sizeof(MediaStreamTick);

    ComPtr<IDataBuffer> spHeader;
    IFR(MakeAndInitialize<DataBufferImpl>(&spHeader, c_cbHeaderSize));

    // Prepare the buffer
    ComPtr<IBuffer> spBuffer;
    IFR(spHeader.As(&spBuffer));

    // Prepare PayloadType header
    BYTE* pHeaderBuffer = GetDataType<BYTE*>(spBuffer.Get());
    PayloadHeader* pOpHeader = reinterpret_cast<PayloadHeader *>(pHeaderBuffer);
    pOpHeader->ePayloadType = PayloadType_SendMediaStreamTick;
    pOpHeader->cbPayloadSize = sizeof(MediaStreamTick);

    // Prepare description
    MediaStreamTick* pSampleTick = reinterpret_cast<MediaStreamTick *>(pHeaderBuffer + sizeof(PayloadHeader));
    pSampleTick->dwStreamId = _dwStreamId;

    LONGLONG llSampleTime;
    ComPtr<IMFSample> spSample;
    IFR(pAttributes->QueryInterface(__uuidof(IMFSample), static_cast<LPVOID*>(&spSample)));
    IFR(spSample->GetSampleTime(&llSampleTime));

    pSampleTick->hnsTimestamp = llSampleTime - _adjustedStartTime;
    if (pSampleTick->hnsTimestamp < 0)
    {
        pSampleTick->hnsTimestamp = 0;
    }

    IFR(pAttributes->SetUINT64(MFSampleExtension_Timestamp, llSampleTime));

    // copy attribute blob to a buffer
    // Get size of attributes blob
    IFR(MFGetAttributesAsBlobSize(pAttributes, &pSampleTick->cbAttributesSize));

    // Create a buffer for attribute blob
    ComPtr<IDataBuffer> spDataBuffer;
    IFR(MakeAndInitialize<DataBufferImpl>(&spDataBuffer, pSampleTick->cbAttributesSize));

    // Prepare the IBuffer
    ComPtr<IBuffer> spAttribBuffer;
    IFR(spDataBuffer.As(&spAttribBuffer));

    BYTE* pBuf = GetDataType<BYTE*>(spAttribBuffer.Get());

    // Copy attributes to the buffer
    IFR(MFGetAttributesAsBlob(pAttributes, pBuf, pSampleTick->cbAttributesSize));

    IFR(spDataBuffer->put_CurrentLength(pSampleTick->cbAttributesSize));

    // Add size of variable size attribute blob to size of the package.
    pOpHeader->cbPayloadSize += pSampleTick->cbAttributesSize;

    // Set length of the buffer
    IFR(spHeader->put_CurrentLength(c_cbHeaderSize));

    // Prepare bundle to send
    ComPtr<IDataBundle> spBundle;
    IFR(MakeAndInitialize<DataBundleImpl>(&spBundle));

    // Add fixed size header and description to the bundle
    IFR(spBundle->AddBuffer(spHeader.Get()));

    // Add attributes
    IFR(spBundle->AddBuffer(spDataBuffer.Get()));

    NULL_CHK_HR(spBundle, E_NOT_SET);

    return spBundle.CopyTo(ppDataBundle);
}

// Prepare bundle with format change information to be sent over the wire
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::PrepareFormatChange(
    IMFMediaType* pMediaType, 
    IDataBundle** ppDataBundle)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::CompleteOpen()\n");

    const DWORD c_cbPayloadSize = sizeof(PayloadHeader) + sizeof(MediaTypeDescription);

    ComPtr<IDataBuffer> spDataBuffer;
    IFR(MakeAndInitialize<DataBufferImpl>(&spDataBuffer, c_cbPayloadSize));

    // Prepare the buffer
    ComPtr<IBuffer> spBuffer;
    IFR(spDataBuffer.As(&spBuffer));

    // Prepare PayloadType header
    BYTE* pBuf = GetDataType<BYTE*>(spBuffer.Get());
    PayloadHeader* pOpHeader = reinterpret_cast<PayloadHeader *>(pBuf);
    pOpHeader->cbPayloadSize = sizeof(MediaTypeDescription);
    pOpHeader->ePayloadType = PayloadType_SendFormatChange;

    // Prepare description
    MediaTypeDescription* pStreamDescription = reinterpret_cast<MediaTypeDescription *>(pBuf + sizeof(PayloadHeader));
    ComPtr<IDataBuffer> spAttr;
    IFR(FillStreamDescription(pStreamDescription, &spAttr));

    // Add size of attribute blob to size of the package.
    pOpHeader->cbPayloadSize += pStreamDescription->AttributesBlobSize;

    // Set length of the buffer
    IFR(spDataBuffer->put_CurrentLength(c_cbPayloadSize));

    // Prepare bundle to send
    ComPtr<IDataBundle> spBundle;
    IFR(MakeAndInitialize<DataBundleImpl>(&spBundle));

    // Add fixed size header and description to the bundle
    IFR(spBundle->AddBuffer(spDataBuffer.Get()));

    // Add attributes
    IFR(spBundle->AddBuffer(spAttr.Get()));

    return spBundle.CopyTo(ppDataBundle);
}

// Fill stream description and prepare attributes blob.
_Use_decl_annotations_
HRESULT NetworkMediaSinkStreamImpl::FillStreamDescription(
    MediaTypeDescription* pStreamDescription,
    IDataBuffer** ppDataBuffer)
{
    Log(Log_Level_Info, L"NetworkMediaSinkStreamImpl::CompleteOpen()\n");

    NULL_CHK(ppDataBuffer);

    // Clear the stream descriptor memory
    ZeroMemory(pStreamDescription, sizeof(MediaTypeDescription));

    // Get the media type for the stream
    ComPtr<IMFMediaType> spMediaType;
    IFR(GetCurrentMediaType(&spMediaType));

    ComPtr<IMFMediaType> spFilteredMediaType;
    IFR(MFCreateMediaType(&spFilteredMediaType));

    // filter types to those deemed needed
    IFR(FilterOutputMediaType(spMediaType.Get(), spFilteredMediaType.Get()));

    // fill in streamDescription
    pStreamDescription->dwStreamId = _dwStreamId;

    // set major type (Audio, Video and so on)
    IFR(GetMajorType(&pStreamDescription->guiMajorType));

    // set subtype (format of the stream)
    IFR(spMediaType->GetGUID(MF_MT_SUBTYPE, &pStreamDescription->guiSubType));

    // Set size of attributes blob
    UINT32 attributesSize = 0;
    IFR(MFGetAttributesAsBlobSize(spFilteredMediaType.Get(), &attributesSize));

    // Prepare a buffer for the filtered mediaType
    ComPtr<DataBufferImpl> spAttributes;
    IFR(MakeAndInitialize<DataBufferImpl>(&spAttributes, attributesSize));

    // Set length of the buffer
    IFR(spAttributes->put_CurrentLength(attributesSize));

    // Copy attributes to the buffer
    UINT8* pBuffer = GetDataType<UINT8*>(spAttributes.Get());
    NULL_CHK(pBuffer);

    IFR(MFGetAttributesAsBlob(spFilteredMediaType.Get(), pBuffer, attributesSize));

    // were good, save the valus and return
    pStreamDescription->AttributesBlobSize = attributesSize;

    return spAttributes.CopyTo(ppDataBuffer);
}
