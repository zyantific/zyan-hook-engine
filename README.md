# Zyrex

<img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT"><a href="https://gitter.im/zyantific/zyrex?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=body_badge">
  <img src="https://badges.gitter.im/zyantific/zyrex.svg" alt="Gitter">
</a>
<a href="https://discord.zyantific.com/">
  <img src="https://img.shields.io/discord/390136917779415060.svg?logo=discord&label=Discord">
</a>

Advanced x86/x86-64 hooking library for Windows

## Readme

Everything in this repository is highly WiP and will probably not work as intended right now. Due to lack of time, development is currently on halt, but will hopefully resumed soon.

## Features

- Supports x86 and x86-64 (uses our [Zydis](https://github.com/zyantific/zydis) diassembler library)
- Extremely safe and easy to use ([read more](./doc/Safety.md))
- Thread-safe by design due to a [Transactional API](./doc/Transaction.md)
- Inbuild [Barrier API](./doc/Barrier.md) to prevent unwanted hook recursion
- Complete doxygen documentation ([master](insert_link_here))

### Hooking methods

#### Inline Hook

Patches the prologue of a function to redirect its codeflow and allocates a trampoline which can be used to continue execution of the original function.

## Roadmap

- Windows kernel-mode support
- Multi-platform support (macOS, FreeBSD, Linux and UEFI)
- Software-Breakpoint (SWBP) Hook
  - Writes an interrupt/privileged instruction at the begin of a target function and redirects codeflow by catching the resulting exceptions in an unhandled exception handler (Windows only).
- Hardware-Breakpoint (HWBP) Hook
  - Hooks code using the CPU debug registers. Not a single byte of code is changed (Windows only).
- Import/Export Address Table Hook
  - Hooks code by replacing import-address table (IAT) and export-address table (EAT) entries of COFF binaries at runtime (Windows only).
- Virtual-Method-Table Hook
  - Hooks code by replacing virtual-method-table (VMT) entries of object instances at runtime.

## Build

#### Unix

Zyrex builds cleanly on most platforms without any external dependencies. You can use CMake to generate project files for your favorite C99 compiler.

```bash
git clone --recursive 'https://github.com/zyantific/zyrex.git'
cd zyrex
mkdir build && cd build
cmake ..
make
```

#### Windows

Either use the [Visual Studio 2017 project](./msvc/) or build Zyrex using [CMake](https://cmake.org/download/) ([video guide](https://www.youtube.com/watch?v=fywLDK1OAtQ)).

## Versions

#### Scheme
Versions follow the [semantic versioning scheme](https://semver.org/). All stability guarantees apply to the API only — ABI stability between patches cannot be assumed unless explicitly mentioned in the release notes.

#### Branches
- `master` holds the bleeding edge code of the next, unreleased Zyrex version. Elevated amounts of bugs and issues must be expected, API stability is not guaranteed outside of tagged commits.
- `maintenance/v1` contains the code of the latest stable v1 release.

## License

Zyrex is licensed under the MIT License.
