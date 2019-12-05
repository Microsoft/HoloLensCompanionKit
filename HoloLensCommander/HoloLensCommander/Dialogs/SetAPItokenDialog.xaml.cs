using System.Net;
using Windows.UI.Xaml.Controls;

// The Content Dialog item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

namespace HoloLensCommander
{
    public sealed partial class SetAPItokenDialog : ContentDialog
    {
        private UserToken apiToken;
        private NetworkCredential appCenterCredentials;

        public SetAPItokenDialog(UserToken APItoken, NetworkCredential AppCenterCredentials)
        {
            this.apiToken = APItoken;
            this.appCenterCredentials = AppCenterCredentials;
            this.DataContext = new SetAPItokenDialogViewModel(APItoken, AppCenterCredentials);
            this.InitializeComponent();
        }

        private void ContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            ((SetAPItokenDialogViewModel)this.DataContext).UpdateAPItoken(this.apiToken);
            ((SetAPItokenDialogViewModel)this.DataContext).UpdateAppCenterCredentials(this.appCenterCredentials);
        }

        private void ContentDialog_SecondaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
        }
    }
}
