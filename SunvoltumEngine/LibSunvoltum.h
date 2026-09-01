#pragma once

// -------------------------------------------------------
// LibSunvoltum.h — единый макрос экспорта/импорта DLL
// Подключай этот файл везде вместо повторения #ifdef _WIN32
// -------------------------------------------------------

#ifdef _WIN32
    #ifdef SUNOVER_BUILD_DLL
        #define LibSunvoltum __declspec(dllexport)
    #else
        #define LibSunvoltum __declspec(dllimport)
    #endif
#else
    #define LibSunvoltum __attribute__((visibility("default")))
#endif
