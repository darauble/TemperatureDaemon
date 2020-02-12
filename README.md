# USART Temperature Daemon

This daemon is designated to run on any Linux compatible hardware with USB port(s), measure temperature using Dallas
One Wire thermometers family connected via USB-USART adapter. Adapter might be any of FT232, CH304, PL2303, etc.

It could run fine on Raspberry Pi and it is much more reliable than One Wire GPIO kernel module, as latter cannot keep
precise timings and also can only use one GPIO. Whereas this daemon can run on any count of adapters. Having several
One Wire lines is desirable in star type topology, as overall length of wires matters, better to keep them shorter.

# Compilation

This daemon depents on pthread, Jansson and Eclipse Paho MQTT libraries.

## Jansson

You can go with Jansson library supplied by your Linux distribution. However, this daemon is developed with particular
version.

Use included `get_jansson.sh` script to download particular version of Jansson library. Then enter the `jansson`
directory, compile and install:

```
cd jansson
./configure
make
sudo make install
```

## Eclipse Paho MQTT

Paho MQTT is checked out as this daemon's GIT submodule. Enter `paho.mqtt.c` directory, compile and install:

```
cd paho.mqtt.c
make
sudo make install
```

## The Daemon

Copy `Config.in` to `Config` and edit for your needs: choose DEBUG or RELEASE, choose if daemon should wait for MQTT
messages delivery. MQTT messages delivery is better to be left running asynchronously, but in some cases it is desirable
to wait for delivery: network is slow and it is not desirable to read thermometers again until messages are delivered to
save possible congestion. However, in most cases _there is no need to wait for MQTT delivery_.

Type `make` and daemon should be compiled. There is no `install` target, it is supposed to be run in userspace.

# Using the Daemon

The daemon can be run with several USB adapters, connected to target hardware. It can output readings to TSV and/or JSON
files and also publish readings to MQTT server.

Daemon runs in foreground mode by default, as this is usually desired mode of `systemd`, but it can also be run in
background mode.

Let's say you have two USB devices and several Dallas temperature sensors connected to each. Let's start daemon and
let it output its resulsts to `/tmp/temperature.json` file:

`./temp_daemon -d /dev/ttyUSB0 -d /dev/ttyUSB1 --json=/tmp/temperature.json`

Add `--verbose` switch to see more output.

Daemon will run, read temperatures every minute and update output file. By default it will also search for sensors on
every adapter every 5 minutes. Both search period and reading period are configurable, see `--help` for more details.

## Stability

USB dongles may be reconnected while daemon runs, it will simply report failed USB device and for every reading cycle it
will retry attaching to USB adapter. MQTT reconnections are also handled automatically should the network or server
fail.

## Optimizations

Because of the slow nature of the One Wire bus, daemon starts separate thread for each USB adapter and each One Wire
line is read simultaneously saving a lot of time. Also, by default, daemon reads only first two bytes of the Dallas
sensors, as that's enough to convert temperature with expected 12-bit resolution. However, this works well only for
**DS18B20**. If you have **DS18S20**, full scratchpad reading _is required_ for successful converstion (`-F` switch).

**DS18S20** support also needs to be enabled in `dallas` library (see `dallas.h` in `DallasOneWire` submodule).
