#pragma once

// -------------------------------------------------------
// LibSunover.h — единый макрос экспорта/импорта DLL
// Подключай этот файл везде вместо повторения #ifdef _WIN32
// -------------------------------------------------------

#ifdef _WIN32
    #ifdef SUNOVER_BUILD_DLL
        #define LibSunover __declspec(dllexport)
    #else
        #define LibSunover __declspec(dllimport)
    #endif
#else
    #define LibSunover __attribute__((visibility("default")))
#endif
