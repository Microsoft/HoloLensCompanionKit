// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace HoloLensCommander
{
    /// <summary>
    /// The view model for the MainPage.
    /// </summary>
    partial class MainWindowViewModel
    {
        /// <summary>
        /// Gets a value indicating whether or not all devices are selected.
        /// </summary>
        /// <remarks>A value of null indicates that more than 0 and less than the total number of devices is selected.</remarks>
        private bool? allDevicesSelected = false;
        public bool? AllDevicesSelected
        {
            get
            {
                return this.allDevicesSelected;
            }

            private set
            {
                if (this.allDevicesSelected != value)
                {
                    this.allDevicesSelected = value;
                    this.NotifyPropertyChanged("AllDevicesSelected");
                }
            }
        }

        /// <summary>
        /// Gets or sets the API Token to be used when connecting to mobile center.
        /// </summary>
        private string apiToken = string.Empty;
        public string ApiToken
        {
            get
            {
                return this.apiToken;
            }

            set
            {
                if (this.apiToken != value)
                {
                    this.apiToken = value;
                    this.UpdateAPItoken();
                }
            }
        }

        private bool apiTokenSet = false;
        public bool ApiTokenSet
        {
            get
            {
                return this.apiTokenSet;
            }
            set
            {
                if (apiTokenSet!=value)
                {
                    apiTokenSet = value;
                    this.NotifyPropertyChanged("APItokenSet");
                }
            }
        }

        private bool appCenterCredentialsSet = false;
        public bool AppCenterCredentialsSet
        {
            get
            {
                return this.appCenterCredentialsSet;
            }

            private set
            {
                if (this.appCenterCredentialsSet != value)
                {
                    this.appCenterCredentialsSet = value;
                    this.NotifyPropertyChanged("AppCenterCredentialsSet");
                }
            }
        }

        private string appCenterUserName = string.Empty;
        public string AppCenterUserName
        {
            get
            {
                return this.appCenterUserName;
            }
            set
            {
                appCenterUserName = value;
            }
        }

        private string appCenterPassword = string.Empty;
        public string AppCenterPassword
        {
            get
            {
                return this.appCenterPassword;
            }
            set
            {
                appCenterPassword = value;
            }
        }

        /// <summary>
        /// Gets a value indicating whether or not the application management controls are to be enabled.
        /// </summary>
        private bool canManageApps = false;
        public bool CanManageApps
        {
            get
            {
                return this.canManageApps;
            }

            private set
            {
                if (canManageApps != value)
                {
                    canManageApps = value;
                    this.NotifyPropertyChanged("CanManageApps");
                }
            }
        }

        /// <summary>
        /// Gets a value indicating whether or not the restore connections controls are to be enabled.
        /// </summary>
        private bool canReconnectDevices = false;
        public bool CanReconnectDevices
        {
            get
            {
                return this.canReconnectDevices;
            }

            set
            {
                if (this.canReconnectDevices != value)
                {
                    this.canReconnectDevices = value;
                    NotifyPropertyChanged("CanReconnectDevices");
                }
            }
        }

        /// <summary>
        /// Gets the collection of applications that are common to all registered devices.
        /// </summary>
        public ObservableCollection<string> CommonDeviceApps
        { get; private set; }

        /// <summary>
        /// Gets a value indicating whether or not both the username and password boxes contain valid (non-whitespace) strings.
        /// </summary>
        private bool credentialsSet = false;
        public bool CredentialsSet
        {
            get
            {
                return this.credentialsSet;
            }

            private set
            {
                if (this.credentialsSet != value)
                {
                    this.credentialsSet = value;
                    this.NotifyPropertyChanged("CredentialsSet");
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether or not mixed reality files should be deleted after they have been saved.
        /// </summary>
        private bool deleteMixedRealityFilesAfterSave = false;
        public bool DeleteMixedRealityFilesAfterSave
        {
            get
            {
                return this.deleteMixedRealityFilesAfterSave;
            }

            set
            {
                if (deleteMixedRealityFilesAfterSave != value)
                {
                    deleteMixedRealityFilesAfterSave = value;
                    this.NotifyPropertyChanged("DeleteMixedRealityFilesAfterSave");
                }
            }
        }

        /// <summary>
        /// Gets a value indicating whether or not one or more devices have been registered.
        /// </summary>
        private bool haveRegisteredDevices = false;
        public bool HaveRegisteredDevices
        {
            get
            {
                return this.haveRegisteredDevices;
            }

            private set
            {
                if (this.haveRegisteredDevices != value)
                {
                    this.haveRegisteredDevices = value;
                    this.NotifyPropertyChanged("HaveRegisteredDevices");
                    this.UpdateCanReconnect();
                }
            }
        }

        /// <summary>
        /// Gets or sets the password to be used when connecting to a device.
        /// </summary>
        private string password = string.Empty;
        private string Password
        {
            get
            {
                return this.password;
            }

            set
            {
                if (this.password != value)
                {
                    this.password = value;
                    this.UpdateCredentialsSet();
                }
            }
        }

        /// <summary>
        /// Gets the collection of registered DeviceMonitorControls.
        /// </summary>
        public ObservableCollection<DeviceMonitorControl> RegisteredDevices
        { get; private set; }

        public DeviceFilters SelectionFilter
        { get; private set; }

        /// <summary>
        /// Gets or sets the user name to be used when connecting to a device.
        /// </summary>
        private string userName = string.Empty;
        private string UserName
        {
            get
            {
                return this.userName;
            }

            set
            {
                if (this.userName != value)
                {
                    this.userName = value;
                    this.UpdateCredentialsSet();
                }
            }
        }

        /// <summary>
        /// Gets or sets the currently selected application.
        /// </summary>
        public object selectedApp = null;
        public object SelectedApp
        {
            get
            {
                return this.selectedApp;
            }

            set
            {
                if (this.selectedApp != value)
                {
                    this.selectedApp = value;
                    this.NotifyPropertyChanged("SelectedApp");
                }
            }
        }

        /// <summary>
        /// Gets the message to be displayed to the user.
        /// </summary>
        public string statusMessage = null;
        public string StatusMessage
        {
            get
            {
                return this.statusMessage;
            }

           private set
            {
                if (this.statusMessage != value)
                {
                    this.statusMessage = value;
                    this.NotifyPropertyChanged("StatusMessage");
                }
            }
        }

        private void UpdateAPItoken()
        {
            bool haveAPItoken = false;

            if (!string.IsNullOrWhiteSpace(this.ApiToken))
            {
                haveAPItoken = true;
            }

            this.ApiTokenSet = haveAPItoken;
        }

        /// <summary>
        /// Updates whether or not connections are safe to be restored.
        /// </summary>
        private void UpdateCanReconnect()
        {
            // We can reconnect if:
            if (
                // * we have valid credentials
                this.CredentialsSet &&
                // * no connections currently exist
                !this.HaveRegisteredDevices &&
                // * we have not previously attempted to reconnect
                !this.reconnected)
            {
                this.CanReconnectDevices = true;
            }
            else
            {
                this.CanReconnectDevices = false;
            }
        }

        private void UpdateAppCenterCredentials()
        {
            bool haveAppCenterCredentials = false;

            if (!string.IsNullOrWhiteSpace(this.AppCenterUserName) && !string.IsNullOrWhiteSpace(this.AppCenterPassword))
            {
                haveAppCenterCredentials = true;
            }
            this.AppCenterCredentialsSet = haveAppCenterCredentials;
            this.UpdateCanReconnect();
        }

        /// <summary>
        /// Updates whether or not credentials have been set to valid strings.
        /// </summary>
        private void UpdateCredentialsSet()
        {
            bool haveCredentials = false;

            if (!string.IsNullOrWhiteSpace(this.UserName) &&
                !string.IsNullOrWhiteSpace(this.Password))
            {
                haveCredentials = true;
            }

            this.CredentialsSet = haveCredentials;

            this.UpdateCanReconnect();
        }
    }
}
