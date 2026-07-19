#include "ProcessRunner.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace wvm {

int run_process(const Command& command) {
    if (command.empty()) {
        std::cerr << "Cannot run an empty command.\n";
        return 127;
    }

    std::vector<char*> arguments;
    arguments.reserve(command.size() + 1);

    for (const auto& argument : command) {
        arguments.push_back(const_cast<char*>(argument.c_str()));
    }
    arguments.push_back(nullptr);

    pid_t pid = 0;
    const int spawn_error = posix_spawnp(
        &pid,
        command.front().c_str(),
        nullptr,
        nullptr,
        arguments.data(),
        environ
    );

    if (spawn_error != 0) {
        std::cerr << "Failed to start " << command.front() << ": "
                  << std::strerror(spawn_error) << "\n";
        return spawn_error == ENOENT ? 127 : 126;
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        std::cerr << "Failed to wait for " << command.front() << ": "
                  << std::strerror(errno) << "\n";
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

}
