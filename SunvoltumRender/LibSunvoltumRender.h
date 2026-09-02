#pragma once

// -------------------------------------------------------
// LibSunvoltumRender.h — макрос экспорта/импорта DLL рендера
// -------------------------------------------------------

#ifdef _WIN32
    #ifdef SUNOVER_RENDER_BUILD_DLL
        #define LibSunvoltumRender __declspec(dllexport)
    #else
        #define LibSunvoltumRender __declspec(dllimport)
    #endif
#else
    #define LibSunvoltumRender __attribute__((visibility("default")))
#endif
