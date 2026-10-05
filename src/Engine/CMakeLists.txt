find_package(Threads REQUIRED)

add_library(Luwow.Engine STATIC)
target_include_directories(Luwow.Engine PRIVATE .)
target_link_libraries(Luwow.Engine PUBLIC Luau.VM Luau.Common Luau.Config Threads::Threads ${CMAKE_DL_LIBS})
target_sources(Luwow.Engine PRIVATE
    ILuauHost.h
    ILuauModule.h
    Engine.cpp
    Engine.h
    MessageBus.cpp
    MessageBus.h
    ModuleLayers.cpp
    ModuleLayers.h
    ModuleLoader.cpp
    ModuleLoader.h
    StateMutex.h
    Package.cpp
    Package.h
    Require.cpp
    Require.h
    Config.cpp
    Config.h
    Userdata.h
)
