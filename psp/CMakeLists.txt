cmake_minimum_required(VERSION 3.11)
set(CMAKE_C_STANDARD 11)
project(controls_v1)

add_executable(${PROJECT_NAME} main.c)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pspdebug
    pspdisplay
    pspge
    pspctrl
    psputility
    pspjpeg
    pspmpeg
    pspnet
    pspgu 
    pspgum
    pspnet_inet
    pspnet_resolver
    pspnet_apctl
)

# target_link_libraries(${PROJECT_NAME} PRIVATE ${PSP_SDK_LIBS})


# Create an EBOOT.PBP file
create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE ${PROJECT_NAME}
    VERSION 01.00
)
