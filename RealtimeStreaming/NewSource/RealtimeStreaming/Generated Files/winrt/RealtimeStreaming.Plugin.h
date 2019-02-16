﻿// WARNING: Please don't edit this file. It was generated by C++/WinRT v1.0.180821.2

#pragma once

#include "winrt/base.h"


static_assert(winrt::check_version(CPPWINRT_VERSION, "1.0.180821.2"), "Mismatched component and base headers.");
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/impl/RealtimeStreaming.Plugin.2.h"

namespace winrt::impl {

template <typename D> void consume_RealtimeStreaming_Plugin_IModule<D>::Shutdown() const
{
    check_hresult(WINRT_SHIM(RealtimeStreaming::Plugin::IModule)->Shutdown());
}

template <typename D> uint32_t consume_RealtimeStreaming_Plugin_IModuleManager<D>::AddModule(RealtimeStreaming::Plugin::Module const& pluginModule) const
{
    uint32_t result{};
    check_hresult(WINRT_SHIM(RealtimeStreaming::Plugin::IModuleManager)->AddModule(get_abi(pluginModule), &result));
    return result;
}

template <typename D> RealtimeStreaming::Plugin::Module consume_RealtimeStreaming_Plugin_IModuleManager<D>::GetModule(uint32_t moduleHandle) const
{
    RealtimeStreaming::Plugin::Module result{ nullptr };
    check_hresult(WINRT_SHIM(RealtimeStreaming::Plugin::IModuleManager)->GetModule(moduleHandle, put_abi(result)));
    return result;
}

template <typename D> void consume_RealtimeStreaming_Plugin_IModuleManager<D>::ReleaseModule(uint32_t moduleHandle) const
{
    check_hresult(WINRT_SHIM(RealtimeStreaming::Plugin::IModuleManager)->ReleaseModule(moduleHandle));
}

template <typename D> RealtimeStreaming::Plugin::ModuleManager consume_RealtimeStreaming_Plugin_IPluginManager<D>::ModuleManager() const
{
    RealtimeStreaming::Plugin::ModuleManager value{ nullptr };
    check_hresult(WINRT_SHIM(RealtimeStreaming::Plugin::IPluginManager)->get_ModuleManager(put_abi(value)));
    return value;
}

template <typename D>
struct produce<D, RealtimeStreaming::Plugin::IModule> : produce_base<D, RealtimeStreaming::Plugin::IModule>
{
    int32_t WINRT_CALL Shutdown() noexcept final
    {
        try
        {
            typename D::abi_guard guard(this->shim());
            WINRT_ASSERT_DECLARATION(Shutdown, WINRT_WRAP(void));
            this->shim().Shutdown();
            return 0;
        }
        catch (...) { return to_hresult(); }
    }
};

template <typename D>
struct produce<D, RealtimeStreaming::Plugin::IModuleFactory> : produce_base<D, RealtimeStreaming::Plugin::IModuleFactory>
{};

template <typename D>
struct produce<D, RealtimeStreaming::Plugin::IModuleManager> : produce_base<D, RealtimeStreaming::Plugin::IModuleManager>
{
    int32_t WINRT_CALL AddModule(void* pluginModule, uint32_t* result) noexcept final
    {
        try
        {
            typename D::abi_guard guard(this->shim());
            WINRT_ASSERT_DECLARATION(AddModule, WINRT_WRAP(uint32_t), RealtimeStreaming::Plugin::Module const&);
            *result = detach_from<uint32_t>(this->shim().AddModule(*reinterpret_cast<RealtimeStreaming::Plugin::Module const*>(&pluginModule)));
            return 0;
        }
        catch (...) { return to_hresult(); }
    }

    int32_t WINRT_CALL GetModule(uint32_t moduleHandle, void** result) noexcept final
    {
        try
        {
            *result = nullptr;
            typename D::abi_guard guard(this->shim());
            WINRT_ASSERT_DECLARATION(GetModule, WINRT_WRAP(RealtimeStreaming::Plugin::Module), uint32_t);
            *result = detach_from<RealtimeStreaming::Plugin::Module>(this->shim().GetModule(moduleHandle));
            return 0;
        }
        catch (...) { return to_hresult(); }
    }

    int32_t WINRT_CALL ReleaseModule(uint32_t moduleHandle) noexcept final
    {
        try
        {
            typename D::abi_guard guard(this->shim());
            WINRT_ASSERT_DECLARATION(ReleaseModule, WINRT_WRAP(void), uint32_t);
            this->shim().ReleaseModule(moduleHandle);
            return 0;
        }
        catch (...) { return to_hresult(); }
    }
};

template <typename D>
struct produce<D, RealtimeStreaming::Plugin::IPluginManager> : produce_base<D, RealtimeStreaming::Plugin::IPluginManager>
{
    int32_t WINRT_CALL get_ModuleManager(void** value) noexcept final
    {
        try
        {
            *value = nullptr;
            typename D::abi_guard guard(this->shim());
            WINRT_ASSERT_DECLARATION(ModuleManager, WINRT_WRAP(RealtimeStreaming::Plugin::ModuleManager));
            *value = detach_from<RealtimeStreaming::Plugin::ModuleManager>(this->shim().ModuleManager());
            return 0;
        }
        catch (...) { return to_hresult(); }
    }
};

}

WINRT_EXPORT namespace winrt::RealtimeStreaming::Plugin {

inline PluginManager::PluginManager() :
    PluginManager(impl::call_factory<PluginManager>([](auto&& f) { return f.template ActivateInstance<PluginManager>(); }))
{}

}

namespace winrt::impl {

struct property_RealtimeStreaming_Plugin_IPluginManager
{ struct named {
    struct ModuleManager
    {
        struct name { static constexpr std::wstring_view value{ L"ModuleManager"sv }; };
        using property_type = winrt::RealtimeStreaming::Plugin::ModuleManager;
        using target_type = winrt::RealtimeStreaming::Plugin::IPluginManager;

        using is_readable = std::true_type;
        using is_writable = std::false_type;
        using is_static = std::false_type;
        struct getter
        {
            auto operator()(target_type const& target) const
            {
                return target.ModuleManager();
            }
        };
    };};
    struct list { using type = impl::typelist<named::ModuleManager>; };
};

struct property_RealtimeStreaming_Plugin_PluginManager
{ struct named {
    struct ModuleManager
    {
        struct name { static constexpr std::wstring_view value{ L"ModuleManager"sv }; };
        using property_type = winrt::RealtimeStreaming::Plugin::ModuleManager;
        using target_type = winrt::RealtimeStreaming::Plugin::PluginManager;

        using is_readable = std::true_type;
        using is_writable = std::false_type;
        using is_static = std::false_type;
        struct getter
        {
            auto operator()(target_type const& target) const
            {
                return target.ModuleManager();
            }
        };
    };};
    struct list { using type = impl::typelist<named::ModuleManager>; };
};

}

WINRT_EXPORT namespace winrt::experimental::reflect {
template <> struct named_property<RealtimeStreaming::Plugin::IPluginManager> : impl::property_RealtimeStreaming_Plugin_IPluginManager::named {};
template <> struct properties<RealtimeStreaming::Plugin::IPluginManager> : impl::property_RealtimeStreaming_Plugin_IPluginManager::list {};
template <> struct named_property<RealtimeStreaming::Plugin::PluginManager> : impl::property_RealtimeStreaming_Plugin_PluginManager::named {};
template <> struct properties<RealtimeStreaming::Plugin::PluginManager> : impl::property_RealtimeStreaming_Plugin_PluginManager::list {};
template <> struct get_enumerator_names<RealtimeStreaming::Plugin::DeviceType>
{
    static constexpr std::array<std::wstring_view, 3> value{{ 
        {L"Null", 4},
        {L"DX11", 4},
        {L"Warp", 4}, }};
};
template <> struct get_enumerator_values<RealtimeStreaming::Plugin::DeviceType>
{
    static constexpr std::array<RealtimeStreaming::Plugin::DeviceType, 3> value{{ 
        RealtimeStreaming::Plugin::DeviceType::Null,
        RealtimeStreaming::Plugin::DeviceType::DX11,
        RealtimeStreaming::Plugin::DeviceType::Warp, }};
};

}

WINRT_EXPORT namespace std {

template<> struct hash<winrt::RealtimeStreaming::Plugin::IModule> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::IModule> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::IModuleFactory> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::IModuleFactory> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::IModuleManager> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::IModuleManager> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::IPluginManager> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::IPluginManager> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::Module> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::Module> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::ModuleManager> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::ModuleManager> {};
template<> struct hash<winrt::RealtimeStreaming::Plugin::PluginManager> : winrt::impl::hash_base<winrt::RealtimeStreaming::Plugin::PluginManager> {};

}
