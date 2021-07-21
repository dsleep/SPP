using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
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

using System.Text.Json;
using System.Text.Json.Serialization;


namespace RACApplication
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
        private BinaryReader mmVSBinaryRead = null;
        private BinaryWriter mmVSBinaryWrite = null;

        private UInt32 WorkerID = 0;

        private bool WorkerGood = false;
        private bool CoordGood = false;
        private bool StunGood = false;

#if DEBUG
        private const string DllFilePath = "SPPCored.dll";
#else
        private const string DllFilePath = "SPPCore.dll";
#endif

        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static UInt32 C_CreateChildProcess(string ProcessPath, string Commandline);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static bool C_IsChildRunning(UInt32 ProcessID);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static void C_CloseChild(UInt32 ProcessID);

        public MainWindow()
        {
            InitializeComponent();

            GoodState = new BitmapImage(new Uri(@"./images/buttongood.png", UriKind.Relative));
            BadState = new BitmapImage(new Uri(@"./images/buttonbad.png", UriKind.Relative));

            mmfGUID = Guid.NewGuid();
            string mmfGUIDStr = mmfGUID.ToString();
            Console.WriteLine("MMF: {0}", mmfGUIDStr);
            mmf = MemoryMappedFile.CreateNew(mmfGUIDStr, 2 * 1024 * 1024);
            mmVS = mmf.CreateViewStream();

            mmfMutex = new Mutex(false, mmfGUIDStr + "_M");
            mmVSBinaryRead = new BinaryReader(mmVS, Encoding.ASCII);
            mmVSBinaryWrite = new BinaryWriter(mmVS, Encoding.ASCII);

            DispatcherTimer timer = new DispatcherTimer();
            timer.Interval = TimeSpan.FromMilliseconds(250);
            timer.Tick += timer_Tick;
            timer.Start();
        }

        public class Host
        {
            public string NAME { get; set; }
            public string APPNAME { get; set; }
            public string GUID { get; set; }
        };

        public class AppStatus
        {
            public bool COORD { get; set; }
            public bool RESOLVEDSDP { get; set; }

            public List<Host> HOSTS { get; set; }
        };

        Dictionary<string, string> HostList = new Dictionary<string, string>();

        void timer_Tick(object sender, EventArgs e)
        {
            bool IsWorkValid = (WorkerID != 0 && C_IsChildRunning(WorkerID));

            if (WorkerGood != IsWorkValid)
            {
                WorkerGood = IsWorkValid;
                IMG_Worker.Source = WorkerGood ? GoodState : BadState;
            }

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
            }
            else
            {
                mmfMutex.WaitOne();

                mmVS.Seek(0, System.IO.SeekOrigin.Begin);
                var CurrentSize = mmVSBinaryRead.ReadUInt32();
                if (CurrentSize > 0)
                {
                    string JsonMemString = new string(mmVSBinaryRead.ReadChars((int)CurrentSize));
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

                    if (appStatus.HOSTS != null)
                    {
                        HostList.Clear();
                        foreach (var curHost in appStatus.HOSTS)
                        {
                            HostList[curHost.GUID] = String.Format("{0} Running... {1}", curHost.NAME, curHost.APPNAME);                                                    
                        }
                    }

                    if(LB_Servers.Items.Count != HostList.Count)
                    {
                        LB_Servers.Items.Clear();
                    }

                    foreach (var item in HostList)
                    {
                        var ItemString = String.Format("{0}:{1}", item.Key, item.Value);
                        if (!LB_Servers.Items.Contains(ItemString))
                        {
                            LB_Servers.Items.Add(ItemString);
                        }
                    }
                }

                mmfMutex.ReleaseMutex();
            }
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
        }

        private void BTN_Connect_Click(object sender, RoutedEventArgs e)
        {
            if (LB_Servers.SelectedItem != null)
            {
                mmfMutex.WaitOne();
                // go 1 meg in thats our writing space
                mmVS.Seek(1 * 1024 * 1024, System.IO.SeekOrigin.Begin);
                var ServerString = LB_Servers.SelectedItem.ToString();
                mmVSBinaryWrite.Write(ServerString.ToCharArray());    
                mmfMutex.ReleaseMutex();
            }
        }

        private void LB_Servers_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {

        }

        private void BTN_Disconnect_Click(object sender, RoutedEventArgs e)
        {

        }

        private void BTN_Start_Click(object sender, RoutedEventArgs e)
        {
            BTN_Start.IsEnabled = false;
#if DEBUG
            WorkerID = C_CreateChildProcess("remoteviewerd.exe", 
#else
            WorkerID = C_CreateChildProcess("remoteviewer.exe",
#endif
                "-MEM=" + mmfGUID.ToString());
        }

        private void BTN_Stop_Click(object sender, RoutedEventArgs e)
        {
            if(WorkerID != 0)
            {
                C_CloseChild(WorkerID);
                WorkerID = 0;

                BTN_Start.IsEnabled = true;
            }
        }
    }
}

