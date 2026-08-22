cmake_minimum_required(VERSION 3.8)

add_executable(SunoverClient "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET SunoverClient PROPERTY CXX_STANDARD 17)
endif()

# Подключаем заголовки LibSunover
target_include_directories(SunoverClient PRIVATE
    ${CMAKE_SOURCE_DIR}/SunoverEngine
)

# Линкуемся с LibSunover.lib (импортная библиотека для .dll)
target_link_libraries(SunoverClient PRIVATE LibSunover)

# После сборки — копируем LibSunover.dll рядом с .exe
add_custom_command(TARGET SunoverClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibSunover>
        $<TARGET_FILE_DIR:SunoverClient>
)

# Копируем PlatformContent рядом с .exe
add_custom_command(TARGET SunoverClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PlatformContent
        $<TARGET_FILE_DIR:SunoverClient>/PlatformContent
)
