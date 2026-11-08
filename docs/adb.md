# Android Debug Bridge (ADB)

CoreDeck's ADB workspace talks to phones, tablets, and emulators through Google's
official `adb` in the SDK's `platform-tools`. It does not embed its own device
protocol. Every action is an `adb` process: a long-running tracker, a long-running
logcat, or a one-shot command.

Open it from the menu: **Android Debug Bridge (ADB)**. If `adb` is missing, the
workspace shows a notice and can start the platform-tools install. The binary
CoreDeck uses is the one recorded for the current SDK, the same path the Health
Check reports.

## What you can do

- Watch devices come and go, including USB phones, Wi-Fi phones, and emulators.
- Stream logcat, filter by priority and buffer, and clear the device's log buffers.
- Run one shell command at a time and keep a per-device history of the ones that succeed.
- Install an APK, uninstall a package, and push or pull files.
- Forward and reverse ports, and reconnect a saved Wi-Fi address later.
- Switch a USB device to Wi-Fi. The port field starts at 5555.

Each device keeps its own tab, inputs, output, and in-flight command. Switching the
dropdown does not throw that away. Leaving the workspace stops live logcat. The
captured text stays until you quit CoreDeck.

## The device list

While the workspace is open, CoreDeck runs:

```bash
adb track-devices -l
```

That stream is the live list. About every two seconds, and after a connect or
disconnect, it also runs `adb devices -l` and publishes that snapshot. The list
changes only when the devices actually change. A green dot means the device can
take commands (`device` or `recovery`). Anything else is red: offline, unauthorized,
bootloader, sideload, no permissions, or unknown.

The label is the model name, with underscores shown as spaces. An emulator uses
its AVD name when CoreDeck knows it. If neither is available, the label is the
serial. The dropdown also shows the serial.

```text
RMX3085 (192.168.68.102:5555)
Pixel_7_API_34 (emulator-5554)
```

Pick the device before you run a command. Logcat, shell, packages, port routes,
and **Connect over Wi-Fi** all use that selection. A saved Wi-Fi **Connect** or
**Disconnect** uses the address on that row, so it still works when nothing is
selected in the dropdown.

One device runs one of those one-shot commands at a time. A second click on the
same device waits, and the tooltip says to wait. Logcat is separate: it can keep
streaming while a shell command or an install runs. Two different devices can run
commands at the same time.

Commands that need a usable device stay disabled until the selection is online
(`device`) or in recovery. Unauthorized devices get a status-bar hint to unlock
the phone and accept the USB debugging key.

## Logcat

**Start** runs a streaming logcat and keeps the view pinned to new lines when
auto-scroll is on in Preferences. **Stop** ends that stream only. The lines already
captured stay in the viewer.

The priority menu is the logcat filter `*:V` through `*:F`. The buffer menu is
either the default set or one buffer:

| Buffer menu | `adb` arguments              |
| ----------- | ---------------------------- |
| Default     | `-b main -b system -b crash` |
| Main        | `-b main`                    |
| System      | `-b system`                  |
| Crash       | `-b crash`                   |

A default Verbose stream of the last 200 lines looks like this:

```bash
adb -s 192.168.68.102:5555 logcat -v threadtime \
  -b main -b system -b crash -T 200 *:V
```

The viewer keeps up to 20,000 lines. Older lines are dropped, and a warning counts
them. Search and pause behave like the other log viewers.

**Clear Device...** asks first, then wipes Android's buffers on that device:

```bash
adb -s 192.168.68.102:5555 logcat -c
```

That does not erase the lines CoreDeck already captured.

## Shell

The shell tab runs one command through `adb shell` and waits for it to finish.
It does not open an interactive terminal, so commands that wait for input will
sit there until you press **Cancel**. **Cancel** interrupts that `adb` process.

```bash
adb -s 192.168.68.102:5555 shell getprop ro.build.version.release
```

Press Enter or **Run**. Stdout and stderr are shown together. A command is added
to **Recent commands** only when it exits 0. The newest successful command is
first, duplicates collapse, and the list stops at 64. Choosing one copies it
back into the field. Failed and cancelled commands are not saved.

The history is stored per serial in `settings.json` under `AdbDeviceProfiles`.
Reopening the ADB workspace reads it again, so an edit you made in that file
shows up without restarting CoreDeck.

```json
"AdbDeviceProfiles": [
  {
    "Serial": "192.168.68.102:5555",
    "ShellCommands": ["getprop ro.build.version.release", "getprop"]
  }
]
```

## Packages and files

Install runs `adb install` against the selected device. **Replace existing** is
on by default.

```bash
adb -s 192.168.68.102:5555 install -r -g app-release.apk
```

| Option                    | Flag |
| ------------------------- | ---- |
| Replace existing          | `-r` |
| Allow downgrade           | `-d` |
| Grant runtime permissions | `-g` |

Uninstall asks for confirmation. **Browse...** lists the packages installed on the
device. It opens on all packages (`pm list packages`). **Third-party only** adds
`-3`, and **System apps** adds `-s`. Choosing one of those reloads the list. The
search field filters whatever was loaded. Choosing a package fills the package
name and closes the list. The button stays off until the selected device can
take commands. **Keep app data** keeps the data directory:

```bash
adb -s 192.168.68.102:5555 uninstall com.example.app
adb -s 192.168.68.102:5555 shell cmd package uninstall -k com.example.app
```

Push sends a local file to a device path. Pull copies a device file into a local
folder you pick.

```bash
adb -s 192.168.68.102:5555 push ./notes.txt /sdcard/Download/notes.txt
adb -s 192.168.68.102:5555 pull /sdcard/Download/notes.txt ~/Downloads
```

The command output stays on this tab until you replace it. The status bar is the
short result: completed, failed, or cancelled.

## Port routes

**Connections** lists `adb forward` and `adb reverse` for the selected device.
CoreDeck does not store these routes. `adb` does.

- A forward stays in the host `adb` server until you remove it, run
  `adb kill-server`, or reboot the computer.
- A reverse stays in `adbd` on the device until you remove it, `adbd` restarts,
  or the device reboots. `adb tcpip 5555` restarts `adbd`, so it drops reverses.

The list reloads when you open the workspace, when you change the device, when
the device list changes, and when you press refresh. Adding or removing a route
reloads it too. While the reload is running, the list keeps the rows it already
has. An empty list stays empty and says **No Port Routes**. The "refreshing"
text is only in the status bar.

**Local** is your computer. **Remote** is the device. The arrow points from the
computer side to the device side for a forward, and the other way for a reverse.

This forward accepts connections on your port 8080 and delivers them to the
device's port 8080:

```bash
adb -s 192.168.68.102:5555 forward tcp:8080 tcp:8080
```

This reverse accepts connections on the device's port 9090 and delivers them to
your port 8080:

```bash
adb -s 192.168.68.102:5555 reverse tcp:9090 tcp:8080
```

In the row, that reverse still reads **Local** `tcp:8080` and **Remote**
`tcp:9090`. Remove uses the device socket, because `adb reverse --remove` wants
that name:

```bash
adb -s 192.168.68.102:5555 reverse --remove tcp:9090
adb -s 192.168.68.102:5555 forward --remove tcp:8080
```

The form accepts the usual adb endpoint forms: `tcp:8080`, `localabstract:name`,
`localreserved`, `localfilesystem`, `dev`, `jdwp`, and `vsock`. A `tcp` port may
be 0 through 65535. Ports below 1024 often fail on a reverse with `Permission
denied` from `adbd`. That is the device refusing the bind, not a parsing error.

`adb forward --list` is host-wide even with `-s`, so CoreDeck drops rows whose
serial is some other device. `adb reverse --list` is already scoped to `-s`.
Older devices print the reverse owner as `host`. Newer ones print `host-12`.
Both are treated as "this device," not as a serial to filter out.

## Wireless devices

**Manage Wireless Devices**, next to the device dropdown, opens a window that is
not tied to the current selection. You can add an address while no device is
selected, and you can reconnect after the last phone has disconnected.

The window has three parts: **Connect over Wi-Fi**, the saved-device list, and a
status bar that stays visible. Closing the window clears that status bar.

### Connect over Wi-Fi

This is enabled only for a physical USB device that is online and authorized.
It stays off for an emulator, a device that is already a `host:port` address,
or a phone that is offline or unauthorized. The button names the USB device,
for example **Connect RMX3085 over Wi-Fi**. A **Port** field sits beside it and
starts at `5555`. The question mark explains the steps without closing this window.

CoreDeck then does the following against that USB serial.

1. Read a private IPv4 address on a `wlan` interface. It tries, in order:

   ```bash
   adb -s SERIAL shell ip -f inet addr
   adb -s SERIAL shell ip addr
   adb -s SERIAL shell getprop dhcp.wlan0.ipaddress
   ```

   A failed probe is skipped. The address has to be `10.x.x.x`, `172.16.x.x`
   through `172.31.x.x`, or `192.168.x.x`. If the output is an interface listing,
   only an address on `wlan` counts. The `getprop` result is a bare address, so
   that address is used when the earlier commands did not print a listing.
   Anything else stops here and leaves the USB connection alone.

2. Restart `adbd` so it listens on the TCP port from the field. The field starts
   at 5555, and any port from 1 to 65535 is accepted.

   ```bash
   adb -s SERIAL tcpip 5555
   ```

   USB debugging drops while `adbd` restarts. The cable can stay plugged in.
   If this command fails, CoreDeck does not try to connect.

3. Wait 800 ms, then connect to that same port. This command has no `-s`. It
   talks to the address.

   ```bash
   adb connect 192.168.68.102:5555
   ```

   Success is a report containing `connected to` or `already connected`.
   Anything else waits 600 ms and tries again, up to five times.

4. Select `192.168.68.102:5555` in the dropdown and save it. The saved address
   uses the port from the field. No further command runs.

### Saved addresses

Type `192.168.68.102:5555` and press **Connect**, or press **Connect** on a saved
row. Both run:

```bash
adb connect 192.168.68.102:5555
```

**Disconnect** is shown only while that address is online:

```bash
adb disconnect 192.168.68.102:5555
```

The remove button forgets the address. CoreDeck remembers at most 16. The list
is global, not per device, in `AdbWirelessDevices`:

```json
"AdbWirelessDevices": [
  "192.168.68.102:5555",
  "192.168.68.105:5555"
]
```

An address has to look like `host:port`, with a port from 1 to 65535 and no
spaces. `192.168.68.102` alone is rejected.

A Wi-Fi device that is already online is added to this list when you open the
workspace, unless you removed it during this visit. Reopening the workspace
reads `settings.json` again. A phone you connected from a terminal shows up
after you leave ADB and open it again. You do not have to restart CoreDeck.

On macOS and Linux the file is `~/.config/coredeck/settings.json`.

## Where a command runs

One-shot work is `adb -s SERIAL ...`, except `adb connect` and `adb disconnect`,
which do not take `-s`. Output is capped at 16 MB per command. The status bar
shows the first line of the report, or a short sentence such as **Device
connected.** when the report is empty.

| Action               | Command                                                  |
| -------------------- | -------------------------------------------------------- |
| Shell                | `adb -s SERIAL shell <command>`                          |
| Install              | `adb -s SERIAL install [-r] [-d] [-g] <apk>`             |
| Uninstall            | `adb -s SERIAL uninstall <package>`                      |
| Uninstall, keep data | `adb -s SERIAL shell cmd package uninstall -k <package>` |
| List packages        | `adb -s SERIAL shell pm list packages [-3\|-s]`          |
| Push                 | `adb -s SERIAL push <local> <remote>`                    |
| Pull                 | `adb -s SERIAL pull <remote> <local>`                    |
| Forward              | `adb -s SERIAL forward <local> <remote>`                 |
| Reverse              | `adb -s SERIAL reverse <remote> <local>`                 |
| List forwards        | `adb -s SERIAL forward --list`                           |
| List reverses        | `adb -s SERIAL reverse --list`                           |
| Connect              | `adb connect <host:port>`                                |
| Disconnect           | `adb disconnect <host:port>`                             |
| Wi-Fi switch         | the probe, `tcpip <port>`, and `connect` sequence above  |
| Clear log            | `adb -s SERIAL logcat -c`                                |

## What this workspace does not do

- It does not open an interactive shell. There is no `adb shell` with a prompt.
- It does not pair a device with an Android 11+ pairing code. The Wi-Fi switch
  needs a USB debugging session first.
- It does not install or uninstall split APKs, app bundles, or more than one
  package at a time.
- It does not edit `adb` server settings, kill the server, or backup the device.
- It does not keep port routes in `settings.json`. Those belong to `adb`.

## Code map

| Piece                                        | Path                           |
| -------------------------------------------- | ------------------------------ |
| One-shot `adb` commands and the Wi-Fi switch | `src/core/adb_client.cpp`      |
| `track-devices` and logcat processes         | `src/core/adb_service.cpp`     |
| Device and forward parsers                   | `src/core/adb_devices.cpp`     |
| Workspace, per-device state, saved profiles  | `src/gui/windows/adb/`         |
| Settings fields                              | `src/app/app_settings_types.h` |
