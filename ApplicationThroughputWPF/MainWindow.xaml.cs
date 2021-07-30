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

using OxyPlot;
using OxyPlot.Series;

namespace ThroughTestApplication
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged
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

        private float LastNetUpdateTime = 0.0f;

#if DEBUG
        private const string DllFilePath = "SPPCored.dll";
#else
        private const string DllFilePath = "SPPCore.dll";
#endif

        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static UInt32 C_InitializeCore();
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static UInt32 C_CreateChildProcess(string ProcessPath, string Commandline, bool bStartVisible);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static bool C_IsChildRunning(UInt32 ProcessID);
        [DllImport(DllFilePath, CharSet = CharSet.Ansi)]
        private extern static void C_CloseChild(UInt32 ProcessID);

        public MainWindow()
        {
            this.IncomingKBs = new List<DataPoint>();
            this.OutgoingKBs = new List<DataPoint>();
            this.CCRateKBs = new List<DataPoint>();
            this.OutgoingKBBuffer = new List<DataPoint>();
            this.OutgoingMessageCount = new List<DataPoint>();

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

            string[] arguments = Environment.GetCommandLineArgs();

#if DEBUG
            if (arguments.Length == 2)
            {
                if(arguments[1] == "-startanother")
                {
                    C_CreateChildProcess("simpleconnectioncoordinatord.exe", "", true);
                    C_CreateChildProcess("AppThroughPut.exe", "", true);                    
                }
            }
#endif
        }

        //basic ViewModelBase
        internal void RaisePropertyChanged(string prop)
        {
            if (PropertyChanged != null) { PropertyChanged(this, new PropertyChangedEventArgs(prop)); }
        }
        public event PropertyChangedEventHandler PropertyChanged;

        public PlotModel MyModel 
        {
            get; 
            private set; 
        }

        public IList<DataPoint> IncomingKBs { get; private set; }
        public IList<DataPoint> OutgoingKBs { get; private set; }
        public IList<DataPoint> CCRateKBs { get; private set; }
        public IList<DataPoint> OutgoingKBBuffer { get; private set; }
        public IList<DataPoint> OutgoingMessageCount { get; private set; }
        
        public class Host
        {
            public string NAME { get; set; }
            public string GUID { get; set; }
        };

        public class AppStatus
        {
            public bool COORD { get; set; }
            public bool RESOLVEDSDP { get; set; }
            public bool CONNECTED { get; set; }

            public float OUTGOINGLIMITKBS { get; set; }
            public float INCOMINGKBS { get; set; }
            public float OUTGOINGKBS { get; set; }

            public float OUTGOINGBUFFERSIZEKB { get; set; }
            public int OUTGOINGMESSAGECOUNT { get; set; }

            public float UPDATETIME { get; set; }

            public List<Host> HOSTS { get; set; }
        };


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

                    if(LastNetUpdateTime != appStatus.UPDATETIME)
                    {
                        IncomingKBs.Add(new DataPoint(appStatus.UPDATETIME, appStatus.INCOMINGKBS));
                        OutgoingKBs.Add(new DataPoint(appStatus.UPDATETIME, appStatus.OUTGOINGKBS));
                        CCRateKBs.Add(new DataPoint(appStatus.UPDATETIME, appStatus.OUTGOINGLIMITKBS));
                        OutgoingKBBuffer.Add(new DataPoint(appStatus.UPDATETIME, appStatus.OUTGOINGBUFFERSIZEKB));
                        OutgoingMessageCount.Add(new DataPoint(appStatus.UPDATETIME, appStatus.OUTGOINGMESSAGECOUNT));                        

                        PL_Net.InvalidatePlot(true);
                        PL_Net_Buffers.InvalidatePlot(true);

                        LastNetUpdateTime = appStatus.UPDATETIME;
                    }

                    if (appStatus.HOSTS != null)
                    {                      
                        int currentIdx = 0;
                        foreach (var curHost in appStatus.HOSTS)
                        {
                            var dataString = String.Format("{0}:{1}", curHost.GUID, curHost.NAME);

                            if (currentIdx >= LB_Servers.Items.Count)
                            {
                                LB_Servers.Items.Add(dataString);
                            }
                            else
                            {
                                if(LB_Servers.Items[currentIdx].ToString() != dataString)
                                { 
                                    LB_Servers.Items[currentIdx] = dataString;
                                }
                            }
                            currentIdx++;
                        }
                    }

                    //
                    //this.Points.Add(new DataPoint(HardValue, 5));
                    //RaisePropertyChanged("Points");
                    //HardValue += 10;
                    //PL_Net.InvalidatePlot(true);
                }

                mmfMutex.ReleaseMutex();
            }            
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            IMG_Worker.Source = BadState;
            IMG_Coord.Source = BadState;
            IMG_Stun.Source = BadState;
        }

        private void button_Click(object sender, RoutedEventArgs e)
        {

        }

        private void BTN_Start_Click(object sender, RoutedEventArgs e)
        {
#if DEBUG
            WorkerID = C_CreateChildProcess("appTransferTestd.exe",
#else
            WorkerID = C_CreateChildProcess("appTransferTest.exe",
#endif
                "-MEM=" + mmfGUID.ToString(), true);
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
        }

        private void LB_Servers_SelectionChanged(object sender, SelectionChangedEventArgs e)
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

        private void BTN_Disconnect_Click(object sender, RoutedEventArgs e)
        {
            if (LB_Servers.SelectedItem != null)
            {
               
            }
        }

        private void BTN_Begin_Click(object sender, RoutedEventArgs e)
        {

        }

        private void BTN_End_Click(object sender, RoutedEventArgs e)
        {

        }
    }
}

