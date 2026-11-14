/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

// Thread management routines for SDL

extern "C" {
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"
}

#include <thread>
#include <system_error>

#ifdef SDL_PLATFORM_WINRT
#include <Windows.h>
#endif

static SDL_ThreadID ThreadIDOf(const std::thread::id &cpp_thread_id)
{
    static_assert(sizeof(std::thread::id) <= sizeof(SDL_ThreadID), "std::thread::id must not be bigger than SDL_ThreadID");
    SDL_ThreadID thread_id{};
    SDL_memcpy(&thread_id, &cpp_thread_id, sizeof(std::thread::id));
    return thread_id;
}

static void RunThread(void *args)
{
    SDL_Thread *thread = (SDL_Thread *)args;
    /* SDL_GetThreadID() of this thread, from its start (the creator sets it
       too, once std::thread is made). WASAPI's management thread compares it
       with SDL_GetCurrentThreadID() to run its own tasks directly; left 0,
       it waited for itself when opening an audio device. */
    thread->threadid = ThreadIDOf(std::this_thread::get_id());
    SDL_RunThread(thread);
}

extern "C"
bool SDL_SYS_CreateThread(SDL_Thread *thread,
                     SDL_FunctionPointer pfnBeginThread,
                     SDL_FunctionPointer pfnEndThread)
{
    try {
        // !!! FIXME: no way to set a thread stack size here.
        std::thread *cpp_thread = new std::thread(RunThread, thread);
        thread->threadid = ThreadIDOf(cpp_thread->get_id());
        thread->handle = (void *)cpp_thread;
        return true;
    } catch (std::system_error &ex) {
        return SDL_SetError("unable to start a C++ thread: code=%d; %s", ex.code().value(), ex.what());
    } catch (std::bad_alloc &) {
        return SDL_OutOfMemory();
    }
}

extern "C"
void SDL_SYS_SetupThread(const char *name)
{
    // Do nothing.
}

extern "C"
SDL_ThreadID SDL_GetCurrentThreadID(void)
{
    return ThreadIDOf(std::this_thread::get_id());
}

extern "C"
bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
#ifdef SDL_PLATFORM_WINRT
    int value;

    if (priority == SDL_THREAD_PRIORITY_LOW) {
        value = THREAD_PRIORITY_LOWEST;
    } else if (priority == SDL_THREAD_PRIORITY_HIGH) {
        value = THREAD_PRIORITY_HIGHEST;
    } else if (priority == SDL_THREAD_PRIORITY_TIME_CRITICAL) {
        // FIXME: WinRT does not support TIME_CRITICAL! -flibit
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "TIME_CRITICAL unsupported, falling back to HIGHEST");
        value = THREAD_PRIORITY_HIGHEST;
    } else {
        value = THREAD_PRIORITY_NORMAL;
    }
    if (!SetThreadPriority(GetCurrentThread(), value)) {
        return WIN_SetError("SetThreadPriority()");
    }
    return true;
#else
    return SDL_Unsupported();
#endif
}

extern "C"
void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    if (!thread) {
        return;
    }

    try {
        std::thread *cpp_thread = (std::thread *)thread->handle;
        if (cpp_thread) {
            if (cpp_thread->joinable()) {
                cpp_thread->join();
            }
            delete cpp_thread;
            thread->handle = nullptr;
        }
    } catch (std::system_error &) {
        // An error occurred when joining the thread.  SDL_WaitThread does not,
        // however, seem to provide a means to report errors to its callers
        // though!
    }
}

extern "C"
void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    if (!thread) {
        return;
    }

    try {
        std::thread *cpp_thread = (std::thread *)thread->handle;
        if (cpp_thread) {
            if (cpp_thread->joinable()) {
                cpp_thread->detach();
            }
            delete cpp_thread;
            thread->handle = nullptr;
        }
    } catch (std::system_error &) {
        // An error occurred when detaching the thread.  SDL_DetachThread does not,
        // however, seem to provide a means to report errors to its callers
        // though!
    }
}

static thread_local SDL_TLSData *thread_local_storage;

extern "C"
void SDL_SYS_InitTLSData(void)
{
}

extern "C"
SDL_TLSData * SDL_SYS_GetTLSData(void)
{
    return thread_local_storage;
}

extern "C"
bool SDL_SYS_SetTLSData(SDL_TLSData *data)
{
    thread_local_storage = data;
    return true;
}

extern "C"
void SDL_SYS_QuitTLSData(void)
{
}
