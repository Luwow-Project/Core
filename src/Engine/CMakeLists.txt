add_library(Luwow.Engine STATIC)
target_include_directories(Luwow.Engine PRIVATE .)
target_link_libraries(Luwow.Engine PUBLIC Luau.VM Luau.Common)
target_sources(Luwow.Engine PRIVATE
    ILuauModule.h
    Engine.cpp
    Engine.h
    Package.cpp
    Package.h
    FileUtils.cpp
    FileUtils.h
    RequireResolver.cpp
    RequireResolver.h
)