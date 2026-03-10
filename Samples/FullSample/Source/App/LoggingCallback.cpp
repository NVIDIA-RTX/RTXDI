/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "LoggingCallback.h"

#include <mutex>
#include <string>

#include <donut/core/log.h>

#if _WIN32
#include <Windows.h>
#endif

using namespace donut;

extern const std::string g_ApplicationTitle;

void ApplicationLogCallback(log::Severity severity, const char* message)
{
    static std::mutex g_LogMutex;

    const char* severityText = "";
    switch (severity)
    {
    case log::Severity::Debug: severityText = "DEBUG";  break;
    case log::Severity::Info: severityText = "INFO";  break;
    case log::Severity::Warning: severityText = "WARNING"; break;
    case log::Severity::Error: severityText = "ERROR"; break;
    case log::Severity::Fatal: severityText = "FATAL ERROR"; break;
    default:
        break;
    }

    {
        std::lock_guard<std::mutex> lockGuard(g_LogMutex);

#if defined(_WIN32) && !defined(IS_CONSOLE_APP)
        static char buf[4096];
        snprintf(buf, std::size(buf), "%s: %s", severityText, message);

        OutputDebugStringA(buf);
        OutputDebugStringA("\n");

        if (severity >= log::Severity::Error)
        {
            MessageBoxA(nullptr, buf, g_ApplicationTitle.c_str(), MB_ICONERROR);
        }
#else
        FILE* fd = (severity >= log::Severity::Error) ? stderr : stdout;

        fprintf(fd, "%s: %s\n", severityText, message);
        fflush(fd);
#endif
    }

    if (severity == log::Severity::Fatal)
        abort();
}