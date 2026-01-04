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

// =============================================================
// НАЛАШТУВАННЯ
// =============================================================

// Шлях до мого компілятора (треба перевіряти перед запуском на іншому ПК)
const std::string COMPILER_EXE_PATH = "..\\x64\\Debug\\cw_sp2__2025_2026.exe";

// Назви файлів, з якими працюємо
const std::string C_SOURCE_NAME = "discord_code_input.c";
const std::string RESULT_EXE_NAME = "discord_code_input.exe";
const std::string TEMP_FILENAME = "discord_code_input.txt";

// Папка, де лежить бот і куди будуть падати тимчасові файли
const std::string TEMP_FILE_FOLDER = "..\\discord_bot";

// =============================================================
// СТРУКТУРИ
// =============================================================

struct CompilerSession {
    HANDLE hProcess;    // Хендл запущеного процесу
    HANDLE hIn_Wr;      // Сюди ми пишемо (ввід для програми)
    HANDLE hOut_Rd;     // Звідси читаємо (вивід програми)
    std::string tempFilePath;
    bool isActive;

    // Прапорці для "перехоплення" запуску
    bool runRequested;
    std::string runArgs;    // Аргументи (наприклад "10 20"), які юзер ввів разом з 'y'
};

std::map<dpp::snowflake, std::shared_ptr<CompilerSession>> sessions;
std::mutex session_mutex; // Щоб потоки не боролися за доступ до сесій

// =============================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// =============================================================

// Читаємо токен бота з файлу, щоб не світити його в коді
std::string LoadToken(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) exit(1);
    std::string token;
    std::getline(file, token);
    token.erase(token.find_last_not_of(" \n\r\t") + 1);
    return token;
}

// Прибираємо форматування коду з Діскорда (оці потрійні лапки ```)
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

// Вирізаємо ANSI-коди кольорів, бо в Діскорді вони виглядають як сміття
std::string StripAnsiCodes(const std::string& input) {
    std::regex ansi_regex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    return std::regex_replace(input, ansi_regex, "");
}

// =============================================================
// ЛОГІКА: КОМПІЛЯЦІЯ C -> EXE І ЗАПУСК
// =============================================================

bool LaunchGeneratedExe(std::shared_ptr<CompilerSession> session, dpp::cluster& bot, dpp::snowflake channel_id) {
    std::string cPath = TEMP_FILE_FOLDER + "\\" + C_SOURCE_NAME;
    std::string exePath = TEMP_FILE_FOLDER + "\\" + RESULT_EXE_NAME;

    // 1. Перевіряємо, чи взагалі створився .c файл
    if (GetFileAttributesA(cPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        bot.message_create(dpp::message(channel_id, "**Error:** Generated file `" + C_SOURCE_NAME + "` not found!"));
        return false;
    }

    bot.message_create(dpp::message(channel_id, "**Preparation...**"));

    // 2. Компілюємо через GCC (має бути встановлений в системі)
    std::string compileCmd = "gcc \"" + cPath + "\" -o \"" + exePath + "\"";
    int compileResult = system(compileCmd.c_str());

    if (compileResult != 0) {
        bot.message_create(dpp::message(channel_id, "**Compilation Failed!** Check if GCC is installed."));
        return false;
    }

    // 3. Налаштовуємо пайпи (труби) для перехоплення вводу/виводу
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE; // Дозволяємо спадкувати хендли
    sa.lpSecurityDescriptor = NULL;

    HANDLE hNewOut_Rd, hNewOut_Wr, hNewIn_Rd, hNewIn_Wr;
    CreatePipe(&hNewOut_Rd, &hNewOut_Wr, &sa, 0); // Труба для виводу
    SetHandleInformation(hNewOut_Rd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hNewIn_Rd, &hNewIn_Wr, &sa, 0);   // Труба для вводу
    SetHandleInformation(hNewIn_Wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = hNewOut_Wr;  // Перенаправляємо помилки в нашу трубу
    si.hStdOutput = hNewOut_Wr; // Перенаправляємо вивід в нашу трубу
    si.hStdInput = hNewIn_Rd;   // Перенаправляємо ввід
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    std::string dir = TEMP_FILE_FOLDER;

    // Запускаємо скомпільований EXE
    BOOL bSuccess = CreateProcessA(NULL, (LPSTR)exePath.c_str(), NULL, NULL, TRUE, 0, NULL, dir.c_str(), &si, &pi);

    // Закриваємо кінці труб, які ми віддали дитині (щоб не було дедлоків)
    CloseHandle(hNewOut_Wr);
    CloseHandle(hNewIn_Rd);

    if (!bSuccess) {
        bot.message_create(dpp::message(channel_id, "Failed to launch process."));
        return false;
    }

    // Закриваємо хендли від СТАРОГО процесу (компілятора)
    CloseHandle(session->hProcess);
    CloseHandle(session->hOut_Rd);
    CloseHandle(session->hIn_Wr);

    // Оновлюємо сесію новими хендлами
    session->hProcess = pi.hProcess;
    session->hOut_Rd = hNewOut_Rd;
    session->hIn_Wr = hNewIn_Wr;
    session->isActive = true;
    session->runRequested = false;

    CloseHandle(pi.hThread);

    // Якщо юзер вже передав аргументи (наприклад "y 10 20"), відразу пхаємо їх в програму
    if (!session->runArgs.empty()) {
        DWORD dwWritten;
        WriteFile(session->hIn_Wr, session->runArgs.c_str(), session->runArgs.size(), &dwWritten, NULL);
        FlushFileBuffers(session->hIn_Wr); // Примусово проштовхуємо дані
    }

    return true;
}

// =============================================================
// МОНІТОРИНГ ПРОЦЕСУ
// =============================================================

void MonitorSession(dpp::cluster& bot, dpp::snowflake channel_id, std::shared_ptr<CompilerSession> session) {
    CHAR chBuf[4096];
    DWORD dwRead;
    const std::string HIDDEN_PROMPT = "Input filename not setted";
    const std::string PRESS_ENTER_TRIGGER = "Press Enter:";
    const std::string RUN_ACTION_TRIGGER = "Enter 'y' to run program action";

    // Фрази меню, які ми тупо вирізаємо, щоб не смітити в чат
    std::vector<std::string> blocked_phrases = {
        "No command line arguments are entered, so you are working in interactive mode.",
        "Enter 'y' to rerun compiler (to pass action enter other key):",
        "Go to step-by-step interactive mode('y') or start instant processing by default('n' or other key):",
        "View input file('y') or pass('n' or other key):",
        "View lexems/ast/c/assembly/native/obj/exe('y') or pass('n' or other key):",
        "Enter 'y' to run program action"
    };

    int retriesAfterDeath = 10;

    while (true) {
        // Перевіряємо, чи процес живий
        bool isAlive = false;
        if (session->isActive) {
            if (WaitForSingleObject(session->hProcess, 0) == WAIT_TIMEOUT) isAlive = true;
            else session->isActive = false;
        }

        // Перевіряємо, чи є дані в трубі
        DWORD dwAvail = 0;
        bool hasData = PeekNamedPipe(session->hOut_Rd, NULL, 0, NULL, &dwAvail, NULL) && (dwAvail > 0);

        // Якщо процес помер і даних немає
        if (!isAlive && !hasData) {
            // Якщо була команда на запуск (через 'y'), запускаємо EXE замість компілятора
            if (session->runRequested) {
                if (LaunchGeneratedExe(session, bot, channel_id)) {
                    retriesAfterDeath = 50; // Даємо більше часу новій програмі
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                else {
                    break;
                }
            }
            // Даємо трохи часу "дочитати" залишки
            if (retriesAfterDeath > 0) {
                retriesAfterDeath--;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else {
                break;
            }
        }

        if (!hasData) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        retriesAfterDeath = 10; // Скидаємо таймер, якщо пішли дані

        if (ReadFile(session->hOut_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead != 0) {
            chBuf[dwRead] = 0;
            std::string chunk = chBuf;
            std::string clean = StripAnsiCodes(chunk);

            // --- ЛОГІКА ОБРОБКИ ТЕКСТУ ---

            // Кидаємо підказку, якщо програма готова до запуску
            if (clean.find(RUN_ACTION_TRIGGER) != std::string::npos) {
                std::thread([&bot, channel_id]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    bot.message_create(dpp::message(channel_id, "Input \"y\" and variable values (e.g. y 10 20) to run the program"));
                    }).detach();
            }

            // Автоматично тиснемо Enter, якщо програма просить (щоб не висіло)
            if (clean.find(PRESS_ENTER_TRIGGER) != std::string::npos) {
                DWORD dwWritten;
                std::string newline = "\n";
                WriteFile(session->hIn_Wr, newline.c_str(), newline.size(), &dwWritten, NULL);
                FlushFileBuffers(session->hIn_Wr);
                // Вирізаємо це повідомлення
                size_t pos;
                while ((pos = clean.find(PRESS_ENTER_TRIGGER)) != std::string::npos) {
                    clean.erase(pos, PRESS_ENTER_TRIGGER.length());
                }
            }

            // Фільтруємо всі заборонені фрази
            for (const auto& phrase : blocked_phrases) {
                size_t pos;
                while ((pos = clean.find(phrase)) != std::string::npos) {
                    clean.erase(pos, phrase.length());
                }
            }

            // Фільтруємо запит імені файлу
            size_t pos = clean.find(HIDDEN_PROMPT);
            if (pos != std::string::npos) {
                size_t end = clean.find_first_of(":\n", pos);
                if (end != std::string::npos) clean.erase(pos, end - pos + 2);
                else clean.erase(pos);
            }

            // === АГРЕСИВНА ЧИСТКА (REGEX) ===

            // 1. Видаляємо ВСІ пробіли і ентери на початку повідомлення
            clean = std::regex_replace(clean, std::regex("^\\s+"), "");

            // 2. Видаляємо пробіли в кінці
            clean = std::regex_replace(clean, std::regex("\\s+$"), "");

            // 3. Замінюємо купу ентерів (3+) на два, щоб було акуратно
            clean = std::regex_replace(clean, std::regex("\\n{3,}"), "\n\n");

            // Відправляємо тільки якщо залишився текст
            if (!clean.empty()) {
                if (clean.length() > 1900) {
                    for (size_t i = 0; i < clean.length(); i += 1900) {
                        bot.message_create(dpp::message(channel_id, "```\n" + clean.substr(i, 1900) + "\n```"));
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                }
                else {
                    bot.message_create(dpp::message(channel_id, "```\n" + clean + "\n```"));
                }
            }
        }
    }

    bot.message_create(dpp::message(channel_id, "**Session closed.**"));

    // Закриваємо хендли та чистимо пам'ять
    if (session->hProcess) CloseHandle(session->hProcess);
    if (session->hOut_Rd) CloseHandle(session->hOut_Rd);
    if (session->hIn_Wr) CloseHandle(session->hIn_Wr);

    std::remove(session->tempFilePath.c_str());
    std::lock_guard<std::mutex> lock(session_mutex);
    sessions.erase(channel_id);
}

// =============================================================
// ЗАПУСК СЕСІЇ (Старт компілятора)
// =============================================================

bool StartCompilerSession(dpp::cluster& bot, dpp::snowflake channel_id, std::string codeContent) {
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        if (sessions.count(channel_id)) {
            bot.message_create(dpp::message(channel_id, "Session is already active."));
            return false;
        }
    }

    // Створюємо файл з кодом
    std::string fullInputPath = TEMP_FILE_FOLDER + "\\" + TEMP_FILENAME;
    std::ofstream tempFile(fullInputPath);
    if (!tempFile.is_open()) return false;
    tempFile << codeContent;
    tempFile.close();

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Створюємо пайпи
    HANDLE hOut_Rd, hOut_Wr, hIn_Rd, hIn_Wr;
    CreatePipe(&hOut_Rd, &hOut_Wr, &sa, 0);
    SetHandleInformation(hOut_Rd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hIn_Rd, &hIn_Wr, &sa, 0);
    SetHandleInformation(hIn_Wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = hOut_Wr;
    si.hStdOutput = hOut_Wr;
    si.hStdInput = hIn_Rd;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // Запускаємо компілятор
    std::string cmdLine = "\"" + COMPILER_EXE_PATH + "\" \"" + TEMP_FILENAME + "\"";
    std::string dir = TEMP_FILE_FOLDER;

    BOOL bSuccess = CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, TRUE, 0, NULL, dir.c_str(), &si, &pi);

    CloseHandle(hOut_Wr);
    CloseHandle(hIn_Rd);

    if (!bSuccess) {
        bot.message_create(dpp::message(channel_id, "Error launching compiler."));
        return false;
    }

    auto session = std::make_shared<CompilerSession>();
    session->hProcess = pi.hProcess;
    session->hIn_Wr = hIn_Wr;
    session->hOut_Rd = hOut_Rd;
    session->tempFilePath = fullInputPath;
    session->isActive = true;
    session->runRequested = false;

    CloseHandle(pi.hThread);

    {
        std::lock_guard<std::mutex> lock(session_mutex);
        sessions[channel_id] = session;
    }

    // Запускаємо потік моніторингу
    std::thread monitorThread(MonitorSession, std::ref(bot), channel_id, session);
    monitorThread.detach();

    // =======================================================
    // АВТОПІЛОТ (Пропуск меню)
    // =======================================================

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Відправляємо назву файлу і купу 'n', щоб пропустити менюшки
    std::string autoPilot = TEMP_FILENAME + "\nn\ny\nn\n\n\n";

    DWORD dwWritten;
    WriteFile(hIn_Wr, autoPilot.c_str(), autoPilot.size(), &dwWritten, NULL);
    FlushFileBuffers(hIn_Wr);

    return true;
}

// =============================================================
// MAIN
// =============================================================

int main() {
    std::string token = LoadToken("token.txt");
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) return;

        // Обробка команди !run
        if (event.msg.content.starts_with("!run")) {
            std::string rawCode = event.msg.content.substr(4);
            std::string cleanCode = CleanCode(rawCode);
            if (cleanCode.empty()) { event.reply("Code required."); return; }

            // Запускаємо мовчки (без повідомлення "Started")
            StartCompilerSession(bot, event.msg.channel_id, cleanCode);
            return;
        }

        std::shared_ptr<CompilerSession> currentSession = nullptr;
        {
            std::lock_guard<std::mutex> lock(session_mutex);
            if (sessions.count(event.msg.channel_id)) currentSession = sessions[event.msg.channel_id];
        }

        // Обробка вводу користувача в активній сесії
        if (currentSession && currentSession->isActive) {
            if (event.msg.content == "stop") {
                TerminateProcess(currentSession->hProcess, 0);
                event.reply("Stopped.");
                return;
            }

            std::string msg = event.msg.content;
            DWORD dwWritten;

            // Якщо юзер пише "y ...", перехоплюємо це
            if ((msg.rfind("y", 0) == 0) || (msg.rfind("Y", 0) == 0)) {
                currentSession->runRequested = true;
                // Зберігаємо аргументи (все, що після 'y')
                if (msg.length() > 2) currentSession->runArgs = msg.substr(2) + "\n";
                else currentSession->runArgs = "";

                // Вводиво в компілятор декілька 'n', щоб він завершився і ми запустили EXE
                std::string killCmd = "n\nn\n\n";
                WriteFile(currentSession->hIn_Wr, killCmd.c_str(), killCmd.size(), &dwWritten, NULL);
            }
            else {
                // Звичайний ввід
                std::string inputToSend = (msg == ".") ? "\n" : msg + "\n";
                WriteFile(currentSession->hIn_Wr, inputToSend.c_str(), inputToSend.size(), &dwWritten, NULL);
            }
        }
        });

    try { bot.start(dpp::st_wait); }
    catch (std::exception& e) { std::cerr << e.what() << std::endl; }
    return 0;
}