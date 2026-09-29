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
target_sources(Luwow.RunScript PRIVATE
    main.cpp
    StaticModules.h
)

# Libraries statically bound into runscript, the default source registers none
if(LUWOW_STATIC_MODULES_SOURCE)
    target_sources(Luwow.RunScript PRIVATE ${LUWOW_STATIC_MODULES_SOURCE})
    target_link_libraries(Luwow.RunScript PRIVATE ${LUWOW_STATIC_MODULE_TARGETS})
else()
    target_sources(Luwow.RunScript PRIVATE StaticModules.cpp)
endif()

# Exports the Luau API so runscript can load module DLLs
if(COMMAND luwow_enable_module_host)
    luwow_enable_module_host(Luwow.RunScript)
endif()
