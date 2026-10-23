add_executable(Luwow.RunPackage)
target_include_directories(Luwow.RunPackage PRIVATE
    ${RUN_PACKAGE_ROOT}
    ${ENGINE_ROOT}
)
target_link_libraries(Luwow.RunPackage PRIVATE
    Luwow.Engine
)
set_target_properties(Luwow.RunPackage PROPERTIES OUTPUT_NAME runpackage)
target_sources(Luwow.RunPackage PRIVATE
    getExecutablePath.h
    main.cpp
    ${RUN_SCRIPT_ROOT}/StaticModules.h
)
target_include_directories(Luwow.RunPackage PRIVATE ${RUN_SCRIPT_ROOT})

# Libraries statically bound into runpackage, like runscript, the default source registers none
if(LUWOW_STATIC_MODULES_SOURCE)
    target_sources(Luwow.RunPackage PRIVATE ${LUWOW_STATIC_MODULES_SOURCE})
    target_link_libraries(Luwow.RunPackage PRIVATE ${LUWOW_STATIC_MODULE_TARGETS})
else()
    target_sources(Luwow.RunPackage PRIVATE ${RUN_SCRIPT_ROOT}/StaticModules.cpp)
endif()

# Exports the Luau API so runpackage can load module DLLs
if(COMMAND luwow_enable_module_host)
    luwow_enable_module_host(Luwow.RunPackage)
endif()
