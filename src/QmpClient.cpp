#include "QmpClient.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace wvm {

namespace {

constexpr std::size_t maximum_message_size = 1024 * 1024;

bool response_has_id(const std::string& response, const std::string& id) {
    const std::size_t key = response.find("\"id\"");
    if (key == std::string::npos) {
        return false;
    }

    const std::size_t colon = response.find(':', key + 4);
    if (colon == std::string::npos) {
        return false;
    }

    const std::size_t opening_quote = response.find('"', colon + 1);
    if (opening_quote == std::string::npos) {
        return false;
    }

    const std::size_t closing_quote = response.find('"', opening_quote + 1);
    return closing_quote != std::string::npos
        && response.substr(opening_quote + 1, closing_quote - opening_quote - 1) == id;
}

bool valid_command_name(const std::string& command) {
    return !command.empty() && std::all_of(
        command.begin(),
        command.end(),
        [](const unsigned char character) {
            return std::isalnum(character) != 0
                || character == '-' || character == '_';
        }
    );
}

}

QmpClient::~QmpClient() {
    if (socket_ >= 0) {
        close(socket_);
    }
}

bool validate_qmp_socket_path(
    const std::filesystem::path& socket_path,
    std::string& error
) {
    sockaddr_un address{};
    if (socket_path.string().size() >= sizeof(address.sun_path)) {
        error = "QMP socket path is too long: " + socket_path.string();
        return false;
    }

    error.clear();
    return true;
}

bool QmpClient::connect(
    const std::filesystem::path& socket_path,
    std::string& error
) {
    if (socket_ >= 0) {
        error = "QMP client is already connected.";
        return false;
    }

    const std::string path = socket_path.string();
    sockaddr_un address{};
    if (!validate_qmp_socket_path(socket_path, error)) {
        return false;
    }

    socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_ < 0) {
        error = "Failed to create QMP socket: " + std::string(std::strerror(errno));
        return false;
    }

    timeval timeout{};
    timeout.tv_sec = 2;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0
        || setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        error = "Failed to configure QMP timeout: " + std::string(std::strerror(errno));
        close(socket_);
        socket_ = -1;
        return false;
    }

    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

    if (::connect(
            socket_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)
        ) != 0) {
        error = "Failed to connect to QMP socket: " + std::string(std::strerror(errno));
        close(socket_);
        socket_ = -1;
        return false;
    }

    return negotiate(error);
}

bool QmpClient::negotiate(std::string& error) {
    std::string greeting;
    if (!read_message(greeting, error)) {
        return false;
    }
    if (greeting.find("\"QMP\"") == std::string::npos) {
        error = "Invalid QMP greeting: " + greeting;
        return false;
    }

    if (!send_message(
            "{\"execute\":\"qmp_capabilities\",\"id\":\"wvm-capabilities\"}",
            error
        )) {
        return false;
    }

    std::string response;
    return read_response("wvm-capabilities", response, error);
}

bool QmpClient::execute(
    const std::string& command,
    std::string& response,
    std::string& error
) {
    if (socket_ < 0) {
        error = "QMP client is not connected.";
        return false;
    }
    if (!valid_command_name(command)) {
        error = "Invalid QMP command name.";
        return false;
    }

    const std::string request = "{\"execute\":\"" + command
        + "\",\"id\":\"wvm\"}";
    if (!send_message(request, error)) {
        return false;
    }

    return read_response("wvm", response, error);
}

bool QmpClient::send_message(const std::string& message, std::string& error) {
    const std::string wire_message = message + "\r\n";
    std::size_t sent = 0;

    while (sent < wire_message.size()) {
        const ssize_t result = send(
            socket_,
            wire_message.data() + sent,
            wire_message.size() - sent,
            MSG_NOSIGNAL
        );
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = "Failed to send QMP command: " + std::string(std::strerror(errno));
            return false;
        }
        if (result == 0) {
            error = "QMP connection closed while sending a command.";
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool QmpClient::read_message(std::string& message, std::string& error) {
    while (true) {
        const std::size_t delimiter = pending_input_.find("\r\n");
        if (delimiter != std::string::npos) {
            message = pending_input_.substr(0, delimiter);
            pending_input_.erase(0, delimiter + 2);
            return true;
        }

        char buffer[4096];
        const ssize_t received = recv(socket_, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = "Failed to read QMP response: " + std::string(std::strerror(errno));
            return false;
        }
        if (received == 0) {
            error = "QMP connection closed unexpectedly.";
            return false;
        }

        pending_input_.append(buffer, static_cast<std::size_t>(received));
        if (pending_input_.size() > maximum_message_size) {
            error = "QMP response exceeded the 1 MiB safety limit.";
            return false;
        }
    }
}

bool QmpClient::read_response(
    const std::string& id,
    std::string& response,
    std::string& error
) {
    for (std::size_t messages = 0; messages < 1024; ++messages) {
        if (!read_message(response, error)) {
            return false;
        }
        if (!response_has_id(response, id)) {
            continue;
        }

        if (response.find("\"error\"") != std::string::npos) {
            error = "QMP command failed: " + response;
            return false;
        }
        if (response.find("\"return\"") == std::string::npos) {
            error = "Invalid QMP command response: " + response;
            return false;
        }
        return true;
    }

    error = "Too many asynchronous QMP messages without a command response.";
    return false;
}

}
