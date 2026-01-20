[![](https://alexey.work/badge)](https://alexey.work?ref=nvml)

# NVML Tool

A powerful command-line utility for monitoring and controlling NVIDIA GPUs using the NVML library. Built with a focus on simplicity, performance, and reliability.

## Quick Start

### Installation

```bash
# Build from source
git clone https://github.com/xl0/nvml-tool
cd nvml-tool
make
sudo make install

# Or install to custom location
make install PREFIX=/usr/local
```

### Basic Usage

```bash
# Show information for all GPUs
nvml-tool info

# Monitor specific GPU
nvml-tool info -d 0

# Get JSON output for automation
nvml-tool info json

# Monitor power consumption
nvml-tool power

# Set power limit (requires root)
sudo nvml-tool power set 250 -d 0

# Control fan speed (requires root)
sudo nvml-tool fan set 80 -d 0

# Restore automatic fan control
sudo nvml-tool fan restore -d 0

# Dynamic fan control with temperature setpoints (requires root)
sudo nvml-tool fanctl 50:30 70:60 80:90 -d 0

# Quick status overview
nvml-tool status
```

## Detailed Usage

### Commands

#### `info [json]`
Display comprehensive device information including name, UUID, temperature, memory usage, fan speed, and power consumption.

```bash
nvml-tool info                    # All devices, human-readable
nvml-tool info -d 0               # Device 0 only
nvml-tool info json               # JSON output
nvml-tool info -d 0-2 json        # Devices 0-2, JSON format
```

#### `power [set VALUE]`
Monitor or control GPU power consumption and limits.

```bash
nvml-tool power                   # Show current power usage
nvml-tool power -d 0              # Power for device 0
sudo nvml-tool power set 200 -d 0 # Set 200W limit on device 0
```

#### `fan [set VALUE|restore]`
Control GPU fan speeds manually or restore automatic control.

```bash
nvml-tool fan                     # Show current fan speeds
sudo nvml-tool fan set 75 -d 0    # Set 75% fan speed on device 0
sudo nvml-tool fan restore        # Restore automatic control (all devices)
sudo nvml-tool fan restore -d 0   # Restore automatic control (device 0)
```

#### `temp`
Display GPU temperatures in various units.

```bash
nvml-tool temp                    # Celsius (default)
nvml-tool temp --temp-unit F      # Fahrenheit
nvml-tool temp --temp-unit K      # Kelvin
```

#### `status`
Show compact status overview with temperature, fan speed, and power.

```bash
nvml-tool status                  # All devices
nvml-tool status -d 0-1           # Devices 0 and 1
```

#### `fanctl SETPOINTS`
Dynamic fan control using temperature setpoints with linear interpolation. Continuously monitors GPU temperature and adjusts fan speed based on the defined temperature-to-fan-speed mapping.

**Requirements:** Root access, controllable fans

```bash
# Basic usage with temperature:fan% setpoints
sudo nvml-tool fanctl 50:30 70:60 80:90 -d 0

# Multiple setpoints for fine control
sudo nvml-tool fanctl 40:20 50:30 60:45 70:60 80:80 90:100

# Control all devices
sudo nvml-tool fanctl 50:30 70:60 80:90
```

**How it works:**
- Takes temperature:fan-speed setpoints (e.g., `70:60` = 70°C → 60% fan speed)
- Uses linear interpolation between setpoints for smooth transitions
- Updates fan speeds every 2 seconds based on current GPU temperature
- Shows live status updates when run in terminal
- Automatically restores automatic fan control on exit (Ctrl-C)

**Safety considerations:**
- Monitor temperatures carefully when using manual fan control
- Insufficient cooling can damage your GPU
- Use `Ctrl-C` to exit and restore automatic control
- Fan control is reset to automatic if the tool exits unexpectedly

#### `list`
List all available GPUs with their IDs, UUIDs, and names.

```bash
nvml-tool list                    # Simple device listing
```

### Device Selection Options

#### By Index
```bash
-d 0                              # Single device
-d 0-2                            # Range (devices 0, 1, 2)
-d 0,2,4                          # List (devices 0, 2, 4)
```

#### By UUID
```bash
-u GPU-abc123                     # Partial UUID match
-u GPU-abc123-def456-789          # Full UUID
```

### Output Options

#### Temperature Units
```bash
--temp-unit C                     # Celsius (default)
--temp-unit F                     # Fahrenheit
--temp-unit K                     # Kelvin
```

#### JSON Output
Perfect for automation and scripting:

```bash
nvml-tool info json | jq '.[0].power_usage_watts'
nvml-tool status | awk -F: '{print $1 ": " $2}' | column -t
```

### Build Requirements

- GCC or compatible C compiler
- NVML library (from NVIDIA drivers, CUDA toolkit, or system packages)
- pkg-config


## Troubleshooting

### NVML Detection Issues

If build fails with NVML detection errors:

```bash
# Check if NVML is installed
pkg-config --list-all | grep nvidia-ml

# Pass CFLAGS and LIBS manually if pkg-config is wrong
make NVML_CFLAGS="-I/usr/local/cuda/include" NVML_LIBS="-L/usr/local/cuda/lib64 -lnvidia-ml"

```

### Permission Issues

Most monitoring commands work as regular user, but control commands require root:

```bash
# Monitoring (no root required)
nvml-tool info
nvml-tool power
nvml-tool temp

# Control (requires root)
sudo nvml-tool power set 200 -d 0
sudo nvml-tool fan set 75 -d 0
sudo nvml-tool fanctl 50:30 70:60 80:90 -d 0
```

### Fan Control Issues

**Error: "Device has no controllable fans"**
- Some GPUs don't support manual fan control
- Older NVIDIA drivers may not support fan control
- Check if your GPU model supports fan control

**Fanctl not working as expected:**
- Ensure you're running as root (`sudo`)
- Check that NVML version supports fan control policies
- Monitor GPU temperatures to verify setpoints are reasonable

## Output Examples

### Device Information
```
=== Device 0: NVIDIA RTX 4090 ===
UUID:        GPU-12345678-abcd-ef12-3456-789abcdef012
Temperature: 45.2°C
Memory:      1024 MB / 24576 MB (4.2%)
Fan Speed:   35%
Power:       125.5W / 450.0W (27.9%)
```

### JSON Output
```json
[
  {
    "device_id": 0,
    "name": "NVIDIA RTX 4090",
    "uuid": "GPU-12345678-abcd-ef12-3456-789abcdef012",
    "temperature": 45.2,
    "temperature_unit": "C",
    "memory_total_mb": 24576,
    "memory_used_mb": 1024,
    "memory_free_mb": 23552,
    "fan_speed_percent": 35,
    "power_usage_watts": 125.50,
    "power_limit_watts": 450.00
  }
]
```

### Status Overview
```
0:45.2C,35%,125.5W
1:42.1C,40%,98.2W
2:50.3C,45%,156.7W
```

### Dynamic Fan Control (reverts to automatic, except for SIGSTOP)
```
Starting dynamic fan control for 1 device(s) (Ctrl-C to exit)
Setpoints: 50:30% 70:60% 80:90%

0:52.3C -> 42%
0:53.1C -> 44%
0:54.2C -> 47%
^C
Restoring automatic fan control...
```