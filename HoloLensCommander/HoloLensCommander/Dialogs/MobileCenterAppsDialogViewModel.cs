﻿using Newtonsoft.Json;
using System.ComponentModel;
using System.Net;

namespace HoloLensCommander
{
    public partial class MobileCenterAppsDialogViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        public MobileCenterAppsDialogViewModel (AppResponse[] appResponseList)
        {
            for (int i = 0; i < appResponseList.Length; i++)
            {
                this.MobileCenterApps.Add(appResponseList[i].name);
            }
            
        }
        private void NotifyPropertyChanged(string propertyName)
        {
            this.PropertyChanged?.Invoke(
                this,
                new PropertyChangedEventArgs(propertyName));
        }
    }
}