# ‘Geek szitman supercamera’ viewer

### Description

This repository is a proof-of-concept to use the ‘Geek szitman supercamera’ camera-based products.
It features a small viewer app.

### Technical information

‘Geek szitman supercamera’ (2ce3:3828 or 0329:2022) is a camera chip (endoscope, glasses, ...) using the com.useeplus.protocol
(officially only working on iOS/Android devices with specific apps, such as ‘Usee Plus’).
Only firmware version 1.00 has been tested. USB descriptors can be found in file the `descriptors` folder.

**Contrary to the advertised specification**, the camera resolution is 640×480.

License is CC0: integrate this code as you like in other camera viewer software / apps.

If you have another hardware also using the ‘com.useeplus.protocol’ protocol, it may or may not work.
If it does, please open an issue so it can be added to the list of working devices.
If it does not work, please open an issue (see [Troubleshooting](#troubleshooting)).

### Build

Install dependencies (packages given assume a Debian-based system):

```bash
apt install build-essential libusb-1.0-0-dev libopencv-dev
```

Build the tool:

```bash
make
```

### Usage

Run the tool:

```bash
./out
```

It will display the camera feed in a GUI window.

- short press on the endoscope button will save the current frame in the `pics` folder
- long press on the endoscope button will switch between the two cameras
- press <kbd>q</kbd> or <kbd>Esc</kbd> in the GUI window to quit

### udev rules

To allow running the tool without superuser privileges, add a udev rule:

```bash
echo 'SUBSYSTEMS=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="2ce3", ATTRS{idProduct}=="3828", MODE="0666"' | sudo tee /etc/udev/rules.d/99-supercamera.rules
echo 'SUBSYSTEMS=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="0329", ATTRS{idProduct}=="2022", MODE="0666"' | sudo tee -a /etc/udev/rules.d/99-supercamera.rules
```

### Troubleshooting

Feel free to open an issue.
Please recompile with `VERBOSE = 3` and include full logs.

If your hardware is different, do include its USB descriptors:

```bash
lsusb -vd $(lsusb | grep Geek | awk '{print $6}')
```

**Known issues:**

- `fatal: usb device not found`: check your device is properly plugged in. Check you have added udev rules properly. Try to run the program with root privileges: `sudo ./out`.

### License

This project is distributed under Creative Commons Zero v1.0 Universal (CC0-1.0).
