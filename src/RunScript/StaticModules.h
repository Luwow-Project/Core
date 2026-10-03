#pragma once

namespace Luwow {
    // Registers the libraries statically bound into this executable. CMake generates the
    // implementation from the enabled libraries, StaticModules.cpp is the empty default.
    void registerStaticModules();
}
