find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

add_executable(MiniCRM
    main.cpp
    mainwindow.h mainwindow.cpp
    itemmodel.h itemmodel.cpp
    itemdelegate.h itemdelegate.cpp
    apiclient.h apiclient.cpp
)

target_link_libraries(MiniCRM PRIVATE
    minicrm_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(MiniCRM PRIVATE
    MINICRM_VERSION="${PROJECT_VERSION}")

install(TARGETS MiniCRM RUNTIME DESTINATION bin)
