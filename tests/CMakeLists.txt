find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

# Тесты ядра: не требуют ни GUI, ни базы данных — запускаются в CI где угодно.
foreach(name test_validator test_client_json)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE minicrm_core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
