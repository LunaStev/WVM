#pragma once

namespace wvm {
    int command_init(int argc, char** argv);
    int command_set(int argc, char** argv);
    int command_install(int argc, char** argv);
    int command_run(int argc, char** argv);
    int command_status(int argc, char** argv);
    int command_stop(int argc, char** argv);
    int command_reboot(int argc, char** argv);
    int command_doctor(int argc, char** argv);
    int command_host_setup(int argc, char** argv);
}
