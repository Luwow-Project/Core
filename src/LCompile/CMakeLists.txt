add_executable(Luwow.Compile)
target_include_directories(Luwow.Compile PRIVATE
    ${COMPILE_ROOT}
    ${ENGINE_ROOT}
)
target_link_libraries(Luwow.Compile PRIVATE
    Luwow.Engine
    Luau.Ast
    Luau.Compiler
    Luau.VM
    Luau.Common
)
set_target_properties(Luwow.Compile PROPERTIES OUTPUT_NAME compile)
target_sources(Luwow.Compile PRIVATE
    main.cpp
    LuauCompiler.cpp
    LuauCompiler.h
)