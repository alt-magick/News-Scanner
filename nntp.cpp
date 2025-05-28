#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

const std::string SERVER = "news.eternal-september.org";
const int PORT = 119;
const std::string USER = "user";
const std::string PASS = "pass";
const std::string STATE_FILE = "nntp_state.txt";

typedef std::map<std::string, int> StateMap;

void sendCommand(SOCKET sock, const std::string& cmd) {
    std::string full = cmd + "\r\n";
    send(sock, full.c_str(), static_cast<int>(full.length()), 0);
}

std::string receiveLine(SOCKET sock) {
    std::string line;
    char ch;
    while (recv(sock, &ch, 1, 0) == 1) {
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

void receiveMultiline(SOCKET sock, std::vector<std::string>& lines) {
    std::string line;
    while (true) {
        line = receiveLine(sock);
        if (line == ".") break;
        lines.push_back(line);
    }
}

StateMap loadState(const std::string& filename) {
    StateMap state;
    std::ifstream file(filename.c_str());
    std::string group;
    int lastSeen;
    while (file >> group >> lastSeen) {
        state[group] = lastSeen;
    }
    return state;
}

void saveState(const StateMap& state, const std::string& filename) {
    std::ofstream file(filename.c_str());
    for (const auto& it : state) {
        file << it.first << " " << it.second << "\n";
    }
}

int main(int argc, char* argv[]) {
    bool clearMode = (argc > 1 && std::string(argv[1]) == "clear");

    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    std::cout << "\nStarting...\n";

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    struct hostent* host = gethostbyname(SERVER.c_str());
    if (!host) {
        std::cerr << "Host resolution failed.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    memcpy(&server.sin_addr, host->h_addr, host->h_length);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(sock, (SOCKADDR*)&server, sizeof(server)) != 0) {
        std::cerr << "Connection failed.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << receiveLine(sock) << "\n";

    sendCommand(sock, "AUTHINFO USER " + USER);
    std::cout << receiveLine(sock) << "\n";

    sendCommand(sock, "AUTHINFO PASS " + PASS);
    std::cout << receiveLine(sock) << "\n";

    sendCommand(sock, "LIST");
    std::cout << receiveLine(sock) << "\n";

    std::vector<std::string> groups;
    receiveMultiline(sock, groups);

    StateMap previousState = loadState(STATE_FILE);
    StateMap newState;

    if (clearMode) {
        std::cout << "Clearing Articles..\n";
    }

    for (const auto& entry : groups) {
        std::istringstream iss(entry);
        std::string group;
        int last = 0, first = 0;
        if (!(iss >> group >> last >> first)) continue;

        int lastSeen = previousState.count(group) ? previousState[group] : 0;

        if (!clearMode && last > lastSeen) {
            std::cout << "New articles in: " << group << "\n";
        }

        if (clearMode) {
            newState[group] = last;
        }
    }

    sendCommand(sock, "QUIT");
    receiveLine(sock);
    closesocket(sock);
    WSACleanup();

    if (clearMode) {
        saveState(newState, STATE_FILE);
        std::cout << "State updated in " << STATE_FILE << "\n";
    }

    return 0;
}
