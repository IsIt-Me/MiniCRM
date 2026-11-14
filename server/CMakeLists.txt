# API-сервер: Qt 6 HttpServer (HTTP) + Qt Sql с драйвером QPSQL (PostgreSQL).
# GUI не нужен: сервер собирается и запускается на машине без дисплея.
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql HttpServer)

add_executable(minicrm-server
    main.cpp
    clientrepository.h clientrepository.cpp
)

target_link_libraries(minicrm-server PRIVATE
    minicrm_core
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::HttpServer
)

target_compile_definitions(minicrm-server PRIVATE
    MINICRM_VERSION="${PROJECT_VERSION}")

install(TARGETS minicrm-server RUNTIME DESTINATION bin)
