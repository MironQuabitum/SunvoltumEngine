# CMakeList.txt: файл проекта CMake верхнего уровня; проведите глобальную конфигурацию
# и включите сюда подпроекты.
#
cmake_minimum_required (VERSION 3.8)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("SunvoltumEngine")
find_package(OpenAL CONFIG REQUIRED)
find_package(unofficial-luau CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
add_subdirectory ("SunvoltumEngine")
add_subdirectory ("SunvoltumRender")
add_subdirectory ("SunvoltumClient")
add_subdirectory ("SunvoltumServer")
add_subdirectory ("SunvoltumPhysics")
add_subdirectory ("SunvoltumPDemo")

