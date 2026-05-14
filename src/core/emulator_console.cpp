#include "emulator_console.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using CoreDeckSocket = SOCKET;
static constexpr CoreDeckSocket COREDECK_INVALID_SOCKET = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
using CoreDeckSocket = int;
static constexpr CoreDeckSocket COREDECK_INVALID_SOCKET = -1;
#endif

namespace CoreDeck::EmulatorConsole {
    namespace {
#if defined(_WIN32)
        struct WsaInit {
            WsaInit() {
                WSADATA d;
                WSAStartup(MAKEWORD(2, 2), &d);
            }

            ~WsaInit() {
                WSACleanup();
            }
        };

        void EnsureWsa() {
            static WsaInit init;
        }

        void CloseSock(const socket_t s) {
            closesocket(s);
        }

        void SetNonBlocking(const socket_t s) {
            u_long m = 1;
            ioctlsocket(s, FIONBIO, &m);
        }

        int LastErr() {
            return WSAGetLastError();
        }

        bool ErrIsWouldBlock(const int e) {
            return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
        }
#else
        void EnsureWsa() {
        }

        void CloseSock(const CoreDeckSocket s) {
            close(s);
        }

        void SetNonBlocking(const CoreDeckSocket s) {
            const int f = fcntl(s, F_GETFL, 0);
            fcntl(s, F_SETFL, f | O_NONBLOCK);
        }

        int LastErr() {
            return errno;
        }

        bool ErrIsWouldBlock(const int e) {
            return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS;
        }
#endif

        std::string ReadAuthToken() {
            const char *home =
#ifdef _WIN32
                std::getenv("USERPROFILE");
#else
                std::getenv("HOME"); // NOLINT(concurrency-mt-unsafe)
#endif
            if (!home) {
                return "";
            }
            const std::filesystem::path p = std::filesystem::path(home) / ".emulator_console_auth_token";
            std::ifstream f(p);
            if (!f) {
                return "";
            }
            std::stringstream ss;
            ss << f.rdbuf();
            std::string token = ss.str();
            while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' ' || token.back() == '\t')) {
                token.pop_back();
            }
            return token;
        }

        bool WaitWritable(const CoreDeckSocket s, const int timeoutMs) {
            fd_set wf;
            FD_ZERO(&wf);
            FD_SET(s, &wf);
            timeval tv{.tv_sec = timeoutMs / 1000, .tv_usec = (timeoutMs % 1000) * 1000};
            return select(static_cast<int>(s) + 1, nullptr, &wf, nullptr, &tv) > 0;
        }

        bool ConnectLocalhost(const CoreDeckSocket s, const int port, const int timeoutMs) {
            SetNonBlocking(s);

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(static_cast<u_short>(port));

            const int rc = connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
            if (rc != 0 && !ErrIsWouldBlock(LastErr())) {
                return false;
            }
            if (rc != 0 && !WaitWritable(s, timeoutMs)) {
                return false;
            }

            int error = 0;
#if defined(_WIN32)
            int len = sizeof(error);
#else
            socklen_t len = sizeof(error);
#endif
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &len) != 0) {
                return false;
            }
            return error == 0;
        }

        bool SendAll(const CoreDeckSocket s, const std::string &payload, const int timeoutMs) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            size_t sent = 0;
            while (sent < payload.size()) {
#if defined(_WIN32)
                const int n = send(s, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
#else
                const ssize_t n = send(s, payload.data() + sent, payload.size() - sent, 0);
#endif
                if (n > 0) {
                    sent += static_cast<size_t>(n);
                    continue;
                }
                if (n < 0 && !ErrIsWouldBlock(LastErr())) {
                    return false;
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    return false;
                }
                WaitWritable(s, 100);
            }
            return true;
        }
    }

    int FindFreePort(const int startPort, const int endPort) {
        EnsureWsa();
        for (int port = startPort; port <= endPort; port += 2) {
            const CoreDeckSocket s = socket(AF_INET, SOCK_STREAM, 0);
            if (s == COREDECK_INVALID_SOCKET) {
                continue;
            }
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(static_cast<u_short>(port));
            const int rc = bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
            CloseSock(s);
            if (rc == 0) {
                return port;
            }
        }
        return -1;
    }

    bool IsAvailable(const int port, const int timeoutMs) {
        EnsureWsa();
        const CoreDeckSocket s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == COREDECK_INVALID_SOCKET) {
            return false;
        }
        const bool connected = ConnectLocalhost(s, port, timeoutMs);
        CloseSock(s);
        return connected;
    }

    bool SendKill(const int port, const int timeoutMs) {
        EnsureWsa();
        const CoreDeckSocket s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == COREDECK_INVALID_SOCKET) {
            return false;
        }

        if (!ConnectLocalhost(s, port, timeoutMs)) {
            CloseSock(s);
            return false;
        }

        std::string payload;
        if (const std::string token = ReadAuthToken(); !token.empty()) {
            payload = "auth " + token + "\r\n";
        }
        payload += "kill\r\n";

        const bool sent = SendAll(s, payload, timeoutMs);
#if defined(_WIN32)
        shutdown(s, SD_SEND);
#else
        shutdown(s, SHUT_WR);
#endif
        CloseSock(s);
        return sent;
    }
}
