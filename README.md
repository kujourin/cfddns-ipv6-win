# Cloudflare DDNS IPv6 for Windows

A lightweight, background-running C++ client for updating Cloudflare DNS records with your local IPv6 address. It continuously monitors your IPv6 address and pushes updates to Cloudflare's API whenever a change is detected.

[Read this in Chinese (中文)](README_zh.md)

## Features

- **Native Windows API**: Built using pure Windows API (`WinHTTP` and `Iphlpapi`). No external dependencies (like libcurl or OpenSSL) are required.
- **Silent Operation**: Runs entirely in the background without spawning a console window.
- **Single Instance**: Uses a Mutex to prevent multiple instances from running concurrently.
- **Simple Configuration**: Reads settings dynamically from an easy-to-use `config.ini` file.

## Setup & Usage

1. Download the latest compiled executable from the [Releases](https://github.com/) page (or compile it yourself).
2. Place the executable in any folder of your choice.
3. Run the executable once. It will detect the missing configuration, automatically generate a template `config.ini` in the same directory, and display a prompt asking you to configure it.
4. Open the `config.ini` file and fill in your Cloudflare details:

```ini
API_TOKEN=your_cloudflare_api_token
DNS_NAME=sub.yourdomain.com
ZONE_NAME=yourdomain.com
SLEEP_INTERVAL_SEC=3600
```

- `API_TOKEN`: Your Cloudflare API Token (needs permissions to Edit Zone DNS).
- `DNS_NAME`: The full sub-domain you want to update (e.g., `ddns.example.com`).
- `ZONE_NAME`: The root domain name (e.g., `example.com`).
- `SLEEP_INTERVAL_SEC`: How often the program checks for IP changes, in seconds (default is 3600 seconds = 1 hour).

5. Save the `config.ini` and run the executable again. It will run silently in the background.

## Running on Startup

To make the application start automatically when you log in:
1. Press `Win + R` to open the Run dialog.
2. Type `shell:startup` and press Enter to open the Startup folder.
3. Create a shortcut to the executable (`cfddns-ipv6-*.exe`) and place it in this folder.

## Stopping the Application

Since the application runs silently in the background without a window, you can stop it using Task Manager:
1. Open Task Manager (`Ctrl + Shift + Esc`).
2. Go to the "Details" or "Processes" tab.
3. Find the executable (e.g., `cfddns-ipv6-x64.exe`), right-click it, and select "End task".

## Build Instructions (MSVC)

To build the project yourself using Microsoft Visual C++ (MSVC):

1. Open an "x64 Native Tools Command Prompt for VS".
2. Navigate to the source code folder.
3. Run the following compile command:

```cmd
cl.exe ddns_ipv6.cpp /EHsc /O2 /link /out:cfddns-ipv6.exe
```

## License
MIT License
