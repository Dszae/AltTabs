#pragma once
#include <windows.h>

#ifndef WINBOOL
typedef BOOL WINBOOL;
#endif

#ifndef WH_MOD_ID
#define WH_MOD_ID L"chromium-alttab-tabs"
#endif

inline int Wh_GetIntSetting(const wchar_t*) { return 0; }
inline void Wh_Log(const wchar_t*, ...) {}
inline bool Wh_SetFunctionHook(void*, void*, void**) { return true; }
