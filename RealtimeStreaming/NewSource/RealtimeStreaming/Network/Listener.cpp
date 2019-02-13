// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Listener.h"
#include "AsyncOperations.h"

ActivatableStaticOnlyFactory(ListenerStaticsImpl);

ListenerImpl::ListenerImpl()
    : _isInitialized(false)
    , m_port(-1)
    , _socketListener(nullptr)
    , m_streamSocketResult(nullptr)
{
    Log(Log_Level_Info, L"ListenerImpl::ListenerImpl()\n");
}

ListenerImpl::~ListenerImpl()
{
    Log(Log_Level_Info, L"ListenerImpl::~ListenerImpl()\n");

    Uninitialize();
}

_Use_decl_annotations_
HRESULT ListenerImpl::RuntimeClassInitialize(UINT16 port)
{
    Log(Log_Level_Info, L"ListenerImpl::RuntimeClassInitialize()\n");

    auto lock = _lock.Lock();

    _isInitialized = true;

    m_port = port;

    return S_OK;
}

// ModuleBaseImpl
_Use_decl_annotations_
HRESULT ListenerImpl::get_IsInitialized(
    _Out_ boolean *initialized)
{
    Log(Log_Level_Info, L"ListenerImpl::get_IsInitialized()\n");

    NULL_CHK(initialized);

    auto lock = _lock.Lock();

    *initialized = _isInitialized;

    return S_OK;
};

_Use_decl_annotations_
HRESULT ListenerImpl::Uninitialize()
{
    Log(Log_Level_Info, L"ListenerImpl::Uninitialize()\n");

    auto lock = _lock.Lock();

    if (!_isInitialized)
    {
        return S_OK;
    }

    _isInitialized = false;

    Close();

    return S_OK;
}

// IListener
_Use_decl_annotations_
HRESULT ListenerImpl::ListenAsync(
    IAsyncOperation<Connection*>** operation)
{
    NULL_CHK(operation);

    com_ptr<IAsyncOperation<Connection*>> spThis(this);
    NULL_CHK_HR(spThis.get(), E_POINTER);

    IFT(spThis.CopyTo(operation));

    return Start();
}

_Use_decl_annotations_
HRESULT ListenerImpl::add_Closed(
    IClosedEventHandler *eventHandler,
    EventRegistrationToken *token)
{
    Log(Log_Level_Info, L"ListenerImpl::add_Closed()\n");

    NULL_CHK(eventHandler);
    NULL_CHK(token);

    auto lock = _lock.Lock();

    return _evtClosed.Add(eventHandler, token);
}

_Use_decl_annotations_
HRESULT ListenerImpl::remove_Closed(EventRegistrationToken token)
{
    Log(Log_Level_Info, L"ListenerImpl::remove_Closed()\n");

    auto lock = _lock.Lock();

    return _evtClosed.Remove(token);
}

_Use_decl_annotations_
HRESULT ListenerImpl::OnConnectionReceived(
    IStreamSocketListener *sender,
    IStreamSocketListenerConnectionReceivedEventArgs *args)
{
    UNREFERENCED_PARAMETER(sender);

    NULL_CHK(args);

    Log(Log_Level_Info, L"ListenerImpl::OnConnectionReceived()\n");

    auto lock = _lock.Lock();

    HRESULT hr = S_OK;

    IFC(args->get_Socket(&m_streamSocketResult));

done:
    if (FAILED(hr))
    {
        TryTransitionToError(hr);
    }

    return FireCompletion();
};


// IAsyncOperation
_Use_decl_annotations_
HRESULT ListenerImpl::put_Completed(
    ABI::Windows::Foundation::IAsyncOperationCompletedHandler<Connection*> *handler)
{
    return PutOnComplete(handler);
}

_Use_decl_annotations_
HRESULT ListenerImpl::get_Completed(
    ABI::Windows::Foundation::IAsyncOperationCompletedHandler<Connection*>** handler)
{
    return GetOnComplete(handler);
}

_Use_decl_annotations_
HRESULT ListenerImpl::GetResults(
    IConnection** ppConnection)
{
    auto lock = _lock.Lock();

    IFT(AsyncBase::CheckValidStateForResultsCall());

    com_ptr<Connection> spConnection;
    IFT(MakeAndInitialize<Connection>(&spConnection, m_streamSocketResult.detach()));

    NULL_CHK_HR(spConnection, E_OUTOFMEMORY);

    spConnection.CopyTo(ppConnection);

    return Close();
}

_Use_decl_annotations_
HRESULT ListenerImpl::OnStart(void)
{
    Log(Log_Level_Info, L"ListenerImpl::OnStart()\n");

    // convert port to string
    std::wstring wsPort = to_wstring(m_port);

    // create a listener
    com_ptr<ABI::Windows::Networking::Sockets::IStreamSocketListener> socketListener;
    IFT(Windows::Foundation::ActivateInstance(
        Wrappers::HStringReference(RuntimeClass_Windows_Networking_Sockets_StreamSocketListener).get(), 
        &socketListener));

    // define callback for any connections
    auto receivedFn = std::bind(&ListenerImpl::OnConnectionReceived, this, std::placeholders::_1, std::placeholders::_2);

    auto connectionReceivedCallback = Callback<IConnectionReceivedEventHandler>(receivedFn);

    // register for callbacks
    IFT(socketListener->add_ConnectionReceived(connectionReceivedCallback.get(), &_connectionReceivedEventToken));

    // setup bindAsync
    com_ptr<IAsyncAction> bindOperation;
    IFT(socketListener->BindServiceNameAsync(
        Wrappers::HStringReference(wsPort.data()).get(),
        &bindOperation));

    // setup callback and start
    com_ptr<ListenerImpl> spThis(this);
    return StartAsyncThen(
        bindOperation.get(),
        [this, spThis, socketListener](_In_ HRESULT hr, _In_ IAsyncAction* pAsyncResult, _In_ AsyncStatus asyncStatus) -> HRESULT
    {
        auto lock = _lock.Lock();

        IFC(hr);

        // getting ready to begin, store the socket
        IFC(socketListener.As(&_socketListener));

    done:
        if (FAILED(hr))
        {
            TryTransitionToError(hr);
            return FireCompletion();
        }

        return hr;
    });
}

_Use_decl_annotations_
void ListenerImpl::OnClose(void)
{
    CloseInternal();
}

_Use_decl_annotations_
void ListenerImpl::OnCancel(void)
{
    CloseInternal();
}

_Use_decl_annotations_
void ListenerImpl::CloseInternal()
{
    com_ptr<ABI::Windows::Foundation::IClosable> closeable;
    if (nullptr != _socketListener)
    {
        // cleanup socket
        LOG_RESULT(_socketListener->remove_ConnectionReceived(_connectionReceivedEventToken));
        if SUCCEEDED(_socketListener.As(&closeable))
        {
            LOG_RESULT(closeable->Close());
        }

        com_ptr<IListener> spThis(this);
        LOG_RESULT(_evtClosed.InvokeAll(spThis.get()));
    }
    _socketListener.Reset();
    _socketListener = nullptr;

    if (nullptr != m_streamSocketResult)
    {
        if SUCCEEDED(m_streamSocketResult.As(&closeable))
        {
            LOG_RESULT(closeable->Close());
        }
    }
    m_streamSocketResult.Reset();
    m_streamSocketResult = nullptr;
}