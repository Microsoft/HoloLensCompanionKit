using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HoloLensCommander
{
    public partial class SetAPItokenDialogViewModel
    {
        private string apiToken = string.Empty;
        public string APIToken
        {
            get
            {
                return this.apiToken;
            }
            set
            {
                if (this.apiToken!=value)
                {
                    this.apiToken = value;
                    this.NotifyPropertyChanged(nameof(APIToken));
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
                if (this.appCenterUserName!=value)
                {
                    this.appCenterUserName = value;
                    this.NotifyPropertyChanged(nameof(AppCenterUserName));
                }
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
                if (this.appCenterPassword != value)
                {
                    this.appCenterPassword = value;
                    this.NotifyPropertyChanged(nameof(AppCenterPassword));
                }
            }
        }
    }
}
