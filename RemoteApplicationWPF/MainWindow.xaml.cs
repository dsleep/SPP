using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;

using System.Diagnostics;
using System.Windows;
using System.Windows.Threading;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Drawing;
using System.Runtime.InteropServices;
using Microsoft.Win32;
using System.IO.MemoryMappedFiles;
using System.IO;

using System.ComponentModel;

using System.Text.Json;
using System.Text.Json.Serialization;


namespace RAMApplication
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private BitmapImage GoodState = null;
        private BitmapImage BadState = null;
        private MemoryMappedFile mmf = null;
        private MemoryMappedViewStream mmVS = null;
        private Mutex mmfMutex = null;
        private Guid mmfGUID;
        private BinaryReader mmVSBinar = null;

        private UInt32 WorkerID = 0;

        private bool WorkerGood = false;
        private bool CoordGood = false;
        private bool StunGood = false;
        private bool ClientGood = false;

#if DEBUG
        private const string DllFilePath = "SPPCored.dll";
#else
        private const string DllFilePath = "SPPCore.dll";
#endif



        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static void C_IntializeCore();
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static UInt32 C_CreateChildProcess(string ProcessPath, string Commandline, bool bStartVisible);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static bool C_IsChildRunning(UInt32 ProcessID);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static void C_CloseChild(UInt32 ProcessID);

        public MainWindow()
        {
            InitializeComponent();

#if DEBUG
            C_IntializeCore();
            C_CreateChildProcess("RAC.exe", "", true);
            C_CreateChildProcess("simpleconnectioncoordinatord.exe", "", true);
#endif

            GoodState = new BitmapImage(new Uri(@"./images/buttongood.png", UriKind.Relative));
            BadState = new BitmapImage(new Uri(@"./images/buttonbad.png", UriKind.Relative));

            mmfGUID = Guid.NewGuid();
            string mmfGUIDStr = mmfGUID.ToString();
            Console.WriteLine("MMF: {0}", mmfGUIDStr);
            mmf = MemoryMappedFile.CreateNew(mmfGUIDStr, 1 * 1024 * 1024);
            mmVS = mmf.CreateViewStream();

            mmfMutex = new Mutex(false, mmfGUIDStr + "_M");
            mmVSBinar = new BinaryReader(mmVS, Encoding.ASCII);

            DispatcherTimer timer = new DispatcherTimer();
            timer.Interval = TimeSpan.FromMilliseconds(250);
            timer.Tick += timer_Tick;
            timer.Start();
            
        }
       
        public class NetData
        {
            public float OUTGOINGKBS { get; set; }
            public float SECONDTIME { get; set; }
            public string KBSLIMIT { get; set; }
        };

        public class AppStatus
        {
            public bool COORD { get; set; }
            public bool RESOLVEDSDP { get; set; }
            public bool CONNECTED { get; set; }

            public NetData NETDATA { get; set; }
        };

        void timer_Tick(object sender, EventArgs e)
        {
            // if there is an ID but its not running set it to 0
            if(WorkerID != 0 && C_IsChildRunning(WorkerID) == false)
            {
                WorkerID = 0;
            }

            bool IsWorkValid = (WorkerID != 0);

            if (WorkerGood != IsWorkValid)
            {
                WorkerGood = IsWorkValid;
                IMG_Worker.Source = WorkerGood ? GoodState : BadState;
            }

            BTN_Stop.IsEnabled = WorkerGood;
            BTN_Start.IsEnabled = !WorkerGood;

            if (IsWorkValid == false)
            {          
                if (CoordGood != false)
                {
                    CoordGood = false;
                    IMG_Coord.Source = BadState;
                }

                if (StunGood != false)
                {
                    StunGood = false;
                    IMG_Stun.Source = BadState;
                }

                if (ClientGood != false)
                {
                    ClientGood = false;
                    IMG_Client.Source = BadState;
                }                
            }
            else
            {
                mmfMutex.WaitOne();

                mmVS.Seek(0, System.IO.SeekOrigin.Begin);
                var CurrentSize = mmVSBinar.ReadUInt32();
                if (CurrentSize > 0)
                {
                    string JsonMemString = new string(mmVSBinar.ReadChars((int)CurrentSize));
                    var appStatus = JsonSerializer.Deserialize<AppStatus>(JsonMemString);

                    if (CoordGood != appStatus.COORD)
                    {
                        CoordGood = appStatus.COORD;
                        IMG_Coord.Source = CoordGood ? GoodState : BadState;
                    }

                    if (StunGood != appStatus.RESOLVEDSDP)
                    {
                        StunGood = appStatus.RESOLVEDSDP;
                        IMG_Stun.Source = StunGood ? GoodState : BadState;
                    }

                    if (ClientGood != appStatus.CONNECTED)
                    {
                        ClientGood = appStatus.CONNECTED;
                        IMG_Client.Source = ClientGood ? GoodState : BadState;
                    }          

                }

                mmfMutex.ReleaseMutex();
            }            
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            TB_AppPath.Text = Properties.Settings.Default.ApplicationPath;
            TB_Args.Text = Properties.Settings.Default.ApplicationCommandline;

            IMG_Worker.Source = BadState;
            IMG_Coord.Source = BadState;
            IMG_Stun.Source = BadState;
            IMG_Client.Source = BadState;

            BTN_Stop.IsEnabled = false;
        }

        private void button_Click(object sender, RoutedEventArgs e)
        {

        }

        private void AppBrowse_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();            
            openFileDialog.Filter = "Application files (*.exe)|*.exe|All files (*.*)|*.*";
            openFileDialog.InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            if (openFileDialog.ShowDialog() == true)
            {
                TB_AppPath.Text = openFileDialog.FileName;
            }
        }

        private void BTN_Start_Click(object sender, RoutedEventArgs e)
        {
            if (WorkerID == 0)
            {
#if DEBUG
                WorkerID = C_CreateChildProcess("applicationhostd.exe",
#else
                WorkerID = C_CreateChildProcess("applicationhost.exe",
#endif
                "-MEM=" + mmfGUID.ToString() +
                    " -APP=\"" + TB_AppPath.Text + "\"", false);

                BTN_Start.IsEnabled = false;
            }
        }

        private void BTN_Stop_Click(object sender, RoutedEventArgs e)
        {
            if (WorkerID != 0)
            {
                C_CloseChild(WorkerID);
                WorkerID = 0;
            }
        }

        private void TB_Args_TextChanged(object sender, TextChangedEventArgs e)
        {

        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            Properties.Settings.Default.ApplicationPath = TB_AppPath.Text;
            Properties.Settings.Default.ApplicationCommandline = TB_Args.Text;
            Properties.Settings.Default.Save();
        }
    }
}

