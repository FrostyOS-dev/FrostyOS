/*
Copyright (©) 2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Server.hpp"

#include "Backends/VideoBackend.hpp"

#if !defined(WM_BACKEND_FROSTYOS) && defined(__frostyos__)
#define WM_BACKEND_FROSTYOS 1
#endif

#ifdef WM_BACKEND_FROSTYOS
#include "Backends/FrostyOS/FrostyOSVideoBackend.hpp"
#endif

int main(int argc, char** argv) {
    const char* socketPath = "/run/wm";
    size_t exitAfter = 0;

    for (int i = 1; i < argc; i++) {
        bool hasArg = i + 1 < argc;
        if (strcmp(argv[i], "--socket") == 0 && hasArg)
            socketPath = argv[++i];
        else if (strcmp(argv[i], "--exit-after") == 0 && hasArg)
            exitAfter = (size_t)atoi(argv[++i]);
        else {
            fprintf(stderr, "bad argument: %s\n", argv[i]);
            return 2;
        }
    }

    WM::Server server;
    server.SetExitAfter(exitAfter);
    if (!server.Listen(socketPath)) {
        fprintf(stderr, "Cannon listen on %s: %s\n", socketPath, strerror(errno));
        return 1;
    }

    std::unique_ptr<WM::VideoBackend> backend;
#ifdef WM_BACKEND_FROSTYOS
    backend = std::make_unique<WM::FrostyOSVideoBackend>();
#else
    fprintf(stderr, "Cannot create video backend\n");
    return 1;
#endif

    if (!backend->Initialise()) {
        fprintf(stderr, "No usable displays\n");
        return 1;
    }

    for (size_t i = 0; i < backend->DisplayCount(); i++) {
        auto& d = backend->GetDisplay(i);
        printf("display %zu: %s %ux%u %s\n", i, d.Name().c_str(), d.CurrentMode().width, d.CurrentMode().height, WM::PixelFormatName(d.CurrentMode().format));
    }

    if (!server.Start(*backend)) {
        fprintf(stderr, "Failed to set up outputs\n");
        return 1;
    }
    server.Run();

    return 0;
}
