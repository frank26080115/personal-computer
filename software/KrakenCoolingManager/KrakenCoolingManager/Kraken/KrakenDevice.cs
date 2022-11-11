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

        private SystemTray _sysTray;
        private System.Windows.Forms.Timer _bgTimer;

        private ToolStripMenuItem _menuItemCoolingDevice;
        private ToolStripMenuItem _menuItemCoolingOff;
        private ToolStripMenuItem _menuItemCoolingAuto;
        private ToolStripMenuItem _menuItemCoolingLow;
        private ToolStripMenuItem _menuItemCoolingFull;
        private ToolStripMenuItem _menuItemCoolingShutdown;

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

        public KrakenDevice(SystemTray sysTray)
        {
            _sysTray = sysTray;
            _bgTimer = new System.Windows.Forms.Timer();
            _bgTimer.Interval = 5000;
            _bgTimer.Tick += bgTimer_Tick;

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
                _sysTray.MainIcon.ContextMenuStrip.Items.Insert(i, moreItems[i]);
            }

            _bgTimer.Enabled = true;
        }

        private void bgTimer_Tick(object sender, EventArgs e)
        {
            _bgTimer.Interval = 5000;
            try
            {

                float fanCtrl = GetAverageFanControl();
                fanCtrl *= 1.5f;
                fanCtrl -= 50.0f * 1.5f;
                _menuItemCoolingAuto.Text = string.Format("Cooling - Auto ({0}%)", Convert.ToInt32(Math.Round(fanCtrl)));

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
                            int fan = Convert.ToInt32(Math.Round(fanCtrl * 255.0));
                            fan = fan < 16 ? 0 : (fan < 32 ? 32 : fan);
                            int pump = Convert.ToInt32(Math.Round(fanCtrl * 128.0));
                            pump = pump > 0 ? (pump + 128) : 0;
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
                _sysTray.MainIcon.ShowBalloonTip(0, "Kraken Cooling Error", "Error: " + ex.ToString(), ToolTipIcon.Error);
            }
        }

        private float GetAverageFanControl()
        {
            if (allFanControls.Count == 0)
            {
                return 0;
            }
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
            return sum / Convert.ToSingle(cnt);
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
