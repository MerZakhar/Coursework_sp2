#define _CRT_SECURE_NO_WARNINGS
#include <dpp/dpp.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <thread>
#include <map>
#include <mutex>
#include <memory> 
#include <chrono> 
#include <regex>
#include <tchar.h>

// --- НАЛАШТУВАННЯ ---
const std::string COMPILER_EXE_PATH = "..\\x64\\Debug\\cw_sp2__2025_2026.exe"; // Шлях до вашого компілятора
const std::string RESULT_EXE_NAME = "discord_code_input.exe";
const std::string TEMP_FILENAME = "discord_code_input.txt";
const std::string TEMP_FILE_FOLDER = "..\\discord_bot"; // Папка для тимчасових файлів

struct CompilerSession {
    HANDLE hProcess;      // Процес програми
    HANDLE hIn_Wr;        // Для запису вводу
    HANDLE hOut_Rd;       // Для читання виводу
    HPCON hPC;            // Віртуальна консоль (потрібна для exe)

    std::string tempFilePath;
    bool isActive;
    bool runRequested;    // Чи натиснув юзер 'y' для запуску
    std::string runArgs;  // Аргументи запуску (наприклад "10 20")
};

std::map<dpp::snowflake, std::shared_ptr<CompilerSession>> sessions;
std::mutex session_mutex;

// Читаємо токен з файлу
std::string LoadToken(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) exit(1);
    std::string token;
    std::getline(file, token);
    token.erase(token.find_last_not_of(" \n\r\t") + 1);
    return token;
}

// Прибираємо ``` з повідомлення
std::string CleanCode(std::string raw) {
    size_t firstTick = raw.find("```");
    if (firstTick != std::string::npos) {
        size_t nextLine = raw.find('\n', firstTick);
        if (nextLine != std::string::npos) raw = raw.substr(nextLine + 1);
        size_t lastTick = raw.rfind("```");
        if (lastTick != std::string::npos) raw = raw.substr(0, lastTick);
    }
    return raw;
}

// Чистимо кольорові коди консолі
std::string StripAnsiCodes(const std::string& input) {
    std::regex ansi_regex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    return std::regex_replace(input, ansi_regex, "");
}

// --- ЗАПУСК ГОТОВОГО EXE (ЧЕРЕЗ ВІРТУАЛЬНУ КОНСОЛЬ) ---
bool LaunchGeneratedExeWithConPTY(std::shared_ptr<CompilerSession> session, dpp::cluster& bot, dpp::snowflake channel_id) {
    std::string exePath = TEMP_FILE_FOLDER + "\\" + RESULT_EXE_NAME;

    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        bot.message_create(dpp::message(channel_id, "**Помилка:** exe-файл не знайдено!"));
        return false;
    }

    bot.message_create(dpp::message(channel_id, "**Запускаю програму...**"));

    HANDLE hPipePTYIn, hPipePTYOut;       // Труби для PTY
    HANDLE hPipeParentIn, hPipeParentOut; // Труби для нашого бота
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    // Створюємо труби
    CreatePipe(&hPipePTYIn, &hPipeParentIn, &sa, 0);
    CreatePipe(&hPipeParentOut, &hPipePTYOut, &sa, 0);

    // Створюємо віртуальну консоль (ConPTY)
    HPCON hPC;
    COORD consoleSize = { 80, 25 };
    HRESULT hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, &hPC);

    // Закриваємо зайві кінці труб
    if (hPipePTYIn) CloseHandle(hPipePTYIn);
    if (hPipePTYOut) CloseHandle(hPipePTYOut);

    if (FAILED(hr)) return false;

    // Налаштовуємо атрибути запуску, щоб програма підчепилась до консолі
    STARTUPINFOEXA siEx;
    ZeroMemory(&siEx, sizeof(STARTUPINFOEXA));
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXA);
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
    siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrListSize);
    InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize);
    UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    std::string dir = TEMP_FILE_FOLDER;

    // Запускаємо exe
    BOOL bSuccess = CreateProcessA(NULL, (LPSTR)exePath.c_str(), NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT, NULL, dir.c_str(), &siEx.StartupInfo, &pi);

    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);

    if (!bSuccess) {
        ClosePseudoConsole(hPC);
        CloseHandle(hPipeParentOut);
        CloseHandle(hPipeParentIn);
        return false;
    }

    // Закриваємо старі хендли від компілятора і зберігаємо нові
    if (session->hProcess) CloseHandle(session->hProcess);
    if (session->hOut_Rd) CloseHandle(session->hOut_Rd);
    if (session->hIn_Wr) CloseHandle(session->hIn_Wr);

    session->hProcess = pi.hProcess;
    session->hOut_Rd = hPipeParentOut;
    session->hIn_Wr = hPipeParentIn;
    session->hPC = hPC; // Запам'ятовуємо консоль, щоб потім закрити
    session->isActive = true;
    session->runRequested = false;

    CloseHandle(pi.hThread);

    // Якщо юзер вже ввів аргументи (наприклад "y 10"), відправляємо їх
    if (!session->runArgs.empty()) {
        DWORD dwWritten;
        WriteFile(session->hIn_Wr, session->runArgs.c_str(), session->runArgs.size(), &dwWritten, NULL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return true;
}

// --- СТЕЖИМО ЗА ПРОЦЕСОМ ---
void MonitorSession(dpp::cluster& bot, dpp::snowflake channel_id, std::shared_ptr<CompilerSession> session) {
    CHAR chBuf[4096];
    DWORD dwRead;

    // Фрази меню, які не треба показувати в чаті
    std::vector<std::string> blocked_phrases = {
        "No command line arguments are entered", "Enter 'y' to rerun",
        "Go to step-by-step", "View input file", "View lexems", "Enter 'y' to run program action"
    };

    while (true) {
        bool isAlive = false;
        if (session->isActive) {
            if (WaitForSingleObject(session->hProcess, 0) == WAIT_TIMEOUT) isAlive = true;
            else session->isActive = false;
        }

        DWORD dwAvail = 0;
        bool hasData = PeekNamedPipe(session->hOut_Rd, NULL, 0, NULL, &dwAvail, NULL) && (dwAvail > 0);

        // Якщо процес помер і даних немає
        if (!isAlive && !hasData) {
            // Якщо треба запустити exe після компілятора
            if (session->runRequested) {
                if (LaunchGeneratedExeWithConPTY(session, bot, channel_id)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
            }
            break;
        }

        if (!hasData) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Читаємо вивід
        if (ReadFile(session->hOut_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead != 0) {
            chBuf[dwRead] = 0;
            std::string clean = StripAnsiCodes(chBuf);

            // Підказка для юзера
            if (clean.find("Enter 'y' to run program action") != std::string::npos) {
                std::thread([&bot, channel_id]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    bot.message_create(dpp::message(channel_id, "Введіть \"y\" (та аргументи) для запуску."));
                    }).detach();
            }

            // Чистимо сміття з меню
            for (const auto& phrase : blocked_phrases) {
                size_t pos;
                while ((pos = clean.find(phrase)) != std::string::npos) clean.erase(pos, phrase.length());
            }

            clean = std::regex_replace(clean, std::regex("^\\s+"), ""); // Прибрати пробіли спочатку
            clean = std::regex_replace(clean, std::regex("\\n{3,}"), "\n\n"); // Прибрати купу ентерів

            if (!clean.empty()) {
                if (clean.length() > 1900) clean = clean.substr(0, 1900);
                bot.message_create(dpp::message(channel_id, "```\n" + clean + "\n```"));
            }
        }
    }

    bot.message_create(dpp::message(channel_id, "**Сесія завершена.**"));

    if (session->hProcess) CloseHandle(session->hProcess);
    if (session->hOut_Rd) CloseHandle(session->hOut_Rd);
    if (session->hIn_Wr) CloseHandle(session->hIn_Wr);
    if (session->hPC) ClosePseudoConsole(session->hPC);

    std::remove(session->tempFilePath.c_str());
    std::lock_guard<std::mutex> lock(session_mutex);
    sessions.erase(channel_id);
}

// --- СТАРТ КОМПІЛЯТОРА (ПЕРШИЙ ЕТАП) ---
bool StartCompilerSession(dpp::cluster& bot, dpp::snowflake channel_id, std::string codeContent) {
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        if (sessions.count(channel_id)) {
            bot.message_create(dpp::message(channel_id, "Вже працюю."));
            return false;
        }
    }

    // Створюємо файл з кодом
    std::string fullInputPath = TEMP_FILE_FOLDER + "\\" + TEMP_FILENAME;
    std::ofstream tempFile(fullInputPath);
    if (!tempFile.is_open()) return false;
    tempFile << codeContent;
    tempFile.close();

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hOut_Rd, hOut_Wr, hIn_Rd, hIn_Wr;
    CreatePipe(&hOut_Rd, &hOut_Wr, &sa, 0); SetHandleInformation(hOut_Rd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hIn_Rd, &hIn_Wr, &sa, 0);   SetHandleInformation(hIn_Wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si; ZeroMemory(&si, sizeof(STARTUPINFO)); si.cb = sizeof(STARTUPINFO);
    si.hStdError = hOut_Wr; si.hStdOutput = hOut_Wr; si.hStdInput = hIn_Rd; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    std::string cmdLine = "\"" + COMPILER_EXE_PATH + "\" \"" + TEMP_FILENAME + "\"";

    // Запускаємо компілятор (тут звичайна консоль, щоб швидко пропустити меню)
    if (!CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, TRUE, 0, NULL, TEMP_FILE_FOLDER.c_str(), &si, &pi)) {
        CloseHandle(hOut_Wr); CloseHandle(hIn_Rd); return false;
    }
    CloseHandle(hOut_Wr); CloseHandle(hIn_Rd);

    auto session = std::make_shared<CompilerSession>();
    session->hProcess = pi.hProcess;
    session->hIn_Wr = hIn_Wr;
    session->hOut_Rd = hOut_Rd;
    session->hPC = NULL;
    session->tempFilePath = fullInputPath;
    session->isActive = true;
    session->runRequested = false;

    CloseHandle(pi.hThread);

    { std::lock_guard<std::mutex> lock(session_mutex); sessions[channel_id] = session; }

    std::thread(MonitorSession, std::ref(bot), channel_id, session).detach();

    // Автоматично пропускаємо меню компілятора
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    std::string autoPilot = TEMP_FILENAME + "\nn\ny\nn\n\n\n";
    DWORD dwWritten; WriteFile(hIn_Wr, autoPilot.c_str(), autoPilot.size(), &dwWritten, NULL);

    return true;
}

int main() {
    std::string token = LoadToken("token.txt");
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) return;

        if (event.msg.content.starts_with("!run")) {
            std::string code = CleanCode(event.msg.content.substr(4));
            if (code.empty()) { event.reply("Code required."); return; }
            StartCompilerSession(bot, event.msg.channel_id, code);
            return;
        }

        std::shared_ptr<CompilerSession> s = nullptr;
        { std::lock_guard<std::mutex> lock(session_mutex); if (sessions.count(event.msg.channel_id)) s = sessions[event.msg.channel_id]; }

        if (s && s->isActive) {
            if (event.msg.content == "stop") {
                TerminateProcess(s->hProcess, 0);
                event.reply("Зупинено.");
                return;
            }

            std::string msg = event.msg.content;
            DWORD dw;
            // Якщо юзер пише "y" або "y 10 20"
            if ((msg.rfind("y", 0) == 0) || (msg.rfind("Y", 0) == 0)) {
                s->runRequested = true;
                if (msg.length() > 2) s->runArgs = msg.substr(2) + "\n";
                else s->runArgs = "";

                // Завершуємо компілятор, щоб перейти до exe
                std::string killCmd = "n\nn\n\n";
                WriteFile(s->hIn_Wr, killCmd.c_str(), killCmd.size(), &dw, NULL);
            }
            else {
                std::string input = (msg == ".") ? "\n" : msg + "\n";
                WriteFile(s->hIn_Wr, input.c_str(), input.size(), &dw, NULL);
            }
        }
        });

    try { bot.start(dpp::st_wait); }
    catch (std::exception& e) { std::cerr << e.what() << std::endl; }
    return 0;
}