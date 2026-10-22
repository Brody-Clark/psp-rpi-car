cmake_minimum_required(VERSION 3.11)
project(psp_rpi_car C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/app.c
    src/display.c
    src/input.c
    src/main.c
    src/network.c
    src/platform.c
    src/utils.c
    src/video.c
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${PROJECT_NAME} PRIVATE
    pspdebug
    pspdisplay
    pspctrl
    psputility
    pspjpeg
    pspmpeg
    pspnet
    pspnet_inet
    pspnet_apctl
    pspge
    pspgu
    pspuser
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE ${PROJECT_NAME}
    VERSION 01.00
)
