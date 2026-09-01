cmake_minimum_required(VERSION 3.8)

add_executable(SunvoltumClient "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET SunvoltumClient PROPERTY CXX_STANDARD 17)
endif()

# Подключаем заголовки LibSunvoltum
target_include_directories(SunvoltumClient PRIVATE
    ${CMAKE_SOURCE_DIR}/SunvoltumEngine
)

# Линкуемся с LibSunvoltum.lib (импортная библиотека для .dll)
target_link_libraries(SunvoltumClient PRIVATE LibSunvoltum)

# После сборки — копируем LibSunvoltum.dll рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibSunvoltum>
        $<TARGET_FILE_DIR:SunvoltumClient>
)

# Копируем PlatformContent рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PlatformContent
        $<TARGET_FILE_DIR:SunvoltumClient>/PlatformContent
)
