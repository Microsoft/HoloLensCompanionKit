// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <sdkddkver.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
#endif

// Standard C++ first
#include <assert.h>
#include <list>
#include <map>
#include <unordered_set>
#include <memory>
#include <future>

// Windows
#include <initguid.h>
#include <wrl.h>
#include <wrl\async.h>
#include <wrl\wrappers\corewrappers.h>
#include <strsafe.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.foundation.numerics.h>
#include <windows.media.h>
#include <windows.media.capture.h>
#include <windows.media.effects.h>
#include <windows.media.mediaproperties.h>
#include <windows.networking.sockets.h>
#include <windows.perception.h>
#include <windows.perception.spatial.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <windows.system.threading.h>
#include <robuffer.h>
#pragma comment(lib, "runtimeobject")

#include <ppltasks.h>
#include <concurrent_queue.h>

// DirectX
#include <directxmath.h>
#include <directxpackedvector.h>
#include <d3d11.h>
#include <d3d11_2.h>
#pragma comment(lib, "d3d11")
#include <dxgi.h>
#pragma comment(lib, "dxgi")

// Media Foundation
#include <mfidl.h>
#include <mferror.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mfmediacapture.h>
#include <mfmediaengine.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mf")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite.lib")

#ifndef MRVCDLL
#ifdef MIXEDREMOTEVIEWCOMPOSITOR_EXPORTS
#define MRVCDLL EXTERN_C HRESULT __declspec(dllexport) STDAPICALLTYPE
#define MRVCDLL_(type) EXTERN_C type __declspec(dllexport) STDAPICALLTYPE
#else
#define MRVCDLL EXTERN_C HRESULT __declspec(dllimport) STDAPICALLTYPE
#define MRVCDLL_(type) EXTERN_C type __declspec(dllimport) STDAPICALLTYPE
#endif // MIXEDREMOTEVIEWCOMPOSITOR_EXPORTS
#endif

// mf guids for pulling sample data
//EXTERN_GUID(MFSampleExtension_PinholeCameraIntrinsics, 0x4ee3b6c5, 0x6a15, 0x4e72, 0x97, 0x61, 0x70, 0xc1, 0xdb, 0x8b, 0x9f, 0xe3);
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
EXTERN_GUID(MFSampleExtension_Spatial_CameraCoordinateSystem, 0x9d13c82f, 0x2199, 0x4e67, 0x91, 0xcd, 0xd1, 0xa4, 0x18, 0x1f, 0x25, 0x34);
EXTERN_GUID(MFSampleExtension_Spatial_CameraViewTransform, 0x4e251fa4, 0x830f, 0x4770, 0x85, 0x9a, 0x4b, 0x8d, 0x99, 0xaa, 0x80, 0x9b);
EXTERN_GUID(MFSampleExtension_Spatial_CameraProjectionTransform, 0x47f9fcb5, 0x2a02, 0x4f26, 0xa4, 0x77, 0x79, 0x2f, 0xdf, 0x95, 0x88, 0x6a);
#endif
EXTERN_GUID(Spatial_CameraTransform, 0x49d793d7, 0x5378, 0x43dd, 0xb2, 0xb3, 0xfe, 0x17, 0x18, 0xaa, 0xcb, 0x1d);

template <typename T>
inline T GetDataType(_In_ ABI::Windows::Storage::Streams::IBuffer* pBuffer)
{
    // take a reference to the passed in buffer
    ComPtr<ABI::Windows::Storage::Streams::IBuffer> spBuffer(pBuffer);

    // QI for BufferByteAccess
    Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> spBufferByteAccess;
    if (SUCCEEDED(spBuffer.As(&spBufferByteAccess)))
    {
        byte* pBytes = nullptr;
        if (SUCCEEDED(spBufferByteAccess->Buffer(&pBytes)))
        {
            // cast to correct type
            return reinterpret_cast<T>(pBytes);
        };
    }

    return nullptr;
}

inline std::wstring to_wstring(unsigned int d)
{
    wchar_t buffer[65] = { 0 }; //64 + sign
    _ui64tow_s(d, buffer, sizeof(buffer) / sizeof(buffer[0]), 10);

    return std::wstring(buffer);
}


using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

using namespace Windows::Foundation;

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Foundation::Numerics;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Networking;
using namespace ABI::Windows::Networking::Sockets;
using namespace ABI::Windows::Perception::Spatial;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::System::Threading;

#include "ErrorHandling.h"
#include "AsyncOperations.h"
#include "LinkList.h"

#include "MixedRemoteViewCompositor.h"
using namespace ABI::MixedRemoteViewCompositor;
using namespace ABI::MixedRemoteViewCompositor::Plugin;
using namespace ABI::MixedRemoteViewCompositor::Network;
using namespace ABI::MixedRemoteViewCompositor::Media;

#include "RenderingPlugin.h"
#include "ModuleManager.h"
#include "ModuleManagerStatics.h"
#include "DirectXManager.h"
#include "PluginManager.h"
#include "PluginManagerStatics.h"
#include "DataBuffer.h"
#include "DataBundle.h"
#include "DataBundleArgs.h"
#include "Connection.h"
#include "Listener.h"
#include "Connector.h"
#include "Marker.h"
#include "NetworkMediaSinkStream.h"
#include "NetworkMediaSink.h"
#include "MrcAudioEffectDefinition.h"
#include "MrcVideoEffectDefinition.h"
#include "CaptureEngine.h"
#include "NetworkMediaSourceStream.h"
#include "NetworkMediaSource.h"
#include "PlaybackEngine.h"

using namespace MixedRemoteViewCompositor::Plugin;
using namespace MixedRemoteViewCompositor::Network;
using namespace MixedRemoteViewCompositor::Media;
