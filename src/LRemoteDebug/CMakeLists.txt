# if(ANDROID)
#   add_library(luaud SHARED)
#   target_link_libraries(luaud PRIVATE android log)
# else()
add_executable(Luwow.LRemoteDebug)
# endif()

target_sources(Luwow.LRemoteDebug
  PRIVATE
  main.cpp
  #luau_runtime.cpp
)
target_link_libraries(Luwow.LRemoteDebug PRIVATE
    Luwow.Engine
    Luau.VM
    Luau.Compiler
    Luau.CLI.lib
    Luau.Config
    Luau.Debugger
)

set_target_properties(Luwow.LRemoteDebug PROPERTIES OUTPUT_NAME lremotedebug)

target_include_directories(Luwow.LRemoteDebug PRIVATE
    ${LUAU_ROOT}/VM/include
    ${LUAU_ROOT}/VM/src
    ${LUAU_ROOT}/Compiler/include
    ${LUAU_ROOT}/CLI
    ${LUAU_ROOT}/Config/include
    ${CMAKE_SOURCE_DIR}/src/Engine
    ${CMAKE_SOURCE_DIR}/extern/luau-debugger/debugger/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)