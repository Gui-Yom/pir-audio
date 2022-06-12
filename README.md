# PIR 9 - Audio

## Building

The project is built with [Platformio](https://platformio.org). Supported hardware : Teensy 4.1

```shell
# Clone the repo
git clone https://github.com/Gui-Yom/pir-audio
# Build the project
pio run
# Upload program to the teensy
pio run -t upload
```

### Build options

The fields and defines at the start of `main.cpp` are options which may be useful to tinker with (e.g. DHCP or manual ip
configuration).

## Running

After uploading to the teensy, the program will wait for a serial connection (if the `WAIT_FOR_SERIAL` define is set).
Start a jacktrip server with `jacktrip -S -q 2 -p 2`, and add another client
with `jacktrip -C 127.0.0.1 -B 4465 -q 2 -n 1`. Then you can open a serial connection to the device and the program will
resume (`pio device monitor`).

## Notes

### Jacktrip protocol (Hub mode)

- TCP connection to exchange UDP ports between client and server
- Client starts to send UDP packets, the server uses the header to initialize jack parameters
- When at least a second client is connected, the server starts broadcasting the mixed audio to everyone.

### TODO

- Transmit audio
- Filter microphone audio
- Autoconfiguration with OpenSoundControl
- Restart strategy after receiving an exit packet (currently need a program reset)
