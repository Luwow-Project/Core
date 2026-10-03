add_library(Luwow.Engine STATIC)
target_include_directories(Luwow.Engine PRIVATE .)
target_link_libraries(Luwow.Engine PUBLIC Luau.VM Luau.Common Luau.Config ${CMAKE_DL_LIBS})
target_sources(Luwow.Engine PRIVATE
    ILuauHost.h
    ILuauModule.h
    Engine.cpp
    Engine.h
    ModuleLoader.cpp
    ModuleLoader.h
    Package.cpp
    Package.h
    Require.cpp
    Require.h
    Config.cpp
    Config.h
    Userdata.h
)
