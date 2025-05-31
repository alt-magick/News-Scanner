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
        std::cerr << "  " << argv[0] << " <server> <username> <password> read <newsgroup> <article_number> [output_file]\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> search <newsgroup> <term> <count> [output_file]\n";
        std::cerr << "  " << argv[0] << " <server> <username> <password> help\n";
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
            std::cerr << "Usage: " << argv[0] << " <server> <username> <password> read <newsgroup> <article_number> [output_file]\n";
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }
        std::string targetGroup = argv[5];
        std::string articleNumber = argv[6];
        std::string outputFile = (argc > 7) ? argv[7] : "";

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

        std::string line;
        while (true) {
            line = receiveLine(sock);
            if (line == ".") break;

            if (outFile.is_open()) {
                outFile << line << "\n";
            }
            else {
                std::cout << line << "\n";
            }
        }

        if (outFile.is_open()) {
            outFile.close();
            std::cout << "Article saved to " << outputFile << "\n";
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
        int maxResults = std::stoi(argv[7]);
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

        sendCommand(sock, "XOVER 1-" + std::to_string(INT32_MAX)); // Request overview of all articles
        std::string xoverResponse = receiveLine(sock);

        if (xoverResponse.substr(0, 3) != "224") {
            std::cerr << "XOVER command failed.\n";
            sendCommand(sock, "QUIT");
            receiveLine(sock);
            closesocket(sock);
#if defined(_WIN32)
            WSACleanup();
#endif
            return 1;
        }

        std::vector<std::string> lines;
        receiveMultiline(sock, lines);

        std::vector<std::pair<int, std::string>> found; // article number, subject

        for (const auto& line : lines) {
            // Overview format: article-number<TAB>subject<TAB>from<TAB>date<TAB>message-id<TAB>references<TAB>bytes<TAB>lines
            std::istringstream iss(line);
            std::string artNumStr, subject, from, date, msgid, refs, bytes, linesStr;
            if (!std::getline(iss, artNumStr, '\t')) continue;
            if (!std::getline(iss, subject, '\t')) continue;
            if (!std::getline(iss, from, '\t')) continue;
            if (!std::getline(iss, date, '\t')) continue;
            if (!std::getline(iss, msgid, '\t')) continue;
            if (!std::getline(iss, refs, '\t')) continue;
            if (!std::getline(iss, bytes, '\t')) continue;
            if (!std::getline(iss, linesStr, '\t')) continue;

            std::string lowerSubject = subject;
            std::string lowerTerm = searchTerm;
            std::transform(lowerSubject.begin(), lowerSubject.end(), lowerSubject.begin(), ::tolower);
            std::transform(lowerTerm.begin(), lowerTerm.end(), lowerTerm.begin(), ::tolower);

            if (lowerSubject.find(lowerTerm) != std::string::npos) {
                found.emplace_back(std::stoi(artNumStr), subject);
                if ((int)found.size() >= maxResults) break;
            }
        }

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

        for (const auto& item : found) {
            std::string outputLine = "#" + std::to_string(item.first) + ": " + item.second;
            if (outFile.is_open()) {
                outFile << outputLine << "\n";
            }
            else {
                std::cout << outputLine << "\n";
            }
        }

        if (outFile.is_open()) {
            outFile.close();
            std::cout << "Search results saved to " << outputFile << "\n";
        }

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else if (command == "clear") {
        StateMap state = loadState(STATE_FILE);
        if (argc > 5) {
            std::string groupToClear = argv[5];
            auto it = state.find(groupToClear);
            if (it != state.end()) {
                state.erase(it);
                std::cout << "Cleared state for group: " << groupToClear << "\n";
            }
            else {
                std::cout << "No stored state found for group: " << groupToClear << "\n";
            }
        }
        else {
            state.clear();
            std::cout << "Cleared all stored state.\n";
        }
        saveState(state, STATE_FILE);

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else if (command == "help") {
        std::cout << "Available commands:\n";
        std::cout << "  clear [group]                     : Clear stored state (all or specific group)\n";
        std::cout << "  list <newsgroup> [number]        : List recent articles in a newsgroup\n";
        std::cout << "  read <newsgroup> <article> [file]: Read a specific article, optionally save to file\n";
        std::cout << "  search <newsgroup> <term> <count> [file]: Search articles by term, optionally save results\n";
        std::cout << "  help                             : Show this help message\n";
        std::cout << "\nUsage examples:\n";
        std::cout << "  " << argv[0] << " <server> <username> <password> clear\n";
        std::cout << "  " << argv[0] << " <server> <username> <password> list comp.lang.c++ 10\n";
        std::cout << "  " << argv[0] << " <server> <username> <password> read comp.lang.c++ 12345 article.txt\n";
        std::cout << "  " << argv[0] << " <server> <username> <password> search comp.lang.c++ threading 5\n";
        std::cout << std::endl;

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 0;
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        std::cerr << "Use 'help' command to see available options.\n";

        sendCommand(sock, "QUIT");
        receiveLine(sock);
        closesocket(sock);
#if defined(_WIN32)
        WSACleanup();
#endif
        return 1;
    }

    // Should not reach here
    sendCommand(sock, "QUIT");
    receiveLine(sock);
    closesocket(sock);
#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
