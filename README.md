Zyrex
====================

Advanced x86/x64 hooking library.

## Readme

Everything in this repository is highly WiP and will probably not work as intended right now. Due to lack of time, development is currently on halt, but will hopefully resumed soon.

## Features

### Core Features

- Transaction based API
- x86 and x64 support

### Inline Hook

- The most common hook method. Patches the first instructions of a target function to redirect the code-flow.
- Allows the installation of multiple hooks at the same address (even third party hooks are supported, if they are using standard 5 byte relative jumps).
- ...

### Exception Hook

- Writes interrupt/privileged instructions at the begin of a target function and redirects code-flow by catching the resulting exceptions in an unhandled exception handler.
- Supports previously installed inline hooks (5 byte relative jump) at the same address.

### Hardware-Breakpoint Hook

- Hooks code by manipulating the windows debug registers (hardware breakpoint hook). Not a single byte of code is changed.
- Manually select one of the four debug registers, or let the engine choose a suitable debug register for you.
- You can either hook all running threads or just specific ones.
- Supports previously installed inline hooks (5 byte relative jump).
- Compatibility mode that tries to save at least 5 bytes of original instructions to the trampoline. This way you can attach 5 byte relative jumps to the same address as the context hook without crashing your application. 

### IAT / EAT Hook

- Hooks code by manipulating the import- and export-address-tables of windows binaries.
- ...

### VMT Hook

- Hooks code by replacing the whole virtual-method-table (VMT) or single functions from the VMT of a given object instance.

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

## License

Zyrex is licensed under the MIT License.
