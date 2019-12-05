using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace HoloLensCommander
{
    public partial class MobileCenterAppsDialogViewModel
    {
        AppCenterWebRequest appCenterWebRequest = new AppCenterWebRequest();
        public ICommand DownloadInstallMobileAppsCommand
        { get; private set; }

        private async Task DownloadInstallMobileAppsAsync(AppResponse[] appList)
        {
            string selectedApp = this.SelectedMobileCenterApp as string;
            DownloadResponse downloadResponse;

            for (int i = 0; i < appList.Length; i++)
            {
                if (selectedApp == appList[i].name || selectedApp == appList[i].display_name)
                {
                    string requestResponseJson = await appCenterWebRequest.HttpReleaseRequest(appList[i].app_secret);
                    downloadResponse = JsonConvert.DeserializeObject<DownloadResponse>(requestResponseJson);
                    appCenterWebRequest.HttpDownloadRequest(downloadResponse.download_url, selectedApp);
                    break;  
                }  
            } 
        }
    }
}
