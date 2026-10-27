set(LUWOW_EXECUTABLE_TARGET "Luwow.RunScript" CACHE STRING "Target name of the Luwow host executable")

if(NOT LUWOW_LIBRARY_MODE MATCHES "^(STATIC|SHARED)$")
    message(FATAL_ERROR "LUWOW_LIBRARY_MODE must be STATIC or SHARED, got '${LUWOW_LIBRARY_MODE}'")
endif()
if(NOT LUWOW_BUILD_EXECUTABLES AND NOT LUWOW_BUILD_LIBRARIES)
    message(FATAL_ERROR "Nothing to build, enable LUWOW_BUILD_EXECUTABLES and/or LUWOW_BUILD_LIBRARIES")
endif()
if(NOT LUWOW_BUILD_EXECUTABLES AND LUWOW_LIBRARY_MODE STREQUAL "STATIC")
    message(FATAL_ERROR "STATIC libraries are bound into the executable, enable LUWOW_BUILD_EXECUTABLES or set LUWOW_LIBRARY_MODE to SHARED")
endif()

# Static libraries such as uv_a end up inside module DLLs, which needs position-independent code on Linux
if(LUWOW_LIBRARY_MODE STREQUAL "SHARED")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Module DLLs are named <name><suffix>, ModuleLoader.h expects the same suffixes
if(WIN32)
    set(LUWOW_MODULE_SUFFIX ".dll")
elseif(APPLE)
    set(LUWOW_MODULE_SUFFIX ".dylib")
else()
    set(LUWOW_MODULE_SUFFIX ".so")
endif()

# Module DLLs are placed in a "libraries" folder next to the executable
get_property(LUWOW_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(LUWOW_MULTI_CONFIG)
    set(LUWOW_MODULE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/libraries")
else()
    set(LUWOW_MODULE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libraries")
endif()

set(LUWOW_MODULES_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/StaticModules.cpp.in")
set(LUWOW_MODULE_HOST_SOURCE "${CMAKE_CURRENT_LIST_DIR}/ModuleHost.cpp")

# Exports the Luau VM from Luau.VM's objects, call once after adding Luau in SHARED mode
function(luwow_export_luau_api)
    if(NOT LUWOW_LIBRARY_MODE STREQUAL "SHARED")
        return()
    endif()

    if(MSVC)
        target_compile_definitions(Luau.VM PRIVATE "LUA_API=extern __declspec(dllexport)")
    else()
        target_compile_definitions(Luau.VM PRIVATE "LUA_API=extern __attribute__((visibility(\"default\")))")
    endif()
endfunction()

# Makes an executable export the whole Luau API so module DLLs can link against it
function(luwow_enable_module_host target)
    if(NOT LUWOW_LIBRARY_MODE STREQUAL "SHARED")
        return()
    endif()

    set_target_properties(${target} PROPERTIES ENABLE_EXPORTS ON)
    target_compile_definitions(${target} PRIVATE LUWOW_MODULE_HOST)

    # Keep every Luau.VM object, not only the ones the executable uses itself
    if(MSVC)
        target_link_options(${target} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Luau.VM>")
    elseif(APPLE)
        target_link_options(${target} PRIVATE "LINKER:-force_load,$<TARGET_FILE:Luau.VM>")
    else()
        target_link_options(${target} PRIVATE "LINKER:--whole-archive" "$<TARGET_FILE:Luau.VM>" "LINKER:--no-whole-archive")
    endif()
endfunction()

# Adds a Luwow library, bound into the executable in STATIC mode or built as a module DLL in SHARED mode.
# The sources must use LUWOW_REGISTER_MODULE once, NAME is its module id and DLL file name.
# MODES lists the run modes the library supports (SERIAL, PARALLEL), the first is the default.
#   luwow_add_library(<target> NAME <id> MODES <modes...> SOURCES <sources...>)
function(luwow_add_library target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "NAME" "MODES;SOURCES")
    if(NOT ARG_NAME OR NOT ARG_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "luwow_add_library(${target}) needs a NAME that is a valid C identifier")
    endif()

    if(NOT ARG_MODES)
        set(ARG_MODES SERIAL)
    endif()
    foreach(mode IN LISTS ARG_MODES)
        if(NOT mode MATCHES "^(SERIAL|PARALLEL)$")
            message(FATAL_ERROR "luwow_add_library(${target}) has an unknown mode '${mode}', expected SERIAL or PARALLEL")
        endif()
    endforeach()

    string(TOUPPER ${ARG_NAME} upperName)
    set(modeOption LUWOW_${upperName}_MODE)
    list(GET ARG_MODES 0 defaultMode)
    set(${modeOption} ${defaultMode} CACHE STRING "Run mode of the ${ARG_NAME} library: ${ARG_MODES}")
    set_property(CACHE ${modeOption} PROPERTY STRINGS ${ARG_MODES})

    set(runMode ${${modeOption}})
    if(NOT runMode IN_LIST ARG_MODES)
        message(FATAL_ERROR "${modeOption} is ${runMode}, but the ${ARG_NAME} library only supports: ${ARG_MODES}")
    endif()
    message(STATUS "Luwow library ${ARG_NAME}: ${runMode}")

    if(LUWOW_LIBRARY_MODE STREQUAL "SHARED")
        add_library(${target} MODULE)
        set_target_properties(${target} PROPERTIES
            OUTPUT_NAME ${ARG_NAME}
            PREFIX ""
            SUFFIX ${LUWOW_MODULE_SUFFIX}
            LIBRARY_OUTPUT_DIRECTORY ${LUWOW_MODULE_OUTPUT_DIRECTORY}
        )
        target_compile_definitions(${target} PRIVATE
            LUWOW_BUILDING_MODULE
            $<TARGET_PROPERTY:Luau.VM,INTERFACE_COMPILE_DEFINITIONS>
        )

        # Only Luau's headers, the VM itself comes from the host executable at load time
        target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:Luau.VM,INTERFACE_INCLUDE_DIRECTORIES>)
        target_link_libraries(${target} PRIVATE ${LUWOW_EXECUTABLE_TARGET})

        # On Windows the imports name the executable built with, so they're delay-loaded and resolved from the
        # executable the module is loaded into. Elsewhere they bind to the main executable.
        if(MSVC)
            set(hostImport "$<TARGET_FILE_NAME:${LUWOW_EXECUTABLE_TARGET}>")
            target_sources(${target} PRIVATE ${LUWOW_MODULE_HOST_SOURCE})
            target_compile_definitions(${target} PRIVATE "LUWOW_HOST_IMPORT=\"${hostImport}\"")
            target_link_options(${target} PRIVATE "/DELAYLOAD:${hostImport}")
            target_link_libraries(${target} PRIVATE delayimp)
        endif()

        install(TARGETS ${target} LIBRARY DESTINATION bin/libraries COMPONENT libraries)
    else()
        add_library(${target} STATIC)
        target_compile_definitions(${target} PRIVATE LUWOW_MODULE_ID=${ARG_NAME})
        target_link_libraries(${target} PRIVATE Luau.VM)

        set_property(GLOBAL APPEND PROPERTY LUWOW_STATIC_MODULES ${ARG_NAME})
        set_property(GLOBAL APPEND PROPERTY LUWOW_STATIC_MODULE_TARGETS ${target})
    endif()

    if(runMode STREQUAL "PARALLEL")
        target_compile_definitions(${target} PRIVATE LUWOW_MODULE_PARALLEL=1)
    else()
        target_compile_definitions(${target} PRIVATE LUWOW_MODULE_PARALLEL=0)
    endif()

    target_include_directories(${target} PRIVATE ${ENGINE_ROOT})
    target_sources(${target} PRIVATE ${ARG_SOURCES})
endfunction()

# Generates the source registering every STATIC library, sets LUWOW_STATIC_MODULES_SOURCE
# and LUWOW_STATIC_MODULE_TARGETS for the executable. Call after adding the libraries.
function(luwow_generate_static_modules)
    get_property(names GLOBAL PROPERTY LUWOW_STATIC_MODULES)
    get_property(targets GLOBAL PROPERTY LUWOW_STATIC_MODULE_TARGETS)
    if(NOT names)
        return()
    endif()

    set(LUWOW_STATIC_MODULE_DECLARATIONS "")
    set(LUWOW_STATIC_MODULE_REGISTRATIONS "")
    foreach(name IN LISTS names)
        string(APPEND LUWOW_STATIC_MODULE_DECLARATIONS "Luwow::Engine::ILuauModule* luwow_create_module_${name}();\n")
        string(APPEND LUWOW_STATIC_MODULE_REGISTRATIONS "        Luwow::Engine::Engine::registerNativeModule(std::shared_ptr<Luwow::Engine::ILuauModule>(luwow_create_module_${name}()));\n")
    endforeach()

    set(output "${CMAKE_BINARY_DIR}/generated/StaticModules.cpp")
    configure_file(${LUWOW_MODULES_TEMPLATE} ${output} @ONLY)

    set(LUWOW_STATIC_MODULES_SOURCE ${output} PARENT_SCOPE)
    set(LUWOW_STATIC_MODULE_TARGETS ${targets} PARENT_SCOPE)
endfunction()