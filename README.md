# polinaserial

Serial port monitor program for Mac OS X and derivative platforms

**Still very experimental, use it on your own risk and etc. Let me know if there are any issues!**

![](repo/demo.jpg)

## Features

* Automatic obfuscated iBoot logs deobfuscation, when possible
    * All files available in the iOS 9 iBoot source code leak & 500+ recovered ones, including newer files that were never a part of the leak
		* The ones I'm less confident about are shipped in a separate file (`iboot_aux_hmacs.txt`) that you can pass via `POLINASERIAL_IBOOT_HMACS` environment variable

* Input/output filtering
    * Automatically add carriage return (`\r`) to line breaks (`\n`)
        * Some firmwares do not do that on their own (for instance, Apple EFI diags)
    * Replace delete keys (`0x7F`) with backspace (`0x08`) in user input
        * Good for older diags where `0x7F` gets misinterpreted as a line break

* Lolcat support - paint your logs in rainbow colors for cool screenshots and videos

* Interactive menu that lists available serial ports currently connected to the system
    * The list is updated in real time as devices are connected/disconnected

* Logging into a separate file for every session
    * Logs are collected to `~/Library/Logs/polinaserial/<DEVICE>/<SESSION>`
    * Can be disabled via command line option

* Reconnecting automatically in case of device disconnect and rediscovery

* Optionally delaying user input
    * Good for pasting a lot of text into XNU serial console that might be partially lost otherwise

* Baudrate presets for some common Apple targets

## Changelog

<details>

### polinaserial-1.1
* Restructured the code to allow building Polina as a static library to be used in 3rd-party projects
* Minor improvements and bug fixes here and there
	* Handle SIGTERM signal so the program can close gracefully upon OS reboot and etc.
	* Lolcat internal state is now refreshed more often to make sure output is always colorized
	* Removed aux iBoot HMACs that duplicate the hardcoded database
	* Use less allocations to store aux iBoot HMACs at runtime (good for program start-up & shutdown performance)

### polinaserial-1.0.1
* Fixed various bugs in argument parsing

### polinaserial-1
* Initial release

</details>

## Usage (crash course)

`polinaserial` run without any args provided tries to keep things as raw as possible. However, this is usually **NOT** the desired configuration. For iOS things use `-nk` options to keep input/output sane:

```
➜  ~ polinaserial -nk
```

* Add `-i` to deobfuscate RELEASE iBoot logs
	* Pass `POLINASERIAL_IBOOT_HMACS=/path/to/iboot_aux_hmacs.txt` as environment variable to load more iBoot HMAC mappings, or export it via shell configuration scripts

* Add `-l` to paint output into rainbow colors (lolcat)

Serial configuration defaults to **115200** baud rate, **8 data bits**, **1 stop bit** and **no parity** - default config for iOS stuff (iBoot, XNU & etc.)

## Usage

```
➜  ~ polinaserial -h
polinaserial-1
made by john (@nyan_satan)

usage: polinaserial DRIVER <options>

serial:
	-d <device>	path to device (default - shows menu)
	-b <baudrate>	baudrate to use (default - ios/115200)

	available baudrate presets:
		ios         - iOS bootloader/kernel/diags (115200)
		airpods     - AirPods 1/2 case (230400)
		airpodspro  - AirPods Pro case (921600)
		airpower    - AirPower (1250000)
		kanzi       - Kanzi debug (1500000)

	the rest will default to 8N1 (no flow control),
	as the author of this tool cannot currently test other configurations

available filter options:
	-n	add \r to lines without it, good for diags/probe debug logs/etc.
	-k	replace delete keys (0x7F) with backspace (0x08), good for diags
	-i	try to identify filenames in obfuscated iBoot output

available miscallenous options:
	-r	try to reconnect in case of connection loss
	-u <usecs>	delay period in microseconds after each inputted character,
			default - 0 (no delay), 20000 is good for XNU
	-l	lolcat the output, good for screenshots
	-g	disable logging
	-h	show this help menu

additional iBoot HMAC map can be loaded via POLINASERIAL_IBOOT_HMACS variable,
it must be a path to a text file with the following structure:

		HMAC:FILENAME
		HMAC:FILENAME
		...

check "iboot_aux_hmacs.txt" provided along with the program for reference

default DRIVER is "serial"
logs are collected to ~/Library/Logs/polinaserial/
```

## Building

```
make [STYLES="RELEASE ASAN PROFILING"] [PLATFORMS="macosx iphoneos"]
```

By default only `RELEASE/macosx` is built, other targets needs to be specified manually

Xcode 16 broke linking for armv7 binaries, so armv7 builds are gated behind `WITH_ARMV7=1` variable for a case you'd want to build it with an older SDK

`ASAN` & `PROFILING` build styles only make sense for development & testing
