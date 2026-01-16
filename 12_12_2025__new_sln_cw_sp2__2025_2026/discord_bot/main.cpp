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
#include <algorithm> 

// =============================================================
// НАЛАШТУВАННЯ
// =============================================================

// Шляхи до файлів та папок. Потрібно перевірити перед запуском.
const std::string COMPILER_EXE_PATH = "..\\x64\\Debug\\cw_sp2__2025_2026.exe";
const std::string C_SOURCE_NAME = "discord_code_input.c";
const std::string RESULT_EXE_NAME = "discord_code_input.exe";
const std::string TEMP_FILENAME = "discord_code_input.txt";
const std::string TEMP_FILE_FOLDER = "..\\discord_bot";

// =============================================================
// СТРУКТУРИ
// =============================================================

// Структура для зберігання інформації про сесію користувача
struct CompilerSession {
    HANDLE hProcess;      // Хендл активного процесу
    HANDLE hIn_Wr;        // Хендл для запису вводу (Input)
    HANDLE hOut_Rd;       // Хендл для читання виводу (Output)
    HPCON hPC;            // Хендл псевдоконсолі (ConPTY)

    std::string tempFilePath; // Шлях до файлу з кодом
    bool isActive;            // Чи активна сесія

    bool runRequested;        // Чи запросив користувач запуск програми
    std::string runArgs;      // Аргументи запуску (ввід користувача)
};

// Мапа сесій та м'ютекс для безпечної роботи з потоками
std::map<dpp::snowflake, std::shared_ptr<CompilerSession>> sessions;
std::mutex session_mutex;

// =============================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// =============================================================

// Зчитування токена бота з текстового файлу
std::string LoadToken(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) exit(1);
    std::string token;
    std::getline(file, token);
    token.erase(token.find_last_not_of(" \n\r\t") + 1);
    return token;
}

// Видалення форматування коду (потрійних лапок) з повідомлення Discord
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

// Очищення тексту від ANSI-кодів та заголовків вікна консолі
std::string StripAnsiCodes(const std::string& input) {
    std::string s = input;
    // 1. Видалення заголовків вікна
    s = std::regex_replace(s, std::regex(R"(\x1B\][0-9]*;.*?\x07)"), "");
    // 2. Видалення кодів кольорів
    s = std::regex_replace(s, std::regex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))"), "");
    return s;
}

// =============================================================
// ЛОГІКА: ЗАПУСК ПРОГРАМИ
// =============================================================

// Запуск скомпільованого exe-файлу через ConPTY
bool LaunchGeneratedExe(std::shared_ptr<CompilerSession> session, dpp::cluster& bot, dpp::snowflake channel_id) {
    std::string exePath = TEMP_FILE_FOLDER + "\\" + RESULT_EXE_NAME;

    // Перевірка, чи існує файл
    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        bot.message_create(dpp::message(channel_id, "**Помилка:** Файл `" + RESULT_EXE_NAME + "` не знайдено!"));
        return false;
    }

    bot.message_create(dpp::message(channel_id, "**Launching program...**"));

    HANDLE hPipePTYIn, hPipePTYOut;
    HANDLE hPipeParentIn, hPipeParentOut;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Створення пайпів (каналів)
    CreatePipe(&hPipePTYIn, &hPipeParentIn, &sa, 0);
    CreatePipe(&hPipeParentOut, &hPipePTYOut, &sa, 0);

    // Створення псевдоконсолі
    HPCON hPC;
    COORD consoleSize = { 80, 25 };
    HRESULT hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, &hPC);

    // Закриваємо зайві хендли
    if (hPipePTYIn) CloseHandle(hPipePTYIn);
    if (hPipePTYOut) CloseHandle(hPipePTYOut);

    if (FAILED(hr)) {
        bot.message_create(dpp::message(channel_id, "Не вдалося створити псевдоконсоль."));
        return false;
    }

    // Налаштування атрибутів запуску
    STARTUPINFOEXA siEx;
    ZeroMemory(&siEx, sizeof(STARTUPINFOEXA));
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXA);

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
    siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrListSize);

    if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize) ||
        !UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL)) {
        bot.message_create(dpp::message(channel_id, "Помилка ініціалізації атрибутів."));
        return false;
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    std::string dir = TEMP_FILE_FOLDER;

    // Запуск процесу
    BOOL bSuccess = CreateProcessA(
        NULL, (LPSTR)exePath.c_str(), NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT,
        NULL, dir.c_str(), &siEx.StartupInfo, &pi
    );

    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);

    if (!bSuccess) {
        bot.message_create(dpp::message(channel_id, "Не вдалося запустити процес."));
        ClosePseudoConsole(hPC);
        CloseHandle(hPipeParentOut);
        CloseHandle(hPipeParentIn);
        return false;
    }

    // Закриття хендлів від старого процесу
    CloseHandle(session->hProcess);
    CloseHandle(session->hOut_Rd);
    CloseHandle(session->hIn_Wr);

    // Оновлення сесії новими даними
    session->hProcess = pi.hProcess;
    session->hOut_Rd = hPipeParentOut;
    session->hIn_Wr = hPipeParentIn;
    session->hPC = hPC;
    session->isActive = true;
    session->runRequested = false;

    CloseHandle(pi.hThread);

    // Передача аргументів, якщо вони є
    if (!session->runArgs.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        DWORD dwWritten;
        WriteFile(session->hIn_Wr, session->runArgs.c_str(), session->runArgs.size(), &dwWritten, NULL);
        FlushFileBuffers(session->hIn_Wr);
    }

    return true;
}

// =============================================================
// МОНІТОРИНГ
// =============================================================

// Функція для відстеження роботи процесу та зчитування виводу
void MonitorSession(dpp::cluster& bot, dpp::snowflake channel_id, std::shared_ptr<CompilerSession> session) {
    const int BUFSIZE = 65536; // Буфер 64 КБ
    char* chBuf = new char[BUFSIZE];
    DWORD dwRead;

    // Рядки для автоматизації та фільтрації
    const std::string HIDDEN_PROMPT = "Input filename not setted";
    const std::string PRESS_ENTER_TRIGGER = "Press Enter:";
    const std::string RUN_ACTION_TRIGGER = "Enter 'y' to run program action";

    std::vector<std::string> blocked_phrases = {
        "No command line arguments are entered",
        ", so you are working in interactive mode.",
        "Enter 'y' to rerun",
        "compiler (to pass action enter other key):",
        "(to pass action process Enter 'n' or others key):",
        "Go to step-by-step",
        "View input file",
        "View lexems",
        "Enter 'y' to run program action"
    };

    int retriesAfterDeath = 50;
    bool isExecutionPhase = false; // Чи запущена вже основна програма
    std::string accumulatedOutput = "";

    while (true) {
        // Перевірка, чи живий процес
        bool isAlive = false;
        if (session->isActive) {
            if (WaitForSingleObject(session->hProcess, 0) == WAIT_TIMEOUT) isAlive = true;
            else session->isActive = false;
        }

        // Цикл читання даних (поки вони є)
        bool readSomething = false;
        while (true) {
            DWORD dwAvail = 0;
            if (!PeekNamedPipe(session->hOut_Rd, NULL, 0, NULL, &dwAvail, NULL)) break;
            if (dwAvail == 0) break;

            if (ReadFile(session->hOut_Rd, chBuf, BUFSIZE - 1, &dwRead, NULL) && dwRead != 0) {
                readSomething = true;
                chBuf[dwRead] = 0;
                std::string chunk = chBuf;

                // Видалення символів \r
                chunk.erase(std::remove(chunk.begin(), chunk.end(), '\r'), chunk.end());
                std::string clean = StripAnsiCodes(chunk);

                // Обробка тригерів
                if (clean.find(RUN_ACTION_TRIGGER) != std::string::npos) {
                    std::thread([&bot, channel_id]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        bot.message_create(dpp::message(channel_id, "Input \"y\" to start. Then enter values one by one (wait for the bot to accept input)."));
                        }).detach();
                }

                if (clean.find(PRESS_ENTER_TRIGGER) != std::string::npos) {
                    DWORD dwWritten;
                    std::string newline = "\n";
                    WriteFile(session->hIn_Wr, newline.c_str(), newline.size(), &dwWritten, NULL);
                    size_t pos;
                    while ((pos = clean.find(PRESS_ENTER_TRIGGER)) != std::string::npos) clean.erase(pos, PRESS_ENTER_TRIGGER.length());
                }

                // Фільтрація тексту
                for (const auto& phrase : blocked_phrases) {
                    size_t pos;
                    while ((pos = clean.find(phrase)) != std::string::npos) clean.erase(pos, phrase.length());
                }

                size_t pos = clean.find(HIDDEN_PROMPT);
                if (pos != std::string::npos) {
                    size_t end = clean.find_first_of(":\n", pos);
                    if (end != std::string::npos) clean.erase(pos, end - pos + 2);
                    else clean.erase(pos);
                }

                clean = std::regex_replace(clean, std::regex("^\\s+"), "");

                // Накопичення або вивід
                if (!clean.empty()) {
                    if (isExecutionPhase) {
                        accumulatedOutput += clean;
                    }
                    else {
                        if (clean.length() > 1900) {
                            for (size_t i = 0; i < clean.length(); i += 1900) {
                                bot.message_create(dpp::message(channel_id, "```\n" + clean.substr(i, 1900) + "\n```"));
                                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                            }
                        }
                        else {
                            bot.message_create(dpp::message(channel_id, "```\n" + clean + "\n```"));
                        }
                    }
                }
            }
            else {
                break;
            }
        }

        // Обробка завершення процесу або переходу станів
        if (!isAlive && !readSomething) {
            if (session->runRequested) {
                if (LaunchGeneratedExe(session, bot, channel_id)) {
                    isExecutionPhase = true;
                    retriesAfterDeath = 50;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                else break;
            }

            if (retriesAfterDeath > 0) {
                retriesAfterDeath--;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Вивід накопиченого результату
    if (!accumulatedOutput.empty()) {
        accumulatedOutput = std::regex_replace(accumulatedOutput, std::regex("\n{2,}"), "\n");

        size_t totalLen = accumulatedOutput.length();
        for (size_t i = 0; i < totalLen; i += 1900) {
            std::string part = accumulatedOutput.substr(i, 1900);
            bot.message_create(dpp::message(channel_id, "```\n" + part + "\n```"));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    bot.message_create(dpp::message(channel_id, "**Session closed.**"));

    // Очищення ресурсів
    if (session->hProcess) CloseHandle(session->hProcess);
    if (session->hOut_Rd) CloseHandle(session->hOut_Rd);
    if (session->hIn_Wr) CloseHandle(session->hIn_Wr);
    if (session->hPC) ClosePseudoConsole(session->hPC);

    delete[] chBuf;
    std::remove(session->tempFilePath.c_str());
    std::lock_guard<std::mutex> lock(session_mutex);
    sessions.erase(channel_id);
}

// =============================================================
// ЗАПУСК (Початковий етап)
// =============================================================

// Ініціалізація сесії: запис коду у файл та запуск компілятора
bool StartCompilerSession(dpp::cluster& bot, dpp::snowflake channel_id, std::string codeContent) {
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        if (sessions.count(channel_id)) {
            bot.message_create(dpp::message(channel_id, "Сесія вже активна."));
            return false;
        }
    }

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
    std::string dir = TEMP_FILE_FOLDER;

    if (!CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, TRUE, 0, NULL, dir.c_str(), &si, &pi)) {
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

    // Автоматичне проходження меню
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    std::string autoPilot = TEMP_FILENAME + "\nn\ny\nn\n\n\n";
    DWORD dwWritten;
    WriteFile(hIn_Wr, autoPilot.c_str(), autoPilot.size(), &dwWritten, NULL);

    return true;
}

// =============================================================
// ГОЛОВНА ФУНКЦІЯ
// =============================================================

int main() {
    std::string token = LoadToken("token.txt");
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) return;

        // Команда запуску (!run)
        if (event.msg.content.starts_with("!run")) {
            std::string code = CleanCode(event.msg.content.substr(4));
            if (code.empty()) { event.reply("Потрібен код для запуску."); return; }
            StartCompilerSession(bot, event.msg.channel_id, code);
            return;
        }

        std::shared_ptr<CompilerSession> currentSession = nullptr;
        { std::lock_guard<std::mutex> lock(session_mutex); if (sessions.count(event.msg.channel_id)) currentSession = sessions[event.msg.channel_id]; }

        // Обробка вводу користувача
        if (currentSession && currentSession->isActive) {
            if (event.msg.content == "stop") {
                TerminateProcess(currentSession->hProcess, 0);
                event.reply("Зупинено.");
                return;
            }

            std::string msg = event.msg.content;
            DWORD dwWritten;

            // Обробка команди підтвердження запуску ('y')
            if ((msg.rfind("y", 0) == 0) || (msg.rfind("Y", 0) == 0)) {
                currentSession->runRequested = true;
                if (msg.length() > 2) currentSession->runArgs = msg.substr(2) + "\r\n";
                else currentSession->runArgs = "";

                std::string killCmd = "n\r\nn\r\n\r\n";
                WriteFile(currentSession->hIn_Wr, killCmd.c_str(), killCmd.size(), &dwWritten, NULL);
            }
            else {
                // Передача звичайного вводу
                std::string inputToSend = (msg == ".") ? "\r\n" : msg + "\r\n";
                WriteFile(currentSession->hIn_Wr, inputToSend.c_str(), inputToSend.size(), &dwWritten, NULL);
            }
        }
        });

    try { bot.start(dpp::st_wait); }
    catch (std::exception& e) { std::cerr << e.what() << std::endl; }
    return 0;
}