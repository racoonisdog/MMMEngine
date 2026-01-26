#pragma once

#ifdef USERSCRIPTS_EXPORT
#define USERSCRIPTS __declspec(dllexport)
#else
#define USERSCRIPTS __declspec(dllimport)
#endif