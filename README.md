# Unmark - Advanced Memory Protection Manipulation

Kernel driver for manipulating memory protections while bypassing standard protection mechanisms. This allows for hidden execution of code in memory regions that appear to have restricted permissions.

## Features
- Direct page table manipulation
- CR3 register access and modification
- Cross-process memory protection alteration
- Hidden RWX capabilities while displaying different permissions
- Physical memory mapping preservation

## Technical Details
The driver operates by directly manipulating page table entries and CR3 registers, allowing for fine-grained control over memory permissions. It can maintain hidden RWX permissions while displaying different protection levels to monitoring tools.

### PatchGuard Interaction
The driver has been extensively tested against PatchGuard on various Windows versions. It does indeed trigger PatchGuard and KPP, but this is not a problem as third-party tools can disable PatchGuard and KPP like EfiGuard, Shark, ...

## Testing Results
Tested on multiple systems with different Windows versions:
- Windows 10 22H2 (19045.3803) - Fully operational
- Windows 10 21H2 (19044.2965) - Fully operational
- Windows 11 23H2 (22631.2861) - Fully operational
- Windows 11 22H2 (22621.2715) - Fully operational

All tests performed on both AMD and Intel systems:.

## Usage
The driver provides a straightforward interface through IOCTLs:
```cpp
REQUEST___PROTECT_REGION
REQUEST___UNPROTECT_REGION
REQUEST___ATTACH_PROCESS
REQUEST___DETACH_PROCESS
REQUEST___GET_PROCESS_CR3
REQUEST___SWITCH_PROCESS_CONTEXT
```

## Building
Built using Visual Studio 2022 with Windows Driver Kit (WDK) version 10.0.22621.382 or higher.

## Known Issues
- Rare timing-related issues on heavy system load
- Some antivirus solutions may flag the driver (expected due to its functionality)

## License
This project is for research and educational purposes only. Use at your own risk.
