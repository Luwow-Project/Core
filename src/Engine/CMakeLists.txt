add_library(Luwow.Engine STATIC)
target_include_directories(Luwow.Engine PRIVATE .)
target_link_libraries(Luwow.Engine PUBLIC Luau.VM Luau.Common)
target_sources(Luwow.Engine PRIVATE
    Engine.cpp
    Engine.h
    ILuauModule.h
    Package.cpp
    Package.h
)