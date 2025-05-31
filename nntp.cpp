#define NOMINMAX
#if defined(_WIN32)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

const int PORT = 119;
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
    if (argc < 4) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> clear [group]\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> list <newsgroup> [number]\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> read <newsgroup> <article_number>\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> search <newsgroup> <term> <count> [output_file]\n";
        return 1;
    }

    std::string serverName = argv[1];
    std::string username = argv[2];
    std::string password = argv[3];

#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }

    struct hostent* host = gethostbyname(serverName.c_str());
    if (!host) {
        std::cerr << "Host resolution failed.\n";
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }

    sockaddr_in server{};
    std::memcpy(&server.sin_addr, host->h_addr, host->h_length);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Connection failed.\n";
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }

    std::cout << receiveLine(sock) << "\n";

    sendCommand(sock, "AUTHINFO USER " + username);
    std::cout << receiveLine(sock) << "\n";

    sendCommand(sock, "AUTHINFO PASS " + password);
    std::cout << receiveLine(sock) << "\n";

    std::string command = (argc > 4) ? argv[4] : "";

    if (command.empty()) {
        // Default: list groups and detect new articles or clear state
        sendCommand(sock, "LIST");
        std::string listResponse = receiveLine(sock);
        std::cout << listResponse << "\n";

        std::vector<std::string> groups;
        receiveMultiline(sock, groups);

        StateMap previousState = loadState(STATE_FILE);
        StateMap newState;

        for (const auto& entry : groups) {
            std::istringstream iss(entry);
            std::string group;
            int last = 0, first = 0;
            if (!(iss >> group >> last >> first)) continue;

            int lastSeen = previousState.count(group) ? previousState[group] : first - 1;

            if (last > lastSeen) {
                std::cout << group << "\n";
            }

            newState[group] = lastSeen;
        }

        saveState(newState, STATE_FILE);

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }

    else if (command == "list") {
        if (argc < 6) {
            std::cerr << "Usage: " << argv[0] << " <server> <username> <password> list <newsgroup> [number]\n";
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }
        std::string targetGroup = argv[5];
        int maxArticles = 1;
        if (argc > 6) {
            maxArticles = std::stoi(argv[6]);
        }

        sendCommand(sock, "GROUP " + targetGroup);
        std::string groupResponse = receiveLine(sock);
        std::cout << "GROUP response: " << groupResponse << "\n";

        std::istringstream iss(groupResponse);
        std::string status;
        int estimatedCount = 0, firstArticle = 0, lastArticle = 0;
        std::string groupName;

        if (iss >> status >> estimatedCount >> firstArticle >> lastArticle >> groupName && status == "211") {
            std::cout << "Showing latest " << maxArticles << " article(s):\n";
            int start = std::max(lastArticle - maxArticles + 1, firstArticle);
            for (int i = lastArticle; i >= start; --i) {
                sendCommand(sock, "HEAD " + std::to_string(i));
                std::string headResponse = receiveLine(sock);

                std::vector<std::string> headers;
                receiveMultiline(sock, headers);

                std::string subject = "(no subject)";
                std::string from = "(no author)";

                for (const auto& line : headers) {
                    if (line.find("Subject: ") == 0) {
                        subject = line.substr(9);
                    }
                    else if (line.find("From: ") == 0) {
                        from = line.substr(6);
                    }
                }

                std::cout << "#" << i << ": " << subject << " | Author: " << from << "\n";
            }
        }
        else {
            std::cerr << "Failed to enter newsgroup or parse GROUP response.\n";
        }

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else if (command == "read") {
        if (argc < 7) {
            std::cerr << "Usage: " << argv[0] << " <server> <username> <password> read <newsgroup> <article_number>\n";
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }
        std::string targetGroup = argv[5];
        std::string articleNumber = argv[6];

        sendCommand(sock, "GROUP " + targetGroup);
        std::string groupResponse = receiveLine(sock);

        if (groupResponse.substr(0, 3) != "211") {
            std::cerr << "Failed to enter newsgroup.\n";
            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }

        sendCommand(sock, "ARTICLE " + articleNumber);
        std::string articleResponse = receiveLine(sock);

        if (articleResponse.substr(0, 3) != "220") {
            std::cerr << "Failed to get article.\n";
            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }

        std::string line;
        while (true) {
            line = receiveLine(sock);
            if (line == ".") break;
            std::cout << line << "\n";
        }

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else if (command == "search") {
        if (argc < 8) {
            std::cerr << "Usage: " << argv[0] << " <server> <username> <password> search <newsgroup> <term> <count> [output_file]\n";
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }
        std::string targetGroup = argv[5];
        std::string searchTerm = argv[6];
        int maxCount = std::stoi(argv[7]);
        std::string outputFile = (argc > 8) ? argv[8] : "";

        sendCommand(sock, "GROUP " + targetGroup);
        std::string groupResponse = receiveLine(sock);

        if (groupResponse.substr(0, 3) != "211") {
            std::cerr << "Failed to enter newsgroup.\n";
            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }

        sendCommand(sock, "XOVER");
        std::string xoverResponse = receiveLine(sock);

        if (xoverResponse.substr(0, 3) != "224") {
            std::cerr << "Failed to get overview data.\n";
            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }

        std::vector<std::string> overviewLines;
        receiveMultiline(sock, overviewLines);

        int foundCount = 0;
        std::ofstream outFile;
        if (!outputFile.empty()) {
            outFile.open(outputFile);
            if (!outFile) {
                std::cerr << "Failed to open output file.\n";
                sendCommand(sock, "QUIT");
                receiveLine(sock);
                closesocket(sock);
#if defined(_WIN32)
                WSACleanup();
#endif
                return 1;
            }
        }

        for (const auto& line : overviewLines) {
            if (line.find(searchTerm) != std::string::npos) {
                std::cout << line << "\n";
                if (outFile.is_open()) outFile << line << "\n";
                if (++foundCount >= maxCount) break;
            }
        }

        if (outFile.is_open()) outFile.close();

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else if (command == "clear") {
        if (argc == 5) {
            // No group provided — behave like clear
            sendCommand(sock, "LIST");
            std::string listResponse = receiveLine(sock);
            std::cout << listResponse << "\n";

            if (listResponse.size() < 3 || listResponse.substr(0, 3) != "215") {
                std::cerr << "Failed to get newsgroup list.\n";
                sendCommand(sock, "QUIT");
                receiveLine(sock);
                closesocket(sock);
#if defined(_WIN32)
                WSACleanup();
#endif
                return 1;
            }

            std::vector<std::string> groups;
            receiveMultiline(sock, groups);

            StateMap newState;
            for (const auto& entry : groups) {
                std::istringstream iss(entry);
                std::string group;
                int last = 0, first = 0;
                if (!(iss >> group >> last >> first)) continue;
                newState[group] = last;
            }

            saveState(newState, STATE_FILE);
            std::cout << "State updated for all groups in " << STATE_FILE << "\n";

            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 0;
        }
        else if (argc >= 6) {
            // Group provided — update only that group
            std::string group = argv[5];
            sendCommand(sock, "GROUP " + group);
            std::string groupResponse = receiveLine(sock);

            std::istringstream iss(groupResponse);
            std::string status;
            int count = 0, firstArticle = 0, lastArticle = 0;
            std::string groupName;

            if (!(iss >> status >> count >> firstArticle >> lastArticle >> groupName) || status != "211") {
                std::cerr << "Failed to get group info.\n";
            }
            else {
                StateMap state = loadState(STATE_FILE);
                state[group] = lastArticle;
                saveState(state, STATE_FILE);
                std::cout << "Updated state for " << group << " to last article " << lastArticle << "\n";
            }

            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 0;
        }
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }
}
