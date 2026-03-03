cmake_minimum_required(VERSION 3.30)
project(AsyncHttpProxy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Не больше трёх ошибок за раз
    add_compile_options(-fmax-errors=3)
endif()

set(CMAKE_PREFIX_PATH /usr/local/asio)

# Ищем необходимые библиотеки
find_package(Boost REQUIRED)
find_package(GTest REQUIRED)

# Списки используемых файлов
file(GLOB HEADER_FILES "${CMAKE_SOURCE_DIR}/include/*.hpp")
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/main.cpp")


# Создаём статическую библиотеку
add_library(${PROJECT_NAME}_imp STATIC
    "${CMAKE_SOURCE_DIR}/src/headers.cpp"
    "${CMAKE_SOURCE_DIR}/src/http_relay.cpp"
    ${HEADER_FILES})

# Добавляем в проект используемые сторонние библиотеки
target_include_directories(${PROJECT_NAME}_imp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME}_imp PRIVATE 
        WIN32_LEAN_AND_MEAN)
    target_link_libraries(${PROJECT_NAME}_imp PRIVATE 
        ${Boost_LIBRARIES} "stdc++exp" mswsock ws2_32)
else()
    target_link_libraries(${PROJECT_NAME}_imp PRIVATE 
        ${Boost_LIBRARIES} "stdc++exp")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

# Создаём исполняемый таргет и линкуем к нему статическую библиотеку
add_executable(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${PROJECT_NAME}_imp)

#
# Тесты
#

# Рекурсивно ищем все используемые в тестах .cpp файлы
file(GLOB TEST_SRC_FILES "${CMAKE_SOURCE_DIR}/tests/*.cpp")

add_executable(unit_tests "${TEST_SRC_FILES}")
target_link_libraries(unit_tests PRIVATE 
    ${PROJECT_NAME}_imp GTest::GTest GTest::Main)

# Включаем тестирование
enable_testing()
add_test(NAME Tests COMMAND unit_tests)
