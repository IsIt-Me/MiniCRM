# Ядро бизнес-логики — статическая библиотека только на QtCore,
# без GUI: её можно собрать и протестировать на сервере без дисплея.
add_library(minicrm_core STATIC
    client.h client.cpp
    validator.h validator.cpp
)

target_include_directories(minicrm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(minicrm_core PUBLIC Qt${QT_VERSION_MAJOR}::Core)
