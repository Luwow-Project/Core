# Luwow Core Tools

A collection of C++ tools that share the Luau virtual machine.

This repository is not stand-alone. You must build it externally from within another
repository like https://github.com/Luwow-Project/Release

## Project Overview

This project contains the sources for Luwow core executables:

- **compile** - Compiler that takes Luau files and produces bytecode in a package.
- **runscript** - Runtime compiler/executor from a filesystem path to the starting Luau script.
- **runpackage** - Runtime executor from a package file.

And the core library:
- **Engine** - Support for registering and loading in binary modules and running.

## Acknowledgments

- [Luau](https://github.com/Roblox/luau) - The scripting language and virtual machine
- [Roblox](https://www.roblox.com/) - For creating and maintaining Luau
