﻿// WARNING: Please don't edit this file. It was generated by C++/WinRT v1.0.180821.2

#pragma once

#include "winrt/base.h"


static_assert(winrt::check_version(CPPWINRT_VERSION, "1.0.180821.2"), "Mismatched component and base headers.");
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/impl/RealtimeStreaming.Common.2.h"

namespace winrt::impl {

}

WINRT_EXPORT namespace winrt::RealtimeStreaming::Common {

}

namespace winrt::impl {

}

WINRT_EXPORT namespace winrt::experimental::reflect {
template <> struct get_enumerator_names<RealtimeStreaming::Common::PayloadType>
{
    static constexpr std::array<std::wstring_view, 19> value{{ 
        {L"Unknown", 7},
        {L"State_Scene", 11},
        {L"State_Input", 11},
        {L"State_CaptureReady", 18},
        {L"State_CaptureStarted", 20},
        {L"State_CaptureStopped", 20},
        {L"RequestMediaDescription", 23},
        {L"RequestMediaStart", 17},
        {L"RequestMediaStop", 16},
        {L"RequestMediaCaptureStart", 24},
        {L"RequestMediaCaptureRecord", 25},
        {L"RequestMediaCaptureStop", 23},
        {L"RequestMediaEncodeStart", 23},
        {L"RequestMediaEncodeStop", 22},
        {L"SendMediaDescription", 20},
        {L"SendMediaSample", 15},
        {L"SendMediaStreamTick", 19},
        {L"SendFormatChange", 16},
        {L"ENDOFLIST", 9}, }};
};
template <> struct get_enumerator_values<RealtimeStreaming::Common::PayloadType>
{
    static constexpr std::array<RealtimeStreaming::Common::PayloadType, 19> value{{ 
        RealtimeStreaming::Common::PayloadType::Unknown,
        RealtimeStreaming::Common::PayloadType::State_Scene,
        RealtimeStreaming::Common::PayloadType::State_Input,
        RealtimeStreaming::Common::PayloadType::State_CaptureReady,
        RealtimeStreaming::Common::PayloadType::State_CaptureStarted,
        RealtimeStreaming::Common::PayloadType::State_CaptureStopped,
        RealtimeStreaming::Common::PayloadType::RequestMediaDescription,
        RealtimeStreaming::Common::PayloadType::RequestMediaStart,
        RealtimeStreaming::Common::PayloadType::RequestMediaStop,
        RealtimeStreaming::Common::PayloadType::RequestMediaCaptureStart,
        RealtimeStreaming::Common::PayloadType::RequestMediaCaptureRecord,
        RealtimeStreaming::Common::PayloadType::RequestMediaCaptureStop,
        RealtimeStreaming::Common::PayloadType::RequestMediaEncodeStart,
        RealtimeStreaming::Common::PayloadType::RequestMediaEncodeStop,
        RealtimeStreaming::Common::PayloadType::SendMediaDescription,
        RealtimeStreaming::Common::PayloadType::SendMediaSample,
        RealtimeStreaming::Common::PayloadType::SendMediaStreamTick,
        RealtimeStreaming::Common::PayloadType::SendFormatChange,
        RealtimeStreaming::Common::PayloadType::ENDOFLIST, }};
};
template <> struct get_enumerator_names<RealtimeStreaming::Common::SampleFlags>
{
    static constexpr std::array<std::wstring_view, 7> value{{ 
        {L"SampleFlag_BottomFieldFirst", 27},
        {L"SampleFlag_CleanPoint", 21},
        {L"SampleFlag_DerivedFromTopField", 30},
        {L"SampleFlag_Discontinuity", 24},
        {L"SampleFlag_Interlaced", 21},
        {L"SampleFlag_RepeatFirstField", 27},
        {L"SampleFlag_SingleField", 22}, }};
};
template <> struct get_enumerator_values<RealtimeStreaming::Common::SampleFlags>
{
    static constexpr std::array<RealtimeStreaming::Common::SampleFlags, 7> value{{ 
        RealtimeStreaming::Common::SampleFlags::SampleFlag_BottomFieldFirst,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_CleanPoint,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_DerivedFromTopField,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_Discontinuity,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_Interlaced,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_RepeatFirstField,
        RealtimeStreaming::Common::SampleFlags::SampleFlag_SingleField, }};
};

}

WINRT_EXPORT namespace std {


}
