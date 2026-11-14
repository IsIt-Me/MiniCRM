cmake_minimum_required(VERSION 3.16)

project(MiniCRM
    VERSION 1.0.0
    DESCRIPTION "Мини-CRM: база клиентов (сквозной проект, вариант 20)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Что собирать. Пример: cmake -S . -B build -DMINICRM_BUILD_SERVER=OFF
option(MINICRM_BUILD_CLIENT "Собрать настольный клиент (Qt Widgets)" ON)
option(MINICRM_BUILD_SERVER "Собрать API-сервер (Qt 6 HttpServer + PostgreSQL)" ON)
include(CTest)   # добавляет опцию BUILD_TESTING (по умолчанию ON)

# Клиент и ядро собираются и на Qt 5.15, и на Qt 6.
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
message(STATUS "MiniCRM: Qt ${QT_VERSION}")

# Общее ядро: сущности Client/Order, Validator, JSON. Используют и клиент, и сервер.
add_subdirectory(core)

if(MINICRM_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(MINICRM_BUILD_SERVER)
    # Сервер требует Qt 6.4+ с модулем HttpServer. Если его нет (например, на
    # Windows без этого модуля), сервер пропускается, клиент собирается дальше.
    find_package(Qt6 QUIET COMPONENTS HttpServer Sql Network)
    if(Qt6HttpServer_FOUND AND Qt6Sql_FOUND)
        add_subdirectory(server)
    else()
        message(WARNING "Qt6 HttpServer/Sql не найдены — сервер не будет собран. "
                        "Соберите его под Linux (см. DEVOPS_START.md).")
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
