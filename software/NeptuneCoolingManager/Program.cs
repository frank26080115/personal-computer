using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

using HidLibrary;
using OpenHardwareMonitor;
using OpenHardwareMonitor.Hardware;
using System.Security.Permissions;
using System.Text;

namespace NeptuneCoolingManager
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            KillOtherProcesses();

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new TrayAppContext());
        }

        private static void KillOtherProcesses()
        {
            Process myself = Process.GetCurrentProcess();
            Process[] plist = Process.GetProcessesByName(myself.ProcessName);
            for (int i = 0; i < plist.Length; i++)
            {
                Process p = plist[i];
                if (p.Id != myself.Id)
                {
                    try
                    {
                        p.Kill();
                    }
                    catch
                    {

                    }
                }
            }
        }

        public class TrayAppContext : ApplicationContext
        {
            private const int VendorId = 0x12BA;
            private const int ProductId = 0x3333;

            private const float motherboard_temperature_limit = 50.0f;
            private const float motherboard_temperature_hysteresis = 5.0f;
            private const float motherboard_temperature_fan_factor = 0.75f;
            private const int motherboard_temperature_index = 1;

            private NotifyIcon trayicon;
            private Thread thread;
            private HidDevice usbdev = null;
            private Computer computer;
            private OpenHardwareMonitor2.Hardware.Computer computer2;
            // LibreHardwareMonitor does not support temperatures from NVMe, so a special fork/patch of OpenHardwareMonitor is used as well
            // the namespace is renamed to avoid conflicts

            private MenuItem menuitemModes;
            private MenuItem menuitemModeAuto;
            private MenuItem menuitemModeAlwaysOn;
            private MenuItem menuitemModeAlwaysLow;
            private MenuItem menuitemModeOff;
            private MenuItem menuitemLog;

            private bool isAttached = false;
            private int temperatureIdx;
            private List<float> temperatures = new List<float>();
            private List<string> temperatureKeys = new List<string>();

            private bool isPumpOn = false;
            private int fanRampUp = 0;

            private string logfile;

            public TrayAppContext()
            {
                menuitemModes = new MenuItem("Mode");
                
                menuitemModeAuto = new MenuItem("Mode: Auto", SetModeAuto);
                menuitemModeAlwaysOn = new MenuItem("Mode: Always On", SetModeAlwaysOn);
                menuitemModeAlwaysLow = new MenuItem("Mode: Always Low", SetModeAlwaysLow);
                menuitemModeOff = new MenuItem("Mode: Off", SetModeOff);

                menuitemLog = new MenuItem("Logging", SetLoggingOnOff);

                menuitemModeAuto.Checked = true;
                menuitemModeAuto.RadioCheck = true;

                menuitemModeOff.RadioCheck = true;
                menuitemModeOff.Checked = false;

                menuitemModeAlwaysOn.RadioCheck = true;
                menuitemModeAlwaysOn.Checked = false;

                menuitemModes.MenuItems.Add(menuitemModeAuto);
                menuitemModes.MenuItems.Add(menuitemModeAlwaysOn);
                menuitemModes.MenuItems.Add(menuitemModeAlwaysLow);
                menuitemModes.MenuItems.Add(menuitemModeOff);

                trayicon = new NotifyIcon()
                {
                    Icon = Resources.icon,
                    Text = Application.ProductName,
                    ContextMenu = new ContextMenu(new MenuItem[] {
                        menuitemModes,
                        menuitemLog,
                        new MenuItem("Diagnose", Diagnose),
                        new MenuItem("Exit", Exit),
                    }),
                    Visible = true
                };

                InitializeHardwareMonitor();

                thread = new Thread(new ThreadStart(Task));
                thread.Priority = ThreadPriority.Normal;
                thread.Start();
            }

            void InitializeHardwareMonitor()
            {
                computer = new Computer()
                {
                    MainboardEnabled = true,
                    CPUEnabled = true,
                    GPUEnabled = true,
                    //HDDEnabled = true,
                    //RAMEnabled = true,
                };
                // TODO: see if NVMe SSD temperature becomes supported later
                computer.Open();

                computer2 = new OpenHardwareMonitor2.Hardware.Computer()
                {
                    HDDEnabled = true,
                };
                computer2.Open();
            }

            void Task()
            {
                try
                {
                    while (true)
                    {
                        int sleepTime = 50;
                        try
                        {
                            UsbConnect();

                            ReadTemperatures();

                            if (menuitemLog.Checked)
                            {
                                int tsleep = 10;
                                while (sleepTime > tsleep)
                                {
                                    Thread.Sleep(tsleep * 100);
                                    sleepTime -= tsleep;
                                    ReadTemperatures();
                                }
                            }

                            if (usbdev != null)
                            {
                                if (usbdev.IsOpen)
                                {
                                    if (isAttached)
                                    {
                                        ControlFanSpeed();
                                    }
                                }
                            }
                        }
                        catch (ThreadAbortException ex)
                        {
                            throw ex;
                        }
                        catch (Exception ex)
                        {
                            try
                            {
                                usbdev.CloseDevice();
                                usbdev.Dispose();
                            }
                            catch
                            {

                            }
                            usbdev = null;
                            try
                            {
                                computer.Close();
                            }
                            catch
                            {

                            }
                            InitializeHardwareMonitor();
                        }

                        Thread.Sleep(sleepTime * 100);
                    }
                }
                catch (ThreadAbortException)
                {
                    return;
                }
            }

            private void UsbConnect()
            {
                if (usbdev == null)
                {
                    usbdev = HidDevices.Enumerate(VendorId, ProductId, 0xFFAB, 0x0200).FirstOrDefault();
                    // report ID and usage is from Teensy's raw HID code
                }
                if (usbdev != null)
                {
                    if (usbdev.IsOpen == false)
                    {
                        usbdev.OpenDevice();
                        if (usbdev.IsOpen)
                        {
                            usbdev.Inserted += Dev_InsertedHandler;
                            usbdev.Removed += Dev_RemovedHandler;
                            usbdev.MonitorDeviceEvents = true;
                            isAttached = true;
                        }
                    }
                    if (usbdev.IsOpen == false)
                    {
                        usbdev.Dispose();
                        usbdev = null;
                        // this whole function will be reattempted
                    }
                }
            }

            private void ControlFanSpeed()
            {
                float t = temperatures[motherboard_temperature_index];
                float tlim = motherboard_temperature_limit;
                bool pump = false;
                int fan = 0; // note that the MCU firmware expects numbers between 0 and 9
                bool overtemp = false;

                if (menuitemModeAuto.Checked)
                {
                    if (t > (tlim + (isPumpOn ? 0 : motherboard_temperature_hysteresis)))
                    {
                        // calculate an appropriate fan speed if motherboard temperature exceeds limit
                        pump = true;
                        int f = Convert.ToInt32(Math.Round((t - motherboard_temperature_limit) * motherboard_temperature_fan_factor));
                        f = f > 9 ? 9 : (f < 0 ? 0 : f);
                        fan = f;
                    }

                    // check for extra heavy loads, turn on full cooling if system is being stressed
                    float cpuThresh = 75;
                    float gpuThresh = 75;
                    float ssdThresh = 60;

                    for (int i = 0; i < temperatureKeys.Count; i++)
                    {
                        if (temperatureKeys[i].Contains("CPU"))
                        {
                            if (temperatures[i] > cpuThresh)
                            {
                                overtemp = true;
                            }
                        }
                        else if (temperatureKeys[i].Contains("GPU"))
                        {
                            if (temperatures[i] > gpuThresh)
                            {
                                overtemp = true;
                            }
                        }
                        else if (temperatureKeys[i].Contains("SSD"))
                        {
                            if (temperatures[i] > ssdThresh)
                            {
                                pump = true;
                                fan = fan < 1 ? 1 : fan;
                            }
                        }
                    }
                }

                if (overtemp || menuitemModeAlwaysOn.Checked)
                {
                    pump = true;
                    fan = 9;
                }
                else if (menuitemModeAlwaysLow.Checked)
                {
                    pump = true;
                    fan = 1;
                }

                // ramp up the fan slowly, since the noise is annoying and the thermal response is slow anyways
                if (fan > fanRampUp)
                {
                    fanRampUp++;
                }
                else if (fan < fanRampUp)
                {
                    // ramping down happens faster
                    fanRampUp--;
                    if (fan < fanRampUp)
                    {
                        fanRampUp--;
                    }
                }

                fanRampUp = fanRampUp > 9 ? 9 : (fanRampUp < 0 ? 0 : fanRampUp);
                if (pump == false)
                {
                    // no need to ramp down if pump isn't on, just let momentum take care of stopping slowly
                    fanRampUp = 0;
                }

                isPumpOn = pump;

                string msg = string.Format("P{0}F{1}", pump ? 9 : 0, fanRampUp);
                byte[] data = Encoding.ASCII.GetBytes(msg);

                // this seems like the best way to create the report object since it takes care of the report size
                HidReport report = usbdev.CreateReport();

                int ri;
                // copy the string into the report
                for (ri = 0; ri < report.Data.Length && ri < data.Length; ri++)
                {
                    report.Data[ri] = data[ri];
                }

                // fill the rest with zeros
                // I am unsure if this is required, so I explicitly do it just in case
                for (; ri < report.Data.Length; ri++)
                {
                    report.Data[ri] = 0;
                }

                // it seems like the only way to make sure this call works is to use the one with the async callback
                // the callback does nothing at all
                usbdev.WriteReport(report, Dev_WriteHandler, 0);
            }

            private void ReadTemperatures()
            {
                temperatureIdx = 0;
                foreach (var i in computer.Hardware)
                {
                    ReportSubHardware(i, null);
                }
                foreach (var i in computer2.Hardware)
                {
                    ReportSubHardware(i, null);
                }
                if (menuitemLog.Checked)
                {
                    try
                    {
                        if (File.Exists(logfile) == false)
                        {
                            string header = "Date, Time, Pump, Fan, ";
                            for (int i = 0; i < temperatures.Count; i++)
                            {
                                header += GetShortTempKey(temperatureKeys[i].Replace(',', ' ')) + ", ";
                            }
                            File.AppendAllText(logfile, header);
                        }

                        string entry = DateTime.Now.ToString("MM/dd/yyyy, HH:mm:ss.ff") + ", " + (isPumpOn ? "1" : "0") + ", " + fanRampUp.ToString() + ", ";

                        for (int i = 0; i < temperatures.Count; i++)
                        {
                            entry += temperatures[i].ToString("0.0") + ", ";
                        }
                        File.AppendAllText(logfile, Environment.NewLine + entry);
                    }
                    catch
                    {

                    }
                }
            }

            private void ReportSubHardware(object i, string parent)
            {
                string name = "";
                if (string.IsNullOrWhiteSpace(parent) == false)
                {
                    name = string.Format("{0} -> ", parent);
                }
                else
                {
                    parent = "";
                }

                MethodInfo updateMethod = i.GetType().GetMethod("Update");
                updateMethod.Invoke(i, null);

                string iname = (string)(i.GetType().GetProperty("Name").GetValue(i, null));

                name += (string)(i.GetType().GetProperty("Name").GetValue(i, null));

                IEnumerable<object> subhardware = (IEnumerable<object>)(i.GetType().GetProperty("SubHardware").GetValue(i, null));
                IEnumerable<object> sensors = (IEnumerable<object>)(i.GetType().GetProperty("Sensors").GetValue(i, null));

                foreach (var j in subhardware)
                {
                    ReportSubHardware(j, name);
                }
                foreach (var j in sensors)
                {
                    string jname = (string)(j.GetType().GetProperty("Name").GetValue(j, null));
                    int sensortype = (int)(j.GetType().GetProperty("SensorType").GetValue(j, null));
                    if (sensortype != (int)(OpenHardwareMonitor.Hardware.SensorType.Temperature))
                    {
                        continue;
                    }
                    float? x = (float?)(j.GetType().GetProperty("Value").GetValue(j, null));
                    if (x.HasValue)
                    {
                        string key = string.Format("{0} -> {1}", name, jname);

                        if (temperatures.Count <= temperatureIdx)
                        {
                            temperatures.Add(x.Value);
                            temperatureKeys.Add(key);
                        }
                        else
                        {
                            temperatures[temperatureIdx] = x.Value;
                            temperatureKeys[temperatureIdx] = key;
                        }
                        temperatureIdx++;
                    }
                }
            }


            private string GetShortTempKey(string x)
            {
                string n = x;
                string[] nparts = n.Split('>');
                if (nparts.Length >= 3)
                {
                    n = nparts[0].Trim('-', ' ');
                    n += " -> ";
                    n += nparts[2].Trim('-', ' ');
                }
                return n;
            }

            #region Event Handlers

            void Exit(object sender, EventArgs e)
            {
                trayicon.Visible = false; // this is required or else it lingers in the systray
                thread.Abort(); // required or else the process doesn't actually end

                try
                {
                    // free up the USB device if we grabbed it already
                    usbdev.CloseDevice();
                    usbdev.Dispose();
                }
                catch
                {

                }

                // quits the main thread
                Application.Exit();
            }

            void Diagnose(object sender, EventArgs e)
            {
                string msg = "";

                msg += "USB Device:";
                if (usbdev == null)
                {
                    msg += " NULL";
                }
                else
                {
                    if (usbdev.IsOpen == false)
                    {
                        msg += " closed";
                    }
                    else if (isAttached)
                    {
                        msg += " connected";
                    }
                    else
                    {
                        msg += " disconnected";
                    }
                }
                msg += Environment.NewLine;

                msg += "Temperatures:";
                msg += Environment.NewLine;

                for (int i = 0; i < temperatures.Count; i++)
                {
                    string n = GetShortTempKey(temperatureKeys[i]);
                    msg += string.Format("[{0}] {1}: {2} °C" + Environment.NewLine, i, n, temperatures[i]);
                }

                MessageBox.Show(msg, Application.ProductName, MessageBoxButtons.OK);
            }

            void SetModeAuto(object sender, EventArgs e)
            {
                menuitemModeAuto.Checked = true;
                menuitemModeAlwaysOn.Checked = false;
                menuitemModeAlwaysLow.Checked = false;
                menuitemModeOff.Checked = false;
            }

            void SetModeAlwaysOn(object sender, EventArgs e)
            {
                menuitemModeAuto.Checked = false;
                menuitemModeAlwaysOn.Checked = true;
                menuitemModeAlwaysLow.Checked = false;
                menuitemModeOff.Checked = false;
            }

            void SetModeAlwaysLow(object sender, EventArgs e)
            {
                menuitemModeAuto.Checked = false;
                menuitemModeAlwaysOn.Checked = false;
                menuitemModeAlwaysLow.Checked = true;
                menuitemModeOff.Checked = false;
            }

            void SetModeOff(object sender, EventArgs e)
            {
                menuitemModeAuto.Checked = false;
                menuitemModeAlwaysOn.Checked = false;
                menuitemModeAlwaysLow.Checked = false;
                menuitemModeOff.Checked = true;
            }

            void SetLoggingOnOff(object sender, EventArgs e)
            {
                menuitemLog.Checked = !menuitemLog.Checked;

                // start new file with timestamp as the name
                logfile = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location).TrimEnd('\\', '/') + Path.DirectorySeparatorChar + "log_" + DateTime.Now.ToString("yyyyMMddHHmmss") + ".txt";
            }

            private void Dev_RemovedHandler()
            {
                isAttached = false;
                usbdev.Dispose();
                usbdev = null; // triggers the search again
            }

            private void Dev_InsertedHandler()
            {
                isAttached = true;
            }

            private static void Dev_WriteHandler(bool success)
            {
                // do nothing
            }

            #endregion
        }
    }
}
