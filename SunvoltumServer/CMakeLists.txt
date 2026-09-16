cmake_minimum_required(VERSION 3.8)

add_executable(SunvoltumServer "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET SunvoltumServer PROPERTY CXX_STANDARD 17)
endif()

# Заголовки движка + SunvoltumRender (нужен для LibSunvoltumRender.h макроса,
# который используется в SunvoltumInput.h и Runtime.h через LibSunvoltum)
target_include_directories(SunvoltumServer PRIVATE
    ${CMAKE_SOURCE_DIR}/SunvoltumEngine
    ${CMAKE_SOURCE_DIR}/SunvoltumRender
)

# Линкуемся только с LibSunvoltum — без LibSunvoltumRender (нет рендера)
target_link_libraries(SunvoltumServer PRIVATE LibSunvoltum)

# После сборки — копируем LibSunvoltum.dll рядом с .exe
add_custom_command(TARGET SunvoltumServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibSunvoltum>
        $<TARGET_FILE_DIR:SunvoltumServer>
)

# Копируем PlatformContent рядом с .exe
add_custom_command(TARGET SunvoltumServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PlatformContent
        $<TARGET_FILE_DIR:SunvoltumServer>/PlatformContent
)

# Копируем серверные скрипты рядом с .exe
add_custom_command(TARGET SunvoltumServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Scripts
        $<TARGET_FILE_DIR:SunvoltumServer>/Scripts
)
