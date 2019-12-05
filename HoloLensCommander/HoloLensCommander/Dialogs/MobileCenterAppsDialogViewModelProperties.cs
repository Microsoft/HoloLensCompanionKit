using System.Collections.Generic;

namespace HoloLensCommander
{
    public partial class MobileCenterAppsDialogViewModel
    {
        private List<string> mobileCenterApps = new List<string>();
        public List<string> MobileCenterApps
        {
            get
            {
                return this.mobileCenterApps;
            }
            set
            {
                if (this.mobileCenterApps != value)
                {
                    this.mobileCenterApps = value;
                    this.NotifyPropertyChanged(nameof(MobileCenterApps));
                }
            }
        }

        private string selectedMobileCenterApp = string.Empty;
        public string SelectedMobileCenterApp
        {
            get
            {
                return this.selectedMobileCenterApp;
            }
            set
            {
                if (this.selectedMobileCenterApp!=value)
                {
                    this.selectedMobileCenterApp = value;
                    this.NotifyPropertyChanged(nameof(SelectedMobileCenterApp));
                }
            }
        }
    }
}