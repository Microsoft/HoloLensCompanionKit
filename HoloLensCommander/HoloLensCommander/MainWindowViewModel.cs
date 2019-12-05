// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using Windows.Storage;
using Windows.UI.Core;

namespace HoloLensCommander
{
    /// <summary>
    /// The view model for the MainPage.
    /// </summary>
    partial class MainWindowViewModel : INotifyPropertyChanged, IDisposable
    {
        /// <summary>
        /// Values used to store and retrieve settings data.
        /// </summary>
        private static readonly string AutoReconnectKey = "autoReconnect";
        private static readonly string DefaultAPItokenKey = "defaultAPItokenKey";
        private static readonly string DefaultAppCenterUserNameKey = "defaultAppCenterUserName";
        private static readonly string DefaultAppCenterPasswordKey = "defaultAppCenterPassword";
        private static readonly string DefaultNetworkKeyKey = "defaultNetworkKey";
        private static readonly string DefaultPasswordKey = "defaultPassword";
        private static readonly string DefaultSsidKey = "defaultSsid";
        private static readonly string DefaultUserNameKey = "defaultUserName";
        private static readonly string ExpandCredentialsKey = "expandCredentials";
        private static readonly string ExpandNetworkSettingsKey = "expandNetworkSettings";
        private static readonly string HeartbeatIntervalKey = "heartbeatInterval";
        private static readonly string UseInstalledCertificateKey = "useInstalledCertificate";

        /// <summary>
        /// Name of the folder which will contain mixed reality files from the registered devices.
        /// </summary>
        private static readonly string MixedRealityFilesFolderName = "HoloLensCommander";

        /// <summary>
        /// Dispatcher used to ensure notifications happen on the correct thread.
        /// </summary>
        private CoreDispatcher dispatcher;

        /// <summary>
        /// The application settings container.
        /// </summary>
        private ApplicationDataContainer appSettings;

        /// <summary>
        /// Should we automatically reconnect to the previous device session?
        /// </summary>
        private bool autoReconnect;

        /// <summary>
        /// The network key for the default SSID.
        /// </summary>
        private string defaultNetworkKey;

        /// <summary>
        /// The default network access point SSID to use when connecting devices.
        /// </summary>
        private string defaultSsid;

        /// <summary>
        /// Should the connect dialog auto-expand the credentials controls?
        /// </summary>
        private bool expandCredentials;

        /// <summary>
        /// Should the connect dialog auto-expand the network settings controls?
        /// </summary>
        private bool expandNetworkSettings;

        /// <summary>
        /// The time, in seconds, between a device's heartbeat check.
        /// </summary>
        private float heartbeatInterval;

        /// <summary>
        /// Value indicating whether or not we have attempted to reconnect to previous devices.
        /// </summary>
        private bool reconnected;

        /// <summary>
        /// Value indicating whether or not the common apps list should be refreshed.
        /// </summary>
        private bool suppressRefreshCommonApps;

        /// <summary>
        /// Value indicating whether or not connection changes are to be saved.
        /// </summary>
        private bool suppressSave;

        /// <summary>
        /// Should connections be established using an installed device certificate?
        /// </summary>
        private bool useInstalledCertificate;

        /// <summary>
        /// The local application folder.
        /// </summary>
        private StorageFolder localFolder;

        /// <summary>
        /// Event that is notified when a property value has changed.
        /// </summary>
        public event PropertyChangedEventHandler PropertyChanged;

        /// <summary>
        /// Initializes a new instance of the <see cref="MainWindowViewModel" /> class.
        /// </summary>
        public MainWindowViewModel(CoreDispatcher coreDispatcher)
        {
            this.dispatcher = coreDispatcher;

            this.reconnected = false;
            this.suppressRefreshCommonApps = false;
            this.suppressSave = false;

            this.CommonDeviceApps = new ObservableCollection<string>();
            this.RegisteredDevices = new ObservableCollection<DeviceMonitorControl>();

            this.localFolder = ApplicationData.Current.LocalFolder;

            this.SelectionFilter = DeviceFilters.All;

            // Fetch stored settings.
            this.appSettings = Windows.Storage.ApplicationData.Current.LocalSettings;
            this.LoadApplicationSettings();
            
            if (this.autoReconnect)
            {
                this.ReconnectPreviousSession();    
            }
            this.UpdateCanReconnect();

            this.RegisterCommands();

        }

        /// <summary>
        /// Finalizer so that we are assured we clean up all encapsulated resources.
        /// </summary>
        /// <remarks>Call Dispose on this object to avoid running the finalizer.</remarks>
        ~MainWindowViewModel()
        {
            this.Dispose();
        }

        /// <summary>
        /// Cleans up objects managed by the MainWindowViewModel.
        /// </summary>
        /// <remarks>
        /// Failure to call this method will result in the object not being collected until
        /// finalization occurs.
        /// </remarks>
        public void Dispose()
        {
            foreach (DeviceMonitorControl monitor in this.GetCopyOfRegisteredDevices())
            {
                monitor.Dispose();
            }
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Get a copy of the registered device controls collection.
        /// </summary>
        /// <returns>List of DeviceMonitorControl objects.</returns>
        private List<DeviceMonitorControl> GetCopyOfRegisteredDevices()
        {
            List<DeviceMonitorControl> registeredDevicesCopy = new List<DeviceMonitorControl>();

            registeredDevicesCopy.AddRange(this.RegisteredDevices);

            return registeredDevicesCopy;
        }

        /// <summary>
        /// Sends the PropertyChanged events to registered handlers.
        /// </summary>
        /// <param name="propertyName">The name of property that has changed.</param>
        private void NotifyPropertyChanged(string propertyName)
        {
            this.PropertyChanged?.Invoke(
                this, 
                new PropertyChangedEventArgs(propertyName));
        }

        /// <summary>
        /// Registers commands supported by this object.
        /// </summary>
        private void RegisterCommands()
        {
            this.ClearDeviceStatusCommand = new Command(
                async (parameter) =>
                {
                    await this.ClearDeviceStatus();
                });

            this.ClearStatusMessageCommand = new Command(
                (parameter) =>
                {
                    this.ClearStatusMessage();
                });

            this.CloseAllAppsCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.CloseAllApps();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to close all apps on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.ConnectToDeviceCommand = new Command(
                async (parameter) =>
                {
                    ConnectOptions connectOptions = new ConnectOptions(
                        string.Empty,
                        string.Empty,
                        this.UserName,
                        this.Password);
                    // TODO: use the full featured constructor
                    connectOptions.ExpandCredentials = this.expandCredentials;
                    connectOptions.ExpandNetworkSettings = this.expandNetworkSettings;
                    connectOptions.UseInstalledCertificate = this.useInstalledCertificate;
                    connectOptions.Ssid = this.defaultSsid;
                    connectOptions.NetworkKey = this.defaultNetworkKey;

                    try
                    {
                        await this.ConnectToDeviceAsync(
                            connectOptions);
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to connect to the device ({0})",
                            e.Message);
                    }
                });

            this.DeselectAllDevicesCommand = new Command(
                (parameter) =>
                {
                    this.DeselectAllDevices();
                });

            this.ForgetConnectionsCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.ForgetAllConnectionsAsync();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to unregister devices {0}",
                            e.Message);
                    }
                });

            this.InstallAppCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.InstallAppAsync();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to install the app on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.LaunchAppCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.LaunchApp();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to launch the app on one or more devices ({0})",
                            e.Message);
                    }
                    this.LaunchApp();
                });

            this.LoadSessionFileCommand = new Command(
                async (parameter) =>
                {
                    await this.LoadSessionFileAsync(); 
                });

            this.RebootDevicesCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.RebootDevicesAsync();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to reboot one or more devices ({0})",
                            e.Message);
                    }
                });

            this.ReconnectPreviousSessionCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.ReconnectPreviousSession();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to reconnect previous device session ({0})",
                            e.Message);
                    }
                });

            this.RefreshCommonAppsCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.RefreshCommonAppsAsync();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to refresh common applications ({0})",
                            e.Message);
                    }
                });

            this.SaveMixedRealityFilesCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.SaveMixedRealityFiles();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to save Mixed Reality files from one or more devices ({0})",
                            e.Message);
                    }
                });

            this.SaveSessionFileCommand = new Command(
                async (parameter) =>
                {
                    await this.SaveSessionFile();
                });

            this.SelectAllDevicesCommand = new Command(
                (parameter) =>
                {
                    this.SelectAllDevices();
                });

            this.ShowMobileCenterAppsCommand = new Command(
                async (parameter) =>
                {
                    await this.ShowMobileCenterApps();
                });

            this.ShowSetAPItokenCommand = new Command(
                async (parameter) =>
                {
                    await this.ShowSetAPItoken();
                });

            this.ShowSetCredentialsCommand = new Command(
                async (parameter) =>
                {
                    await this.ShowSetCredentials();
                });

            this.ShowSettingsCommand = new Command(
                async (paraneter) =>
                {
                    await this.ShowSettings();
                });

            this.ShutdownDevicesCommand = new Command(
                async (parameter) =>
                {
                    try
                    {
                        await this.ShutdownDevicesAsync();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to shutdown one or more devices ({0})",
                            e.Message);
                    }
                });

            this.StartMixedRealityRecordingCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.StartMixedRealityRecording();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to start Mixed Reality recording on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.StopMixedRealityRecordingCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.StopMixedRealityRecording();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to stop Mixed Reality recording on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.UninstallAppCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.UninstallApp();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to uninstall an app on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.UninstallAllAppsCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.UninstallAllApps();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to uninstall all apps on one or more devices ({0})",
                            e.Message);
                    }
                });

            this.UseAllDevicesFilterCommand = new Command(
                (parameter) =>
                {
                    this.SelectionFilter = DeviceFilters.All;
                });

            this.UseDesktopFilterCommand = new Command(
                (parameter) =>
                {
                    this.SelectionFilter = DeviceFilters.Desktop;
                });

            this.UseHoloLensFilterCommand = new Command(
                (parameter) =>
                {
                    this.SelectionFilter = DeviceFilters.HoloLens;
                });

            this.WipeCameraRollCommand = new Command(
                (parameter) =>
                {
                    try
                    {
                        this.WipeCameraRoll();
                    }
                    catch (Exception e)
                    {
                        this.StatusMessage = string.Format(
                            "Failed to clear the camera roll on one or more devices ({0})",
                            e.Message);
                    }
                });
        }

        /// <summary>
        /// Updates the UI with the list of applications installed on all registered devices.
        /// </summary>
        /// <param name="appNames">List of application names.</param>
        private void UpdateCommonAppsCollection(List<string> appNames)
        {
            // Get the currently selected application.
            string currentSelection = this.SelectedApp as string;

            this.CommonDeviceApps.Clear();
            foreach (string name in appNames)
            {
                this.CommonDeviceApps.Add(name);
            }

            this.CanManageApps = false;
            if (this.CommonDeviceApps.Count > 0)
            {
                // Set the selected item.
                if ((currentSelection != null) &&
                    this.CommonDeviceApps.Contains(currentSelection))
                {
                    this.SelectedApp = currentSelection;
                }
                else
                {
                    this.SelectedApp = this.CommonDeviceApps[0];
                }

                this.CanManageApps = true;
            }
        }
    }
}
