#pragma once

#include <filesystem>
#include <string>

namespace wvm {

bool validate_qmp_socket_path(
    const std::filesystem::path& socket_path,
    std::string& error
);

class QmpClient {
public:
    QmpClient() = default;
    ~QmpClient();

    QmpClient(const QmpClient&) = delete;
    QmpClient& operator=(const QmpClient&) = delete;

    bool connect(const std::filesystem::path& socket_path, std::string& error);
    bool execute(
        const std::string& command,
        std::string& response,
        std::string& error
    );

private:
    bool send_message(const std::string& message, std::string& error);
    bool read_message(std::string& message, std::string& error);
    bool read_response(
        const std::string& id,
        std::string& response,
        std::string& error
    );
    bool negotiate(std::string& error);

    int socket_ = -1;
    std::string pending_input_;
};

}
