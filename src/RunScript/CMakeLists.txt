add_executable(Luwow.RunScript)
target_include_directories(Luwow.RunScript PRIVATE
    ${RUN_SCRIPT_ROOT}
    ${ENGINE_ROOT}
)
target_link_libraries(Luwow.RunScript PRIVATE
    Luwow.Engine
    Luau.Ast
    Luau.Compiler
    Luau.VM
    Luau.Common
)
set_target_properties(Luwow.RunScript PROPERTIES OUTPUT_NAME runscript)
target_sources(Luwow.RunScript PRIVATE main.cpp)
