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
)