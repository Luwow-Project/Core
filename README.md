# Luwow (Luau Tools)

A collection of C++ tools that share the Luau virtual machine.

## Project Overview

This project contains three main executables:

- **lcompile** - Compiler that takes Luau files and produces bytecode in a package.
- **ldebug** - Runtime compiler/executor from a filesystem path to the starting Luau script.
- **lrun** - Runtime executor from a package file.

And 2 libraries:
- **Engine** - Support for registering and loading in binary modules and running.
- **Gui** - OS GUI Support and example of a binary module that is statically bound.

## Requirements

- C++17 compatible compiler
- CMake 3.16 or later
- Luau source code (see setup instructions below)

### Supported Platforms

- Windows (Microsoft Visual Code + Visual C++ compiler)
- MacOS (Microsoft Visual Studio Code + XCode compiler)
- iPadOS (planned)

## Setup Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/MikeFried/Luwow Luwow --recurse-submodules
cd Luwow
```

If you forget the --recurse-submodules flag, you can follow up with:
```bash
git submodules update --init --recursive
```

### 2. Build the Project

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Your output binaries are located in the bin/Release folder under build.

## Usage

### ldebug - Debugger

Execute script from the command line:

```bash
./ldebug script.luau
```

### lcompile - Script compiler

Compiles one or more scripts into a package in the order listed. The first script
is executed. The remainder are expected to be supporting scripts using require.

```bash
./lcompile script.luau test.pkg
```

### lrun - Script Executor

Execute Luau scripts from the command line:

```bash
./lrun test.pkg
```

### lremotedebug - VS Code / DAP compatible remote debugger

Attach from VS Code to debug your script.

Depends on [Luau Debugger Extension](https://marketplace.visualstudio.com/items?itemName=sssooonnnggg.luau-debugger)

Add this example to launch.json in .vscode to attach to the debugger.

```json
        {
            "name": "attach to luau debugger",
            "type": "luau",
            "request": "attach",
            "address": "localhost",
            "port": 59000
        }
```

You run the command in terminal in the following way before attempting to attach.

```bash
./lremotedebug 59000 script.luau
```

## Acknowledgments

- [Luau](https://github.com/Roblox/luau) - The scripting language and virtual machine
- [Roblox](https://www.roblox.com/) - For creating and maintaining Luau
- [sssooonnnggg](https://github.com/sssooonnnggg) - For creating the Luau-Debugger
- [Google C++ DAP](https://github.com/google/cppdap) - Google C++ Debugger Adapter Protocol
- [Niels Lohmann JSON](https://github.com/nlohmann/json) - JSON for Modern C++