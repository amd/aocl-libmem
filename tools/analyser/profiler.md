# AOCL-LibMem Profiler Tool

## Description
The AOCL-LibMem Profiler is a powerful tool that provides detailed insights into memory function usage patterns by tracing calls to standard C library memory functions. The tool collects data on function call counts, size distributions, and memory alignment, enabling performance analysis and optimization of memory operations in your applications.

## Features
- Traces memory-related libc functions (memcpy, mempcpy, memcmp, memmove, memset, etc.)
- Supports both distribution tracking (with size histograms) and call counting
- Analyzes memory alignment of source and destination pointers (64-byte boundaries)
- Can attach to running processes or launch new applications
- Multi-thread safe - properly tracks and aggregates data across all threads in target processes
- Provides periodic reporting at configurable intervals
- Filters results by process ID for focused analysis
- Supports tracing specific functions only

## Requirements
- Python 3.6+
- BCC Tools (python3-bpfcc) version 0.8.0 or higher, preferably higher than 0.8.0 for better compatibility with newer kernels
- Root privileges (for BPF operations)
- Minimum kernel version 4.19.0 for eBPF tools
- Kernel compatibility:
  - **Kernel 4.19.0**: Works with BCC 0.8.0
  - **Kernel 4.20.0+**: May require BCC version higher than 0.8.0 (due to inline_asm compatibility issues)
  - **Recommended**: Use latest available BCC version for best kernel compatibility
### Installation

#### Check Existing BCC version
Before installing BCC, check if it's already installed and verify the version:

```bash
# Method 1: Check if BCC via pip
pip show bcc
```

```bash
# Method 2: Check BCC version programmatically
python3 -c "import collections, collections.abc; collections.MutableMapping = collections.abc.MutableMapping; import bcc; print('BCC version:', bcc.__version__)"
```

If you see output like "BCC version: = 0.8.0" or higher, BCC installation is not required and you can proceed to use the profiler tool.

> **⚠️ Critical Kernel Compatibility Warning**:
>
> **BCC 0.8.0 + Kernel 5.4.0+ = inline_asm Compilation Errors**
>
> There is a **known incompatibility** between BCC version 0.8.0 and Linux Kernel 5.4.0 and newer that causes eBPF compilation failures with errors like:
> - `expected '(' after 'asm'`
> - `asm_inline not supported`
> - `Failed to compile eBPF program`
>
> **Compatibility Matrix:**
> - ✅ **Kernel 4.19.0** + BCC 0.8.0: **Confirmed working**
> - ⚠️ **Kernel 4.20.0 - 5.3.x** + BCC 0.8.0: **Untested (may work, but BCC upgrade recommended)**
> - ❌ **Kernel 5.4.0+** + BCC 0.8.0: **Confirmed compilation errors (inline_asm issues)**
> - ✅ **Kernel 5.4.0+** + BCC > 0.8.0: **Confirmed working**
>
> **📢 Recommendation:** If you're using Kernel versions between 4.20.0 - 5.3.x, we recommend upgrading to BCC > 0.8.0 for guaranteed compatibility. Update your BCC version to ensure reliable profiler operation.
>
> **Quick Fix - Upgrade BCC:**
> If you encounter BPF compilation failures with inline_asm errors, upgrade to a newer BCC version:
>
> **For Debian/Ubuntu:**
> ```bash
> # Add backports repository (if not already added)
> echo "deb http://deb.debian.org/debian buster-backports main" | sudo tee /etc/apt/sources.list.d/backports.list
> sudo apt update
>
> # Upgrade to newer BCC version from backports
> sudo apt install -t buster-backports python3-bpfcc
> ```
>
> **For RHEL/CentOS/Fedora:**
> ```bash
> # Enable EPEL repository (if not already enabled)
> sudo dnf install -y epel-release  # or: sudo yum install epel-release
>
> # Update to latest BCC version
> sudo dnf update python3-bcc  # or: sudo yum update python3-bcc
> ```
>
> **For SUSE/openSUSE:**
> ```bash
> # Update to latest BCC version
> sudo zypper update python3-bcc
> ```
>
> **Alternative for any distribution (using pip):**
> ```bash
> # Uninstall system package first
> sudo apt remove python3-bpfcc  # or: sudo dnf/yum/zypper remove python3-bcc
>
> # Install newer version via pip
> pip3 install --user bcc
> ```
>
> This resolves kernel header compatibility issues and improves overall stability.

#### Method 0: Recommended Method (using pip)
First, check which BCC versions are available:
```bash
# Check available BCC versions
pip install bcc==
```

This will show an error with available versions (e.g., 0.1.7, 0.1.8, 0.1.10). Choose the highest available version:
```bash
# Install the latest available version from the error output
pip install bcc==0.1.10
```

#### Method 1: Install from Distribution Packages

**Ubuntu/Debian (apt):**
```bash
sudo apt update
sudo apt install -y python3-bpfcc
```

**RHEL/CentOS/Fedora (dnf/yum):**
```bash
# For RHEL/CentOS 8+ and Fedora
sudo dnf install -y python3-bcc

# For older CentOS/RHEL 7
sudo yum install -y python3-bcc
```

**SUSE/openSUSE (zypper):**
```bash
sudo zypper install -y python3-bcc
```

#### Method 2: Install from Source (All Distributions)
```bash
# Install build dependencies (Ubuntu/Debian)
sudo apt install -y build-essential cmake git python3-dev \
    libbpf-dev libelf-dev zlib1g-dev libfl-dev

# Install build dependencies (RHEL/CentOS/Fedora)
sudo dnf install -y gcc gcc-c++ cmake git python3-devel \
    libbpf-devel elfutils-libelf-devel zlib-devel flex-devel

# Clone and build BCC (minimum required version 0.8.0)
git clone https://github.com/iovisor/bcc.git
cd bcc
git checkout v0.8.0  # or use latest: git checkout master
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
sudo make install
```

#### Verify Installation
```bash
# Check BCC installation

```bash
# BBC installed via pip
pip show bcc
```

```bash
# Install BCC tools
sudo apt-get install -y python3-bpfcc
```

**Note:** The profiler requires BCC version 0.8.0 or higher and will automatically detect your version, applying necessary compatibility fixes. If your version is too old, the profiler will display helpful upgrade instructions.

## Basic Usage

### Trace a Running Process
```bash
# Trace an existing process with PID 1234
sudo ./prof_libmem.py -p 1234

# Trace with more verbose output
sudo ./prof_libmem.py -v -p 1234

# Trace with alignment checking
sudo ./prof_libmem.py -p 1234 -a
```

### Launch and Trace an Application
```bash
# Launch and trace an application
sudo ./prof_libmem.py -e ./my_application arg1 arg2

# Launch and trace with alignment checking
sudo ./prof_libmem.py -a -e ./my_application arg1 arg2
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-i SECONDS`, `--interval SECONDS` | Reporting interval in seconds (default: 5) |
| `-p PID`, `--pid PID` | Trace only a specific process ID |
| `-f FUNCTION`, `--functions FUNCTION` | Trace only specific functions (can be used multiple times) |
| `-v`, `--verbose` | Increase verbosity (can be used multiple times) |
| `-e COMMAND`, `--exec COMMAND` | Execute and trace the specified command |
| `-t SECONDS`, `--time SECONDS` | Total time in seconds to run the trace |
| `-c`, `--track-count-functions` | Also track functions without size parameters (count only) |
| `-o FILE`, `--output FILE` | Output file for logging results (default: stdout) |
| `-a`, `--check-alignment` | Check memory alignment of function arguments (64-byte boundary) |

## Initialization Process

When using the `-e` option to launch and trace an application, the profiler:

1. Launches application in suspended state
2. Attaches BPF probes to the specified functions
3. Waits for all probes to be properly attached and initialized
4. Resumes application once everything is ready

The default initialization delay is 10 seconds, which is sufficient for most applications. This ensures that all function calls are captured from the very beginning of your application's execution.

## Use Case Examples

### Example 1: Trace Specific Functions in a Running Application
To trace only `memcpy` and `memset` in a process with PID 1234:

```bash
sudo ./prof_libmem.py -p 1234 -f memcpy -f memset
```

### Example 2: Launch an Application with Tracing for a Limited Time
To execute an application and trace it for 30 seconds with reports every 10 seconds:

```bash
sudo ./prof_libmem.py -i 10 -t 30 -e ./myapp arg1 arg2
```

### Example 3: Track Count-Only Functions
To also track functions like strcpy, strlen in addition to the default memory functions:

```bash
sudo ./prof_libmem.py -p 1234 -c
```

### Example 4: Save Results to a File
To record all profiling data to a log file:

```bash
sudo ./prof_libmem.py -o profile_output.log -e ./myapp
```

### Example 5: High Verbosity for Troubleshooting
For maximum diagnostic information:

```bash
sudo ./prof_libmem.py -v -v -e ./myapp
```

### Example 6: Check Memory Alignment
To analyze alignment of source and destination pointers (important for performance):

```bash
sudo ./prof_libmem.py -p 1234 -a
```

## Understanding the Output

The tool provides three types of output:

1. **Histograms** for functions with size parameters (like memcpy, memset):
```
memcpy length:
     (unit)           : count    distribution
         0 -> 1       : 0        |                                        |
         2 -> 3       : 0        |                                        |
         4 -> 7       : 120      |******                                  |
         8 -> 15      : 60       |***                                     |
        16 -> 31      : 402      |********************                    |
        32 -> 63      : 950      |***********************************************|
```

2. **Call counts** for functions without size parameters (when using -c):
```
strlen: 2445 calls
strcpy: 352 calls
```

3. **Alignment statistics** (when using -a) for analyzing memory access patterns:
```
--- Memory Alignment Statistics (64-byte alignment) ---
Alignment Type       Count      Percentage  Distribution
------------------------------------------------------------
Source aligned    : 1520       63.12%      |*************|
Dest aligned      : 1820       75.60%      |***************|
Both aligned      : 1320       54.85%      |***********|
Neither aligned   : 380        15.79%      |***|

Total calls: 2406
```

Additionally, the profiler generates a summary showing the distribution of all traced function calls:

```
--- Function Call Distribution ---
Function       Calls         Percent  Distribution
---------------------------------------------
memcpy         32450         45.21%   |***********************|
strlen         18940         26.40%   |*************|
strcmp         10230         14.26%   |*******|
strncpy        5620          7.83%    |****|
memset         3500          4.88%    |**|
memcmp         1020          1.42%    |*|

Total function calls: 71760
```

## Tips

1. **Use verbose mode** (`-v`) when troubleshooting or when you need more details about the tracing process.
2. **Start with short intervals** to quickly verify that tracing is working properly.
3. **For performance optimization**, focus on the functions with the highest call counts or those handling large memory blocks.
4. **Check alignment statistics** to identify potential performance bottlenecks. Operations where both source and destination are aligned on 64-byte boundaries typically perform better, particularly for larger memory operations.
5. **Multi-thread support is built-in**. The profiler uses atomic operations to ensure accurate counting even when multiple threads call the same functions simultaneously.
6. **System-wide tracing** can be resource-intensive; prefer specifying a target process when possible.
7. **For timing-sensitive applications**, consider adding your own delay at the beginning of your program to ensure all probes are attached before critical code executes.
8. **Check the log for messages** like "Successfully attached X/Y probes" to confirm the profiler is working correctly.
9. **Always place the `-e` option at the end of your command line** to avoid confusion between the profiler's arguments and the executable's arguments.

## Notes

- The tool requires root privileges to use BPF features.
- Some functions may not be traceable in all environments or applications.
- For executables with very short lifetimes, you may need to increase verbosity to debug issues.
- Memory alignment checking works for both distribution-tracked functions and count-only functions that handle pointers.
- All memmove() calls will be tracked under memcpy() as Glibc has common implementation for both these functions.

## Troubleshooting

### BPF Compilation Errors
If you encounter errors like "expected '(' after 'asm'" or "Failed to compile BPF text", this typically indicates a compatibility issue between your BCC version and kernel version. This is commonly seen with BCC 0.8.0 on Kernel 5.4.0 and newer.

**Solutions:**
1. **Upgrade BCC** using distribution-specific methods (see Compatibility Warning in Installation section above)
2. **Update kernel headers**:
   - Debian/Ubuntu: `sudo apt install linux-headers-$(uname -r)`
   - RHEL/CentOS/Fedora: `sudo dnf install kernel-headers kernel-devel` (or `sudo yum install kernel-headers kernel-devel`)
   - SUSE/openSUSE: `sudo zypper install kernel-default-devel`
3. **Check BCC version**: Use the verification commands in the Installation section

### Function Not Found Errors
Some functions may not be available in all C library versions or may be inlined by the compiler.

### Permission Errors
Ensure you're running the profiler with `sudo` as BPF operations require root privileges.

### No Data Collected
If the profiler runs but shows no function calls:
1. Verify the target process is actively using memory functions
2. Check that the process hasn't exited before profiling starts
3. Use verbose mode (`-v`) to see detailed attachment information
