#include "pch.h"
#include "audio/audio_capture.h"
#include "audio/whisper_engine.h"
#include <fstream>
#include <winhttp.h>

namespace {

struct TranscriptMessage {
  std::string source;
  std::string author;
  std::string original;
  std::string translated;
  ULONGLONG timestamp;
  bool voice;
};

static std::atomic<bool> g_running{false};
static std::thread g_textThread;
static std::thread g_voiceThread;
static std::mutex g_messagesMutex;
static std::deque<TranscriptMessage> g_messages;
static std::mutex g_voiceMutex;
static std::condition_variable g_voiceCv;
static std::deque<std::vector<float>> g_voiceChunks;
static std::atomic<bool> g_audioWanted{false};
static char g_status[256] = "Transcript stopped";
static std::string g_lastLogPath;

static void SetStatus(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_status, sizeof(g_status), fmt, args);
  va_end(args);
}

static std::wstring Utf8ToWide(const std::string &value) {
  if (value.empty())
    return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(),
                                nullptr, 0);
  if (len <= 0)
    return {};
  std::wstring out((size_t)len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(),
                      len);
  return out;
}

static std::string WideToUtf8(const std::wstring &value) {
  if (value.empty())
    return {};
  int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return {};
  std::string out((size_t)len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(),
                      len, nullptr, nullptr);
  return out;
}

static std::string UrlEncode(const std::string &value) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      out.push_back((char)c);
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      out.push_back('%');
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

static const char *TargetLangCode() {
  switch (hooks::transcriptTargetLang) {
  case 1: return "hi";
  case 2: return "ar";
  case 3: return "es";
  case 4: return "pt";
  default: return "en";
  }
}

static const char *TargetLangName() {
  switch (hooks::transcriptTargetLang) {
  case 1: return "Hindi";
  case 2: return "Arabic";
  case 3: return "Spanish";
  case 4: return "Portuguese";
  default: return "English";
  }
}

static std::string GuessSourceLang(const std::string &text,
                                   bool allowAsciiTranslation) {
  std::wstring wide = Utf8ToWide(text);
  bool hasLatinAccent = false;
  for (wchar_t ch : wide) {
    if (ch >= 0x0600 && ch <= 0x06FF)
      return "ar";
    if (ch >= 0x0900 && ch <= 0x097F)
      return "hi";
    if (ch >= 0x0400 && ch <= 0x04FF)
      return "ru";
    if (ch >= 0x3040 && ch <= 0x30FF)
      return "ja";
    if (ch >= 0x4E00 && ch <= 0x9FFF)
      return "zh-CN";
    if (ch >= 0xAC00 && ch <= 0xD7AF)
      return "ko";
    if ((ch >= 0x00C0 && ch <= 0x017F) || ch == 0x00BF || ch == 0x00A1)
      hasLatinAccent = true;
  }
  if (hasLatinAccent)
    return "es";
  if (allowAsciiTranslation)
    return "es";
  return {};
}

static bool LooksForeign(const std::string &text) {
  for (unsigned char c : text) {
    if (c >= 0x80)
      return true;
  }
  return false;
}

static std::string JsonUnescape(std::string value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '\\' || i + 1 >= value.size()) {
      out.push_back(value[i]);
      continue;
    }
    char n = value[++i];
    switch (n) {
    case 'n': out.push_back('\n'); break;
    case 'r': out.push_back('\r'); break;
    case 't': out.push_back('\t'); break;
    case '"': out.push_back('"'); break;
    case '\\': out.push_back('\\'); break;
    case 'u':
      if (i + 4 < value.size()) {
        wchar_t wide[2] = {};
        unsigned code = 0;
        sscanf_s(value.substr(i + 1, 4).c_str(), "%x", &code);
        wide[0] = (wchar_t)code;
        out += WideToUtf8(std::wstring(wide, 1));
        i += 4;
      }
      break;
    default:
      out.push_back(n);
      break;
    }
  }
  return out;
}

static std::string ExtractTranslatedText(const std::string &json) {
  const char *key = "\"translatedText\":\"";
  size_t pos = json.find(key);
  if (pos == std::string::npos)
    return {};
  pos += strlen(key);
  std::string value;
  bool escape = false;
  for (; pos < json.size(); ++pos) {
    char c = json[pos];
    if (!escape && c == '"')
      break;
    if (!escape && c == '\\') {
      escape = true;
      value.push_back(c);
      continue;
    }
    escape = false;
    value.push_back(c);
  }
  return JsonUnescape(value);
}

static std::string TranslateText(const std::string &text,
                                 bool allowAsciiTranslation) {
  if (text.empty())
    return {};

  std::string sourceLang = GuessSourceLang(text, allowAsciiTranslation);
  if (sourceLang.empty())
    return {};
  const char *targetLang = TargetLangCode();
  if (_stricmp(sourceLang.c_str(), targetLang) == 0)
    return {};

  std::wstring path = L"/get?q=" + Utf8ToWide(UrlEncode(text)) +
                      L"&langpair=" + Utf8ToWide(sourceLang) + L"|" +
                      Utf8ToWide(targetLang);

  HINTERNET session =
      WinHttpOpen(L"CS2Overlay/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return {};

  HINTERNET connect =
      WinHttpConnect(session, L"api.mymemory.translated.net",
                     INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!connect) {
    WinHttpCloseHandle(session);
    return {};
  }

  HINTERNET request = WinHttpOpenRequest(
      connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!request) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return {};
  }

  DWORD timeout = 5000;
  WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout,
                   sizeof(timeout));
  WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout,
                   sizeof(timeout));

  std::string response;
  if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(request, nullptr)) {
    DWORD size = 0;
    while (WinHttpQueryDataAvailable(request, &size) && size > 0) {
      std::string buffer(size, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(request, buffer.data(), size, &read) || read == 0)
        break;
      buffer.resize(read);
      response += buffer;
    }
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);

  std::string translated = ExtractTranslatedText(response);
  if (translated == text)
    return {};
  return translated;
}

static std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n' ||
                            value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  size_t start = 0;
  while (start < value.size() &&
         (value[start] == '\r' || value[start] == '\n' ||
          value[start] == ' ' || value[start] == '\t')) {
    start++;
  }
  return value.substr(start);
}

static bool IsLikelyWhisperHallucination(const std::string &text) {
  std::string normalized;
  for (unsigned char c : text) {
    if (c >= 'A' && c <= 'Z')
      c = (unsigned char)(c + 32);
    if ((c >= 'a' && c <= 'z') || c == ' ')
      normalized.push_back((char)c);
  }
  normalized = Trim(normalized);
  return normalized == "you" || normalized == "you you" ||
         normalized == "thank you" || normalized == "thanks for watching";
}

static void AddMessage(const char *source, const char *author,
                       const std::string &original,
                       const std::string &translated, bool voice) {
  if (original.empty())
    return;

  TranscriptMessage msg;
  msg.source = source ? source : "";
  msg.author = author ? author : "";
  msg.original = original;
  msg.translated = translated;
  msg.timestamp = GetTickCount64();
  msg.voice = voice;

  std::lock_guard<std::mutex> lock(g_messagesMutex);
  g_messages.push_back(msg);
  int maxMessages = hooks::transcriptMaxMessages;
  if (maxMessages < 3)
    maxMessages = 3;
  if (maxMessages > 40)
    maxMessages = 40;
  while ((int)g_messages.size() > maxMessages)
    g_messages.pop_front();
}

static bool ParseChatLine(const std::string &line, std::string &author,
                          std::string &message) {
  // CS2 chat messages use ": " as the delimiter.
  // Reject lines that don't contain this pattern.
  size_t chatSep = line.find(": ");
  if (chatSep == std::string::npos)
    return false;

  // Use the FIRST occurrence of ": " (author is before, message is after)
  author = Trim(line.substr(0, chatSep));
  message = Trim(line.substr(chatSep + 2));

  // Extract known chat tags correctly
  std::string tag = "";
  if (author.find("[ALL] ") == 0) { tag = "[ALL] "; author = author.substr(6); }
  else if (author.find("[T] ") == 0) { tag = "[T] "; author = author.substr(4); }
  else if (author.find("[CT] ") == 0) { tag = "[CT] "; author = author.substr(5); }
  else if (author.find("[Party] ") == 0) { tag = "[Party] "; author = author.substr(8); }
  else if (author.find("[Spectator] ") == 0) { tag = "[Spectator] "; author = author.substr(12); }
  
  author = Trim(author);

  if (author.empty()) return false;
  
  // If it still starts with '[', it's an engine subsystem like [Developer], [Networking]
  if (author[0] == '[') return false;

  // Player names are at most 32 chars in CS2, be generous with 48
  if (author.size() > 48 || message.size() > 240)
    return false;

  // ── Reject known console output patterns ──
  static const char *rejectPrefixes[] = {
      "ConVar",        "SteamNetworking", "Host_Error",   "ChangeGameUI",
      "CGameClient",   "CNetworkSystem",  "S2C_CHALLENGE", "Connected to",
      "Connecting to", "Sending client",  "Loading ",     "Precaching",
      "exec ",         "Unknown command", "Server using",  "CSoundEmitter",
      "SV_",           "sv_",             "map_",          "cl_",
      "#",             "Error ",          "WARNING",       "WARN:",
      "INFO:",         "FATAL",           "Assert",        "Cmd_",
      "CMD_",          "ClientCmd",       "Lobby",         "Party",
      "HTTP",          "http",            "URL",           "CDN",
      "DataCenter",    "GC ",             "CSO_",          "Steam",
      "FCVAR",         "material/",       "models/",       "sounds/",
      "Resource",      "Texture",         "Shader",        "CMaterial",
      nullptr,
  };
  for (int i = 0; rejectPrefixes[i]; ++i) {
    if (author.find(rejectPrefixes[i]) != std::string::npos)
      return false;
  }

  // Reject if the "author" contains characters that can't be in player names
  for (char c : author) {
    if (c == '=' || c == '(' || c == ')' || c == '/' || c == '\\' ||
        c == '{' || c == '}' || c == ';' || c == '<' || c == '>')
      return false;
  }

  // Reject if message looks like a cvar value, path, or engine output
  if (message.find("exec ") == 0 || message.find("Unknown command") != std::string::npos)
    return false;
  if (message.find(":\\") != std::string::npos || message.find("://") != std::string::npos)
    return false;

  return true;
}

static std::vector<std::string> ConsoleLogCandidates() {
  std::vector<std::string> out;
  char exe[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char *slash = strrchr(exe, '\\');
  if (slash)
    *slash = '\0';

  char cwd[MAX_PATH] = {};
  GetCurrentDirectoryA(MAX_PATH, cwd);

  out.push_back(std::string(exe) + "\\console.log");
  out.push_back(std::string(exe) + "\\..\\..\\csgo\\console.log");
  out.push_back(std::string(exe) + "\\..\\csgo\\console.log");
  out.push_back(std::string(cwd) + "\\console.log");
  out.push_back("console.log");
  return out;
}

static bool ReadNewBytes(const std::string &path, uint64_t &offset,
                         std::string &buffer) {
  HANDLE file = CreateFileA(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    CloseHandle(file);
    return false;
  }
  if (offset == UINT64_MAX || offset > (uint64_t)size.QuadPart)
    offset = (uint64_t)size.QuadPart;
  if ((uint64_t)size.QuadPart <= offset) {
    CloseHandle(file);
    return true;
  }

  LARGE_INTEGER pos = {};
  pos.QuadPart = (LONGLONG)offset;
  SetFilePointerEx(file, pos, nullptr, FILE_BEGIN);

  DWORD toRead =
      (DWORD)std::min<uint64_t>((uint64_t)size.QuadPart - offset, 64 * 1024);
  std::string data(toRead, '\0');
  DWORD read = 0;
  BOOL ok = ReadFile(file, data.data(), toRead, &read, nullptr);
  CloseHandle(file);
  if (!ok)
    return false;
  data.resize(read);
  offset += read;
  buffer += data;
  return true;
}

static void TextThread() {
  uint64_t offset = UINT64_MAX;
  std::string carry;

  while (g_running) {
    if (!hooks::transcriptEnabled) {
      Sleep(500);
      continue;
    }

    std::string data;
    bool found = false;
    if (!g_lastLogPath.empty()) {
      found = ReadNewBytes(g_lastLogPath, offset, data);
    }
    if (!found) {
      for (const std::string &path : ConsoleLogCandidates()) {
        if (ReadNewBytes(path, offset, data)) {
          g_lastLogPath = path;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      SetStatus("Waiting for console.log (-condebug required)");
      Sleep(1000);
      continue;
    }

    SetStatus("Tailing %s", g_lastLogPath.c_str());
    carry += data;

    size_t newline = 0;
    while ((newline = carry.find('\n')) != std::string::npos) {
      std::string line = Trim(carry.substr(0, newline));
      carry.erase(0, newline + 1);

      std::string author;
      std::string msg;
      if (!ParseChatLine(line, author, msg))
        continue;

      std::string translated;
      if (hooks::transcriptTranslateText &&
          (hooks::transcriptTranslateAscii || LooksForeign(msg))) {
        translated = TranslateText(msg, hooks::transcriptTranslateAscii);
      }
      AddMessage("Text", author.c_str(), msg, translated, false);
    }

    Sleep(250);
  }
}

static void VoiceChunkCallback(const std::vector<float> &chunk) {
  std::lock_guard<std::mutex> lock(g_voiceMutex);
  if (g_voiceChunks.size() >= 3)
    g_voiceChunks.pop_front();
  g_voiceChunks.push_back(chunk);
  g_voiceCv.notify_one();
}

static void VoiceThread() {
  while (g_running) {
    std::vector<float> chunk;
    {
      std::unique_lock<std::mutex> lock(g_voiceMutex);
      g_voiceCv.wait_for(lock, std::chrono::milliseconds(500), [] {
        return !g_running || !g_voiceChunks.empty();
      });
      if (!g_running)
        break;
      if (g_voiceChunks.empty())
        continue;
      chunk = std::move(g_voiceChunks.front());
      g_voiceChunks.pop_front();
    }

    if (!hooks::transcriptEnabled || !hooks::transcriptVoiceEnabled)
      continue;
    if (!whisper_engine::IsReady() && !whisper_engine::Init())
      continue;

    std::string text = whisper_engine::Transcribe(chunk, 16000);
    text = Trim(text);
    if (text.empty() || IsLikelyWhisperHallucination(text))
      continue;

    std::string translated;
    if (hooks::transcriptTranslateVoice)
      translated = TranslateText(text, true);
    AddMessage("Voice", "voice", text, translated, true);
  }
}

static void StartAudioIfNeeded() {
  bool shouldRun = hooks::transcriptEnabled && hooks::transcriptVoiceEnabled;
  if (shouldRun == g_audioWanted)
    return;
  g_audioWanted = shouldRun;
  if (shouldRun) {
    whisper_engine::Init();
    audio_capture::Start(VoiceChunkCallback);
  } else {
    audio_capture::Stop();
  }
}

} // namespace

void transcript::Init() {
  if (g_running)
    return;
  g_running = true;
  g_textThread = std::thread(TextThread);
  g_voiceThread = std::thread(VoiceThread);
  SetStatus("Transcript initialized");
}

void transcript::Shutdown() {
  if (!g_running)
    return;
  g_running = false;
  audio_capture::Stop();
  g_voiceCv.notify_all();
  if (g_textThread.joinable())
    g_textThread.join();
  if (g_voiceThread.joinable())
    g_voiceThread.join();
  whisper_engine::Shutdown();
  SetStatus("Transcript stopped");
}

void transcript::OnFrame() {
  StartAudioIfNeeded();
}

void transcript::AddVoiceText(const char *text) {
  std::string original = text ? text : "";
  std::string translated;
  if (hooks::transcriptTranslateVoice)
    translated = TranslateText(original, true);
  AddMessage("Voice", "voice", original, translated, true);
}

void transcript::Render() {
  if (!hooks::transcriptEnabled)
    return;

  std::deque<TranscriptMessage> messages;
  {
    std::lock_guard<std::mutex> lock(g_messagesMutex);
    messages = g_messages;
  }

  ImGui::SetNextWindowPos(ImVec2(hooks::transcriptPanelX, hooks::transcriptPanelY),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(hooks::transcriptPanelW, hooks::transcriptPanelH),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.34f);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoSavedSettings;
  if (!ImGui::Begin("AI Transcript", &hooks::transcriptEnabled, flags)) {
    ImGui::End();
    return;
  }

  ImGui::TextColored(ImVec4(0.0f, 0.83f, 1.0f, 1.0f), "Target: %s",
                     TargetLangName());
  ImGui::SameLine();
  ImGui::TextDisabled("| %s", g_status);
  if (hooks::transcriptVoiceEnabled) {
    ImGui::TextDisabled("Voice: %s | %s", audio_capture::Status(),
                        whisper_engine::Status());
  }
  ImGui::Separator();

  if (messages.empty()) {
    ImGui::TextDisabled("No translated chat yet");
  } else {
    ImGui::BeginChild("TranscriptScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (const TranscriptMessage &msg : messages) {
      ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
      ImGui::TextColored(msg.voice ? ImVec4(0.40f, 0.85f, 1.0f, 1.0f)
                                   : ImVec4(0.85f, 0.85f, 0.90f, 1.0f),
                         "%s %s:", msg.source.c_str(), msg.author.c_str());
      if (!msg.translated.empty()) {
        ImGui::SameLine();
        ImGui::Text("%s", msg.translated.c_str());
        if (hooks::transcriptShowOriginal)
          ImGui::TextDisabled("  %s", msg.original.c_str());
      } else {
        ImGui::SameLine();
        ImGui::Text("%s", msg.original.c_str());
      }
      ImGui::PopTextWrapPos();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

const char *transcript::Status() {
  return g_status;
}
