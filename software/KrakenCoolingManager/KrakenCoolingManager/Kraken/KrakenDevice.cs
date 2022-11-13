using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using KrakenCoolingManager.UI;
using HidLibrary;
using System.Diagnostics;
using LibreHardwareMonitor.Hardware;
using KrakenCoolingManager.Wmi;
using System.Net;
using KrakenCoolingManager.Utilities;

namespace KrakenCoolingManager
{
    public class KrakenDevice
    {
        private const int VendorId = 0x12BA;
        private const int ProductId = 0x4444;
        private HidDevice _usbdev = null;

        private static List<SensorNode> allFanControls = new List<SensorNode>();
        public static List<SensorNode> AllFanControls
        {
            get
            {
                return allFanControls;
            }
        }

        private static List<SensorNode> allPowerNodes = new List<SensorNode>();
        public static List<SensorNode> AllPowerNodes
        {
            get
            {
                return allPowerNodes;
            }
        }

        private static List<SensorNode> allTemperatureSensors = new List<SensorNode>();
        public static List<SensorNode> AllTemperatureSensors
        {
            get
            {
                return allTemperatureSensors;
            }
        }

        enum CoolingMode
        {
            Off,
            Auto,
            Low,
            Full,
            Shutdown,
        };

        enum DeviceCommandByte : byte
        {
            Both = 0,
            Fan = 1,
            Pump = 2,
            Shutdown = 3,
        };

        private CoolingMode _coolingMode;

        private System.Windows.Forms.Timer _bgTimer;

        private ToolStripMenuItem _menuItemCoolingDevice;
        private ToolStripMenuItem _menuItemCoolingOff;
        private ToolStripMenuItem _menuItemCoolingAuto;
        private ToolStripMenuItem _menuItemCoolingLow;
        private ToolStripMenuItem _menuItemCoolingFull;
        private ToolStripMenuItem _menuItemCoolingShutdown;

        private float _fanMin = 33;
        private float _fanMax = 66;
        private float _pwrMin = 30;
        private float _pwrMax = 100;
        private float _tempMin = 60;
        private float _tempMax = 80;

        private bool IsDeviceConnected
        {
            get
            {
                if (_usbdev == null)
                {
                    return false;
                }
                return _usbdev.IsOpen && _usbdev.IsConnected;
            }
        }

        public KrakenDevice()
        {
            _bgTimer = new System.Windows.Forms.Timer();
            _bgTimer.Interval = 2500;
            _bgTimer.Tick += bgTimer_Tick;

            bool need_save = false;
            need_save |= LoadFloatSetting("KrakenFanMin", ref _fanMin);
            need_save |= LoadFloatSetting("KrakenFanMax", ref _fanMin);
            need_save |= LoadFloatSetting("KrakenPwrMin", ref _fanMin);
            need_save |= LoadFloatSetting("KrakenPwrMax", ref _fanMax);
            need_save |= LoadFloatSetting("KrakenTempMin", ref _fanMin);
            need_save |= LoadFloatSetting("KrakenTempMax", ref _fanMax);
            if (need_save)
            {
                Program.MainForm.SaveConfiguration();
            }

            List<ToolStripItem> moreItems = new List<ToolStripItem>();

            _menuItemCoolingDevice = new ToolStripMenuItem("Kraken Device - ?");
            _menuItemCoolingDevice.Click += delegate
            {
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingDevice);

            _menuItemCoolingOff = new ToolStripMenuItem("Cooling - OFF");
            _menuItemCoolingOff.CheckOnClick = true;
            _menuItemCoolingOff.Click += delegate
            {
                _menuItemCoolingOff.Checked = true;
                _coolingMode = CoolingMode.Off;
                _menuItemCoolingLow.Checked = false;
                _menuItemCoolingFull.Checked = false;
                _menuItemCoolingAuto.Checked = false;
                _menuItemCoolingShutdown.Checked = false;
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingOff);

            _menuItemCoolingAuto = new ToolStripMenuItem("Cooling - Auto");
            _menuItemCoolingAuto.CheckOnClick = true;
            _menuItemCoolingAuto.Checked = true;
            _coolingMode = CoolingMode.Auto;
            _menuItemCoolingAuto.Click += delegate
            {
                _menuItemCoolingAuto.Checked = true;
                _coolingMode = CoolingMode.Auto;
                _menuItemCoolingLow.Checked = false;
                _menuItemCoolingFull.Checked = false;
                _menuItemCoolingOff.Checked = false;
                _menuItemCoolingShutdown.Checked = false;
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingAuto);

            _menuItemCoolingLow = new ToolStripMenuItem("Cooling - Low");
            _menuItemCoolingLow.CheckOnClick = true;
            _menuItemCoolingLow.Click += delegate
            {
                _menuItemCoolingLow.Checked = true;
                _coolingMode = CoolingMode.Low;
                _menuItemCoolingAuto.Checked = false;
                _menuItemCoolingOff.Checked = false;
                _menuItemCoolingFull.Checked = false;
                _menuItemCoolingShutdown.Checked = false;
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingLow);

            _menuItemCoolingFull = new ToolStripMenuItem("Cooling - Full");
            _menuItemCoolingFull.CheckOnClick = true;
            _menuItemCoolingFull.Click += delegate
            {
                _menuItemCoolingFull.Checked = true;
                _coolingMode = CoolingMode.Full;
                _menuItemCoolingAuto.Checked = false;
                _menuItemCoolingOff.Checked = false;
                _menuItemCoolingLow.Checked = false;
                _menuItemCoolingShutdown.Checked = false;
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingFull);

            _menuItemCoolingShutdown = new ToolStripMenuItem("Kraken Shutdown");
            _menuItemCoolingShutdown.CheckOnClick = true;
            _menuItemCoolingShutdown.Click += delegate
            {
                _menuItemCoolingShutdown.Checked = true;
                _coolingMode = CoolingMode.Shutdown;
                _menuItemCoolingAuto.Checked = false;
                _menuItemCoolingOff.Checked = false;
                _menuItemCoolingLow.Checked = false;
                _menuItemCoolingFull.Checked = false;
                _bgTimer.Interval = 1;
            };
            moreItems.Add(_menuItemCoolingShutdown);
            moreItems.Add(new ToolStripSeparator());

            for (int i = 0; i < moreItems.Count; i++)
            {
                Program.SysTray.MainIcon.ContextMenuStrip.Items.Insert(i, moreItems[i]);
            }

            _bgTimer.Enabled = true;
        }

        private bool LoadFloatSetting(string key, ref float x)
        {
            bool need_save = false;
            if (Program.PersistSettings.Contains(key))
            {
                try
                {
                    x = Convert.ToSingle(Program.PersistSettings.GetValue(key, Convert.ToInt32(Math.Round(x)).ToString()));
                }
                catch
                {
                    Program.PersistSettings.SetValue(key, Convert.ToInt32(Math.Round(x)).ToString());
                    need_save = true;
                }
            }
            else
            {
                Program.PersistSettings.SetValue(key, Convert.ToInt32(Math.Round(x)).ToString());
                need_save = true;
            }
            return need_save;
        }

        public static void CheckAddSensor(SensorNode n)
        {
            if (n.Sensor.Name.ToLower().StartsWith("fan") && n.Sensor.SensorType == SensorType.Control)
            {
                KrakenDevice.AllFanControls.Add(n);
            }

            if ((n.Sensor.Identifier.ToString().ToLower().Contains("cpu") || n.Sensor.Identifier.ToString().ToLower().Contains("gpu")) && n.Sensor.SensorType == SensorType.Power)
            {
                KrakenDevice.AllPowerNodes.Add(n);
            }

            if (n.Sensor.Identifier.ToString().ToLower().Contains("cpu") && n.Sensor.SensorType == SensorType.Temperature)
            {
                KrakenDevice.AllTemperatureSensors.Add(n);
            }
        }

        private float prev_cooling = 0;
        private DateTime? cool_req_time = null;
        private bool cooling_latched = false;

        private void bgTimer_Tick(object sender, EventArgs e)
        {
            _bgTimer.Interval = 2500;
            try
            {
                int fan = 0, pump = 0;

                float x = GetCoolingRequested();
                if ((prev_cooling <= 0 || cool_req_time.HasValue == false) && x > 0)
                {
                    cool_req_time = DateTime.Now;
                }
                if (x > 0 && cool_req_time.HasValue)
                {
                    TimeSpan ts = DateTime.Now - cool_req_time.Value;
                    if (ts.TotalSeconds >= 5)
                    {
                        cooling_latched = true;
                    }
                    _bgTimer.Interval = 500;
                }
                if (x <= 0)
                {
                    cooling_latched = false;
                    cool_req_time = null;
                }
                if (x > 0 && cooling_latched)
                {
                    fan = Convert.ToInt32(Math.Round(x * 255.0));
                    fan = fan < 16 ? 0 : (fan < 32 ? 32 : fan);
                    pump = Convert.ToInt32(Math.Round(x * 128.0));
                    pump = pump > 0 ? (pump + 128) : 0;
                }
                prev_cooling = x;
                _menuItemCoolingAuto.Text = string.Format("Cooling - Auto ({0}%)", Convert.ToInt32(Math.Round(x)));

                UsbConnect();
                if (IsDeviceConnected)
                {
                    _menuItemCoolingDevice.Text = "Kraken Device - Open";

                    switch (_coolingMode)
                    {
                        case CoolingMode.Off:
                            SetPwm(0, 0);
                            break;
                        case CoolingMode.Full:
                            SetPwm(255, 255);
                            break;
                        case CoolingMode.Low:
                            SetPwm(32, 128);
                            break;
                        case CoolingMode.Shutdown:
                            SetPwmShutdown();
                            break;
                        case CoolingMode.Auto:
                            SetPwm(fan, pump);
                            break;
                    }
                }
                else
                {
                    _menuItemCoolingDevice.Text = "Kraken Device - Disconnected";
                }
            }
            catch (Exception ex)
            {
                Program.SysTray.MainIcon.ShowBalloonTip(0, "Kraken Cooling Error", "Error: " + ex.ToString(), ToolTipIcon.Error);
            }
        }

        private static float MapValues(float x, float in_min, float in_max, float out_min, float out_max)
        {
            float y = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
            y = Math.Max(out_min, y);
            y = Math.Min(out_max, y);
            return y;
        }

        private float GetCoolingRequested()
        {
            float fan_ctrl = 0;
            if (allFanControls.Count > 0)
            {
                int cnt = 0;
                float sum = 0;
                foreach (SensorNode sensor in allFanControls)
                {
                    float? x = sensor.Sensor.Value;
                    if (x.HasValue)
                    {
                        if (x.Value > 0)
                        {
                            sum += x.Value;
                            cnt += 1;
                        }
                    }
                }
                if (cnt > 0)
                {
                    fan_ctrl = sum / cnt;
                    fan_ctrl = MapValues(fan_ctrl, _fanMin, _fanMax, 0, 100);
                }
            }

            float pwr_ctrl = 0;
            float pwr_sum = 0;
            foreach (SensorNode i in allPowerNodes)
            {
                if (i.Sensor.Value.HasValue)
                {
                    pwr_sum += i.Sensor.Value.Value;
                }
            }
            if (pwr_sum > 0)
            {
                pwr_ctrl = MapValues(pwr_sum, _pwrMin, _pwrMax, 0, 100);
            }

            float temp_ctrl = 0;
            float hottest = 0;
            foreach (SensorNode i in allTemperatureSensors)
            {
                if (i.Sensor.Value.HasValue)
                {
                    float s = i.Sensor.Value.Value;
                    if (s < 110)
                    {
                        if (s > hottest)
                        {
                            hottest = s;
                        }
                    }
                }
            }
            if (hottest > 0)
            {
                temp_ctrl = MapValues(hottest, _tempMin, _tempMax, 0, 100);
            }

            return Math.Max(fan_ctrl, Math.Max(pwr_ctrl, temp_ctrl));
        }

        private void UsbConnect()
        {
            if (_usbdev == null)
            {
                _usbdev = HidDevices.Enumerate(VendorId, ProductId, 0xFFAB, 0x0200).FirstOrDefault();
            }
            if (_usbdev != null)
            {
                if (_usbdev.IsOpen == false)
                {
                    _usbdev.OpenDevice();
                    if (_usbdev.IsOpen)
                    {
                        _usbdev.Inserted += usbdev_Inserted;
                        _usbdev.Removed += usbdev_Removed;
                        _usbdev.MonitorDeviceEvents = true;
                    }
                }
                if (_usbdev.IsOpen == false)
                {
                    _usbdev.Dispose();
                    _usbdev = null;
                    // this whole function will be reattempted
                }
            }
        }

        private void usbdev_Removed()
        {
            _usbdev.Dispose();
            _usbdev = null; // triggers the search again
        }

        private void usbdev_Inserted()
        {

        }
        private static void usbdev_WriteHandler(bool success)
        {
            // do nothing
        }

        private void SetPwm(int pwm_fan, int pwm_pump)
        {
            HidReport report = _usbdev.CreateReport();

            report.Data[1] = 0xAA;
            report.Data[2] = 0x55;
            report.Data[3] = Convert.ToByte(DeviceCommandByte.Both);
            report.Data[4] = Convert.ToByte(pwm_fan > 255 ? 255 : (pwm_fan < 0 ? 0 : pwm_fan));
            report.Data[5] = Convert.ToByte(pwm_pump > 255 ? 255 : (pwm_pump < 0 ? 0 : pwm_pump));

            _usbdev.WriteReport(report, usbdev_WriteHandler, 0);
        }

        private void SetPwmFan(int pwm_fan)
        {
            HidReport report = _usbdev.CreateReport();

            report.Data[1] = 0xAA;
            report.Data[2] = 0x55;
            report.Data[3] = Convert.ToByte(DeviceCommandByte.Fan);
            report.Data[4] = Convert.ToByte(pwm_fan > 255 ? 255 : (pwm_fan < 0 ? 0 : pwm_fan));

            _usbdev.WriteReport(report, usbdev_WriteHandler, 0);
        }

        private void SetPwmPump(int pwm_pump)
        {
            HidReport report = _usbdev.CreateReport();

            report.Data[1] = 0xAA;
            report.Data[2] = 0x55;
            report.Data[3] = Convert.ToByte(DeviceCommandByte.Pump);
            report.Data[4] = Convert.ToByte(pwm_pump > 255 ? 255 : (pwm_pump < 0 ? 0 : pwm_pump));

            _usbdev.WriteReport(report, usbdev_WriteHandler, 0);
        }

        private void SetPwmShutdown()
        {
            HidReport report = _usbdev.CreateReport();

            report.Data[1] = 0xAA;
            report.Data[2] = 0x55;
            report.Data[3] = Convert.ToByte(DeviceCommandByte.Shutdown);

            _usbdev.WriteReport(report, usbdev_WriteHandler, 0);
        }
    }
}
