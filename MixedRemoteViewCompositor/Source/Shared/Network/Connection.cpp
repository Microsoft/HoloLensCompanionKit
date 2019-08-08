// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Connection.h"

_Use_decl_annotations_
inline HRESULT PrepareRemoteUrl(
    _In_ IStreamSocketInformation* pInfo, 
    _Out_ IUriRuntimeClass** ppUri)
{
    NULL_CHK(pInfo);
    NULL_CHK(ppUri);

    ComPtr<IStreamSocketInformation> spInfo(pInfo);
    
    ComPtr<IHostName> spHostName;
    IFR(spInfo->get_RemoteHostName(&spHostName));

    HostNameType spHostNameType;
    IFR(spHostName->get_Type(&spHostNameType));

    HString rawName;
    IFR(spHostName->get_RawName(rawName.GetAddressOf()));

    UINT32 length = 0;
    WCHAR pszUri[MAX_PATH];
    if (spHostNameType == HostNameType::HostNameType_Ipv4 || spHostNameType == HostNameType::HostNameType_DomainName)
    {
        IFR(StringCchPrintf(pszUri, _countof(pszUri), L"%s://%s", c_szNetworkScheme, rawName.GetRawBuffer(&length)));
    }
    else if (spHostNameType == HostNameType::HostNameType_Ipv6)
    {
        IFR(StringCchPrintf(pszUri, _countof(pszUri), L"%s://[%s]", c_szNetworkScheme, rawName.GetRawBuffer(&length)));
    }

    length = std::wcslen(pszUri);

    Microsoft::WRL::Wrappers::HString uriHString;
    IFR(WindowsCreateString(pszUri, length, uriHString.GetAddressOf()));

    ComPtr<IUriRuntimeClassFactory> uriFactory;
    IFR(
        Windows::Foundation::GetActivationFactory(
            Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), 
            uriFactory.GetAddressOf())
    );

    ComPtr<IUriRuntimeClass> spUri;
    IFR(uriFactory->CreateUri(uriHString.Get(), &spUri));

    NULL_CHK_HR(spUri, E_NOT_SET);

    return spUri.CopyTo(ppUri);
}


_Use_decl_annotations_
ConnectionImpl::ConnectionImpl()
    : _isInitialized(false)
    , _concurrentFailedBuffers(0)
    , _concurrentFailedBundles(0)
    , _streamSocket(nullptr)
    , _receivedBundle(nullptr)
{
    ZeroMemory(&_receivedHeader, sizeof(PayloadHeader));
}

_Use_decl_annotations_
ConnectionImpl::~ConnectionImpl()
{
    Log(Log_Level_Warning, L"ConnectionImpl::~ConnectionImpl()\n");

    Uninitialize();
}

_Use_decl_annotations_
HRESULT ConnectionImpl::RuntimeClassInitialize(
    IStreamSocket* socket)
{
    Log(Log_Level_Info, L"ConnectionImpl::RuntimeClassInitialize(socket)\n");

    NULL_CHK(socket);

    auto lock = _lock.Lock();

    _isInitialized = true;

    // store the socket
    ComPtr<IStreamSocket> spSocket(socket);
    IFR(spSocket.As(&_streamSocket));

    ZeroMemory(&_receivedHeader, sizeof(PayloadHeader));
    _receivedHeader.ePayloadType = PayloadType_Unknown;

    // create a thread to send data
    ComPtr<IThreadPoolStatics> threadPoolStatics;
    IFR(Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_System_Threading_ThreadPool).Get(),
        &threadPoolStatics));

    IFR(threadPoolStatics.As(&_threadPoolStatics));

    return WaitForHeader();
}


// IModule
_Use_decl_annotations_
HRESULT ConnectionImpl::get_IsInitialized(
    _Out_ boolean* initialized)
{
    Log(Log_Level_Info, L"ConnectionImpl::get_IsInitialized()\n");

    NULL_CHK(initialized);

    auto lock = _lock.Lock();

    *initialized = _isInitialized;

    return S_OK;
};

_Use_decl_annotations_
HRESULT ConnectionImpl::Uninitialize(void)
{
    Log(Log_Level_Info, L"ConnectionImpl::Uninitialize()\n");

    auto lock = _lock.Lock();

    if (!_isInitialized)
    {
        return S_OK;
    }

    _isInitialized = false;

    return Close();
}

_Use_decl_annotations_
HRESULT ConnectionImpl::Close(void)
{
    Log(Log_Level_Info, L"ConnectionImpl::Close()\n");

    if (nullptr == _streamSocket)
    {
        return S_OK;
    }

    LOG_RESULT(ResetBundle());

    // cleanup socket
    ComPtr<ABI::Windows::Foundation::IClosable> closeable;
    if SUCCEEDED(_streamSocket.As(&closeable))
    {
        LOG_RESULT(closeable->Close());
    }

    _streamSocket.Reset();
    _streamSocket = nullptr;

    ComPtr<IConnection> spThis(this);
    return _evtDisconnected.InvokeAll(spThis.Get());
}


// IConnection
_Use_decl_annotations_
HRESULT ConnectionImpl::get_IsConnected(
    boolean* connected)
{
    Log(Log_Level_Info, L"ConnectionImpl::get_IsConnected()\n");

    NULL_CHK(connected);

    auto lock = _lock.Lock();

    if (nullptr == _streamSocket)
    {
        return MF_E_SHUTDOWN;
    }

    *connected = (nullptr != _streamSocket);

    return S_OK;
}

_Use_decl_annotations_
HRESULT ConnectionImpl::add_Disconnected(
    IDisconnectedEventHandler* eventHandler, 
    EventRegistrationToken* token)
{
    Log(Log_Level_Info, L"ConnectionImpl::add_Disconnected()\n");

    NULL_CHK(eventHandler);
    NULL_CHK(token);

    auto lock = _lock.Lock();

    if (nullptr == _streamSocket)
    {
        return MF_E_SHUTDOWN;
    }

    return _evtDisconnected.Add(eventHandler, token);
}

_Use_decl_annotations_
HRESULT ConnectionImpl::remove_Disconnected(
    EventRegistrationToken token)
{
    Log(Log_Level_Info, L"ConnectionImpl::remove_Disconnected()\n");

    auto lock = _lock.Lock();

    return _evtDisconnected.Remove(token);
}

_Use_decl_annotations_
HRESULT ConnectionImpl::add_Received(
    IBundleReceivedEventHandler* eventHandler,
    EventRegistrationToken* token)
{
    Log(Log_Level_Info, L"ConnectionImpl::add_Received()\n");

    NULL_CHK(eventHandler);
    NULL_CHK(token);

    auto lock = _lock.Lock();

    if (nullptr == _streamSocket)
    {
        return MF_E_SHUTDOWN;
    }

    return _evtBundleReceived.Add(eventHandler, token);
}

_Use_decl_annotations_
HRESULT ConnectionImpl::remove_Received(
    EventRegistrationToken token)
{
    Log(Log_Level_Info, L"ConnectionImpl::remove_Received()\n");

    auto lock = _lock.Lock();

    return _evtBundleReceived.Remove(token);
}

_Use_decl_annotations_
HRESULT ConnectionImpl::SendPayloadType(
    PayloadType payloadType)
{
    Log(Log_Level_Info, L"ConnectionImpl::SendRequest(%d)\n", payloadType);

    // send an PayloadType header, contains no payload.
    ComPtr<IDataBuffer> dataBuffer;
    IFR(MakeAndInitialize<DataBufferImpl>(&dataBuffer, sizeof(PayloadHeader)));

    ComPtr<IDataBundle> dataBundle;
    IFR(MakeAndInitialize<DataBundleImpl>(&dataBundle));

    // cast to IBufferByteAccess
    ComPtr<Windows::Storage::Streams::IBufferByteAccess> spByteAccess;
    IFR(dataBuffer.As(&spByteAccess));

    // get the raw byte pointer
    BYTE* buffer = nullptr;
    IFR(spByteAccess->Buffer(&buffer));
    NULL_CHK(buffer);

    // cast to the dataPtr
    PayloadHeader* pOpHeader = reinterpret_cast<PayloadHeader*>(buffer);
    NULL_CHK(pOpHeader);

    pOpHeader->cbPayloadSize = 0;
    pOpHeader->ePayloadType = payloadType;

    IFR(dataBuffer->put_CurrentLength(sizeof(PayloadHeader)));

    IFR(dataBundle->AddBuffer(dataBuffer.Get()));

    return SendBundle(dataBundle.Get());
}

_Use_decl_annotations_
HRESULT ConnectionImpl::SendBundle(
    IDataBundle* dataBundle)
{
    Log(Log_Level_Info, L"ConnectionImpl::SendBundle(bundle)\n");

    NULL_CHK(dataBundle);

    // get the output stream for socket
    ComPtr<IOutputStream> spOutputStream;
    {
        auto lock = _lock.Lock();

        IFR(CheckClosed());

        IFR(_streamSocket->get_OutputStream(&spOutputStream));
    }

    DataBundleImpl* bundleImpl = static_cast<DataBundleImpl*>(dataBundle);
    NULL_CHK_HR(bundleImpl, E_INVALIDARG);

    DataBundleImpl::Container buffers;
    IFR(bundleImpl->get_Buffers(&buffers));

    // send one buffer at a time syncronously
    DataBundleImpl::Iterator iter = buffers.begin();
    for (; iter != buffers.end(); ++iter)
    {
        ComPtr<IBuffer> rawBuffer;
        IFR((*iter).As(&rawBuffer));

        ComPtr<IStreamWriteOperation> writeOperation;
        IFR(spOutputStream->WriteAsync(rawBuffer.Get(), &writeOperation));

        HRESULT hr = SyncWait<UINT32, UINT32>(writeOperation.Get(), 1);
        IFR(hr);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT ConnectionImpl::SendBundleAsync(
	IDataBundle *dataBundle,
	IAsyncAction **sendAction)
{
	Log(Log_Level_Info, L"ConnectionImpl::SendBundleAsyncSequenced\n");

	NULL_CHK(dataBundle);
	NULL_CHK(sendAction);

	{
		auto lock = _lock.Lock();

		IFR(CheckClosed());
	}

	ComPtr<WriteCompleteImpl> spWriteAction;
	IFR(MakeAndInitialize<WriteCompleteImpl>(&spWriteAction, 1));

	// define workitem
	ComPtr<IDataBundle> spDataBundle(dataBundle);
	ComPtr<ConnectionImpl> spThis(this);
	auto workItem =
		Microsoft::WRL::Callback<ABI::Windows::System::Threading::IWorkItemHandler>(
			[this, spThis, spDataBundle, spWriteAction](IAsyncAction* asyncAction) -> HRESULT
	{
		ComPtr<IOutputStream> spOutputStream;
		DataBundleImpl::Container buffers;
		DataBundleImpl::Iterator iter;

		ComPtr<IStreamWriteOperation> writeOperation;
		ComPtr<IBuffer> rawBuffer;
		ComPtr<IDataBuffer> singleBuffer;

		HRESULT hr = S_OK;

		{
			auto lock = _lock.Lock();

			IFC(CheckClosed());

			// get the output stream for socket
			if (nullptr != _streamSocket)
			{
				IFC(_streamSocket->get_OutputStream(&spOutputStream));
			}
			else
			{
				IFC(E_UNEXPECTED);
			}
		}

		DataBundleImpl* bundleImpl = static_cast<DataBundleImpl*>(spDataBundle.Get());
		if (nullptr == bundleImpl)
		{
			IFC(E_INVALIDARG);
		}

		//make a single big buffer
		ULONG totalSize;
		IFC(bundleImpl->get_TotalSize(&totalSize));

		IFC(MakeAndInitialize<DataBufferImpl>(&singleBuffer, totalSize));

		DataBufferImpl * bufferImpl = static_cast<DataBufferImpl*>(singleBuffer.Get());

		if (nullptr == bufferImpl) 
		{
			IFC(E_INVALIDARG);
		}

		BYTE* rawSingleBuffer;
		IFC(bufferImpl->get_Buffer(&rawSingleBuffer));

		DWORD copied;
		IFC(bundleImpl->CopyTo(0, totalSize, &rawSingleBuffer[0], &copied));

		IFC(bufferImpl->put_CurrentLength(copied));

		IFC(singleBuffer.As(&rawBuffer));

		IFC(spOutputStream->WriteAsync(rawBuffer.Get(), &writeOperation));

		hr = SyncWait<UINT32, UINT32>(writeOperation.Get(), 1);

		spWriteAction->SignalCompleted(hr);

	done:
		if (FAILED(hr))
		{
			spWriteAction->SignalCompleted(hr);
		}

		return S_OK;
	});

	ComPtr<IAsyncAction> workerAsync;
	IFR(_threadPoolStatics->RunAsync(workItem.Get(), &workerAsync));

	IFR(StartAsyncThen(
		workerAsync.Get(),
		[this, spThis, spWriteAction](_In_ HRESULT hr, _In_ IAsyncAction *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
	{
		if (FAILED(hr))
		{
			spWriteAction->SignalCompleted(hr);
		}

		return S_OK;
	}));

	// hand off async op
	return spWriteAction.CopyTo(sendAction);
}

// IConnectionInternal
_Use_decl_annotations_
HRESULT ConnectionImpl::WaitForHeader()
{
    Log(Log_Level_Info, L"ConnectionImpl::WaitForHeader()\n");

    IFR(CheckClosed());

    if (PayloadType_Unknown != _receivedHeader.ePayloadType
        ||
        0 != _receivedHeader.cbPayloadSize)
    {
        ResetBundle();
    }

    if (nullptr == _spHeaderBuffer)
    {
        IFR(MakeAndInitialize<DataBufferImpl>(&_spHeaderBuffer, sizeof(PayloadHeader)));
    }

    ComPtr<IMFMediaBuffer> spMediaBuffer;
    IFR(_spHeaderBuffer->get_MediaBuffer(&spMediaBuffer));

    DWORD bufferLen = 0;
    IFR(spMediaBuffer->GetMaxLength(&bufferLen)); 

    // should be the same as sizeof(PayloadHeader)
    if (bufferLen != sizeof(PayloadHeader))
    {
       IFR(_spHeaderBuffer->put_CurrentLength(sizeof(PayloadHeader)))
    }

    // get the socket input stream reader
    ComPtr<IInputStream> spInputStream;
    IFR(_streamSocket->get_InputStream(&spInputStream));

    // set the read operation and wait for data
    ComPtr<IStreamReadOperation> readAsyncOperation;
    IFR(spInputStream->ReadAsync(_spHeaderBuffer.Get(), bufferLen, InputStreamOptions::InputStreamOptions_Partial, &readAsyncOperation));

    ComPtr<ConnectionImpl> spThis(this);
    return StartAsyncThen(
        readAsyncOperation.Get(),
        [this, spThis](_In_ HRESULT hr, _In_ IStreamReadOperation *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
    {
        auto lock = _lock.Lock();

        if (FAILED(CheckClosed()) || FAILED(hr))
        {
            LOG_RESULT(hr);

            return Close();
        }

        return OnHeaderReceived(asyncResult, asyncStatus);
    });
}

_Use_decl_annotations_
HRESULT ConnectionImpl::WaitForPayload()
{
    Log(Log_Level_Info, L"ConnectionImpl::WaitForPayload()\n");

    if (FAILED(CheckClosed()))
    {
        return S_OK;
    }

    if (PayloadType_Unknown == _receivedHeader.ePayloadType
        ||
        PayloadType_ENDOFLIST <= _receivedHeader.ePayloadType
        ||
        0 == _receivedHeader.cbPayloadSize)
    {
        IFR(HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
    }

    ComPtr<DataBufferImpl> payloadBuffer;
    IFR(MakeAndInitialize<DataBufferImpl>(&payloadBuffer, _receivedHeader.cbPayloadSize));

    // get the underlying buffer and makes sure it the right size
    ComPtr<IMFMediaBuffer> spMediaBuffer;
    IFR(payloadBuffer->get_MediaBuffer(&spMediaBuffer));

    DWORD bufferLen = 0;
    IFR(spMediaBuffer->GetMaxLength(&bufferLen));

    // should be the same as sizeof(PayloadHeader)
    if (bufferLen != _receivedHeader.cbPayloadSize)
    {
        IFR(spMediaBuffer->SetCurrentLength(_receivedHeader.cbPayloadSize))
    }

    // get the socket input stream reader
    ComPtr<IInputStream> spInputStream;
    IFR(_streamSocket->get_InputStream(&spInputStream));

    // set the read operation and wait for data
    ComPtr<IStreamReadOperation> readOperation;
    IFR(spInputStream->ReadAsync(payloadBuffer.Get(), _receivedHeader.cbPayloadSize, InputStreamOptions::InputStreamOptions_None, &readOperation));

    ComPtr<ConnectionImpl> spThis(this);
    return StartAsyncThen(
        readOperation.Get(),
        [this, spThis](_In_ HRESULT hr, _In_ IStreamReadOperation *asyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
    {
        auto lock = _lock.Lock();

        if (FAILED(CheckClosed()) || FAILED(hr))
        {
            LOG_RESULT(hr);

            return Close();
        }

        return OnPayloadReceived(asyncResult, asyncStatus);
    });
}


_Use_decl_annotations_
HRESULT ConnectionImpl::ResetBundle()
{
    Log(Log_Level_Info, L"ConnectionImpl::ResetBundle()\n");

    if (nullptr != _receivedBundle)
    {
        LOG_RESULT(_receivedBundle->Reset());
    }

    ZeroMemory(&_receivedHeader, sizeof(PayloadHeader));

    return S_OK;
}

_Use_decl_annotations_
HRESULT ConnectionImpl::ProcessHeaderBuffer(
    PayloadHeader* header,
    IDataBuffer* dataBuffer)
{
    NULL_CHK(header);
    NULL_CHK(dataBuffer);

    ComPtr<IDataBundle> dataBundle;
    IFR(MakeAndInitialize<DataBundleImpl>(&dataBundle));

    // add buffer
    IFR(dataBundle->AddBuffer(dataBuffer));

    LOG_RESULT(NotifyBundleComplete(header->ePayloadType, dataBundle.Get()));

    // reset anything we created
    return dataBundle.Reset();
}

_Use_decl_annotations_
HRESULT ConnectionImpl::NotifyBundleComplete(
    PayloadType payloadType,
    IDataBundle* dataBundle)
{
    Log(Log_Level_Info, L"ConnectionImpl::OnCompleteBundle\n");

    NULL_CHK(dataBundle);

    if (FAILED(CheckClosed()))
    {
        return S_OK;
    }

    ComPtr<IStreamSocketInformation> spInfo;
    IFR(_streamSocket->get_Information(&spInfo));

    ComPtr<IUriRuntimeClass> spUri;
    IFR(PrepareRemoteUrl(spInfo.Get(), &spUri));

    ComPtr<IBundleReceivedArgs> args;
    IFR(MakeAndInitialize<DataBundleArgsImpl>(&args, payloadType, this, dataBundle, spUri.Get()));

    return _evtBundleReceived.InvokeAll(this, args.Get());
}

// Callbacks
_Use_decl_annotations_
HRESULT ConnectionImpl::OnHeaderReceived(
    IAsyncOperationWithProgress<IBuffer*, UINT32>* asyncResult,
    AsyncStatus asyncStatus)
{
    Log(Log_Level_All, L"ConnectionImpl::OnHeaderReceived\n");

    NULL_CHK(asyncResult);

    ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> spAsynResult(asyncResult);

    // get any error codes
    HRESULT hr = S_OK;

    ComPtr<IBuffer> spBuffer;
    ComPtr<IDataBuffer> dataBuffer;

    IFC(asyncResult->GetResults(&spBuffer));

    UINT32 bytesRead = -1;
    IFC(spBuffer->get_Length(&bytesRead));

    IFC(spBuffer.As(&dataBuffer));

    DWORD bufferSize = -1;
    IFC(dataBuffer->get_CurrentLength(&bufferSize));

    // makes sure this is the expected size
    if (bytesRead != sizeof(PayloadHeader) 
        ||
        bytesRead != bufferSize)
    {
        _concurrentFailedBuffers++;

        goto done;
    }

    // ensure we are waiting for a new payload
    if (_receivedHeader.ePayloadType != PayloadType_Unknown)
    {
        // should not be receiving a header buffer when waiting for payload buffers
        // bad state; bail
        IFC(-1073740758); // STATUS_REQUEST_OUT_OF_SEQUENCE
    }

    // cast buffer to PayloadHeader*
    PayloadHeader* header = GetDataType<PayloadHeader*>(spBuffer.Get());
    IFC(nullptr != header ? S_OK : E_UNEXPECTED);

    // is header type in a range we understand
    if (header->ePayloadType == PayloadType_Unknown
        ||
        header->ePayloadType > PayloadType_ENDOFLIST)
    {
        _concurrentFailedBuffers++;

        goto done;
    }

    // if there is a payload with this buffer, we need to handle that seperately
    if (header->cbPayloadSize != 0)
    {
        // todo: crc checks
        // can we trust the payload size?
        if (header->cbPayloadSize > c_cbMaxBundleSize)
        {
            // just incase it was corrupt we wait for the next one
            _concurrentFailedBuffers++;
        }
        else
        {
            // store the payload header info
            _receivedHeader = *header;

            // start the process to receive payload data
            return WaitForPayload();
        }
    }

    _concurrentFailedBuffers = 0;

    IFC(ProcessHeaderBuffer(header, dataBuffer.Get()));

done:
    if (_concurrentFailedBuffers > c_cbMaxBufferFailures)
    {
        LOG_RESULT(hr);

        LOG_RESULT(Close());

        return S_OK;
    }

    return WaitForHeader(); // go back to waiting for header
}

_Use_decl_annotations_
HRESULT ConnectionImpl::OnPayloadReceived(
    IAsyncOperationWithProgress<IBuffer*, UINT32>* asyncResult,
    AsyncStatus asyncStatus)
{
    Log(Log_Level_All, L"ConnectionImpl::OnPayloadReceived\n");

    ComPtr<IBuffer> buffer;
    IFR(asyncResult->GetResults(&buffer));

    UINT32 bytesRead = -1;
    IFR(buffer->get_Length(&bytesRead));

    ComPtr<IDataBuffer> dataBuffer;
    IFR(buffer.As(&dataBuffer));

    DWORD bufferSize = -1;
    IFR(dataBuffer->get_CurrentLength(&bufferSize));

    // makes sure this is the expected size
    if (c_cbMaxBundleSize < _receivedHeader.cbPayloadSize
        ||
        bytesRead > _receivedHeader.cbPayloadSize
        ||
        bytesRead == 0
        ||
        bytesRead != bufferSize)
    {
        _concurrentFailedBundles++;

        goto done;
    }

    // still have a valid payload type
    if (_receivedHeader.ePayloadType == PayloadType_Unknown
        ||
        _receivedHeader.ePayloadType >= PayloadType_ENDOFLIST)
    {
        _concurrentFailedBundles++;

        goto done;
    }

    // create bundle to hold all buffers
    if (nullptr == _receivedBundle)
    {
        IFR(MakeAndInitialize<DataBundleImpl>(&_receivedBundle));
    }

    ULONG bundleSize = 0;
    IFR(_receivedBundle->get_TotalSize(&bundleSize));

    // before adding the new buffer, make sure that doesn't exceed the payload size
    ULONG expectedSize = _receivedHeader.cbPayloadSize - bundleSize;
    if (bufferSize > expectedSize)
    {
        ComPtr<IDataBuffer> spTrimmed;
        IFR(dataBuffer->TrimRight(expectedSize, &spTrimmed))
    };

    // add the buffer to the bundle
    _receivedBundle->AddBuffer(dataBuffer.Get());

    // recalculate the bundle size
    bundleSize = 0;
    IFR(_receivedBundle->get_TotalSize(&bundleSize));

    // do we have a complete bundle?
    if (bundleSize < _receivedHeader.cbPayloadSize)
    {
        return WaitForPayload();
    }

    // store the bundle data to be used for notification
    PayloadType payloadType = _receivedHeader.ePayloadType;
    LOG_RESULT(NotifyBundleComplete(payloadType, _receivedBundle.Get()));

done:
    ResetBundle();

    if (_concurrentFailedBundles > c_cbMaxBundleFailures)
    {
        LOG_RESULT(HRESULT_FROM_WIN32(ERROR_CONNECTION_UNAVAIL));

        return Close();
    }

    return WaitForHeader();
}

