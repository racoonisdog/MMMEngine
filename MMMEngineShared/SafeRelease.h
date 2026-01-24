#pragma once

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)        \
    do {                       \
        if ((p) != nullptr)    \
        {                      \
            (p)->Release();    \
            (p) = nullptr;     \
        }                      \
    } while (0)
#endif