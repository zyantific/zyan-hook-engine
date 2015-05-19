Zyan Hook Engine
====================

Advanced x86/x64 hooking library.

## Features ##

- x86 and x64 support
- Lots of runtime checks to prevent erroneous usage.

- **Inline Hook**
 - Various common hook methods (5 byte relative jump, 6 byte absolut jump, ...).
 - Intelligent detection of the most suitable hook method.
 - Allows the installation of multiple hooks at the same address (even third party hooks are supported, if they are using standard 5 byte relative jumps).
 - ...

- **Exception Hook**
 - Various common hook methods (INT3 breakpoint, Privileged instructions, ...).
 - Supports previously installed 5 byte relative jump hooks at the same address.

- **Context Hook**
 - Hooks code by manipulating the windows debug registers (hardware breakpoint hook). Not a single byte of code is changed.
 - Manually select one of the four debug registers, or let the engine choose a suitable debug register for you.
 - You can either hook all running threads or just specific ones.
 - Supports previously installed 5 byte relative jump hooks.
 - Compatibility mode that tries to save at least 5 bytes of original instructions to the trampoline. This way you can attach 5 byte relative jumps to the same address as the context hook without crashing your application. 

- **IAT / EAT Hook**
 - Hooks code by manipulating the import and export address tables of windows binaries.
 - ...

## License ##
Zyan Hook Engine is licensed under the MIT License. Dependencies are under their respective licenses.
