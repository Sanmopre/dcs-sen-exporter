cmake_minimum_required(VERSION 3.20)
project(sen_dcs_exporter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find sen package
list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PATH}/cmake")
find_package(sen REQUIRED)

# Add submodules
add_subdirectory(submodules/spdlog)
add_subdirectory(submodules/cli11)

add_executable(${PROJECT_NAME}
src/main.cpp
        src/physical_entity_manager.cpp
        src/physical_entity_manager.h
        src/data_definitions.h
        src/component.cpp
        src/component.h)

sen_generate_cpp(
        TARGET
        ${PROJECT_NAME}
        HLA_FOM_DIRS
        "$ENV{STD_FOM_PATH}/fom/rpr"
        "$ENV{STD_FOM_PATH}/fom/mapping"
        HLA_MAPPINGS_FILE
        "$ENV{STD_FOM_PATH}/fom/mapping/mapping.xml"
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        sen::core
        sen::kernel
        sen::util
        spdlog::spdlog
        CLI11::CLI11
)

target_include_directories(${PROJECT_NAME} PUBLIC submodules/json/include)