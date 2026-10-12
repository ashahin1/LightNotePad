#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <functional>
#include <chrono>
