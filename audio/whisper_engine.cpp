#include "pch.h"
#include "audio/whisper_engine.h"
#include <fstream>

extern "C" IMAGE_DOS_HEADER __ImageBase;

#include <shlobj.h>

namespace {

static bool g_ready = false;
static char g_status[256] = "Whisper not initialized";
static char g_cliPath[MAX_PATH] = "";
static char g_modelPath[MAX_PATH] = "";
static std::mutex g_mutex;

static bool FileExists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void SetStatus(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_status, sizeof(g_status), fmt, args);
  va_end(args);
}

static std::string ModuleDir() {
  char path[MAX_PATH] = {};
  GetModuleFileNameA((HMODULE)&__ImageBase, path, MAX_PATH);
  char *slash = strrchr(path, '\\');
  if (slash)
    *slash = '\0';
  return path;
}

static std::string AppDataWhisperDir() {
  char path[MAX_PATH];
  if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    return "";
  }
  std::string dir = std::string(path) + "\\CS2Overlay";
  CreateDirectoryA(dir.c_str(), NULL);
  dir += "\\whisper";
  CreateDirectoryA(dir.c_str(), NULL);
  return dir;
}

static bool FindWhisperFiles() {
  std::string dir = AppDataWhisperDir();
  if (dir.empty()) return false;

  std::string cliPath = dir + "\\whisper-cli.exe";
  std::string modelPath = dir + "\\ggml-tiny.bin";

  if (!FileExists(cliPath.c_str()) || !FileExists(modelPath.c_str())) {
    SetStatus("Downloading whisper dependencies (this may take a minute)...");
    
    std::string scriptPath = dir + "\\download.ps1";
    FILE* f = nullptr;
    fopen_s(&f, scriptPath.c_str(), "w");
    if (f) {
      fprintf(f, 
        "$ProgressPreference = 'SilentlyContinue'\n"
        "Invoke-WebRequest -Uri 'https://github.com/ggerganov/whisper.cpp/releases/download/v1.5.4/whisper-bin-x64.zip' -OutFile 'whisper.zip'\n"
        "Expand-Archive -Force 'whisper.zip' -DestinationPath .\n"
        "Invoke-WebRequest -Uri 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin' -OutFile 'ggml-tiny.bin'\n"
      );
      fclose(f);
      
      char cmd[MAX_PATH * 2];
      snprintf(cmd, sizeof(cmd), "powershell.exe -WindowStyle Hidden -ExecutionPolicy Bypass -File \"%s\"", scriptPath.c_str());
      
      STARTUPINFOA si = { sizeof(si) };
      PROCESS_INFORMATION pi = {};
      si.dwFlags = STARTF_USESHOWWINDOW;
      si.wShowWindow = SW_HIDE;
      if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000); // Wait up to 2 mins
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
      }
    }
  }

  if (FileExists(cliPath.c_str()) && FileExists(modelPath.c_str())) {
    strncpy_s(g_cliPath, cliPath.c_str(), _TRUNCATE);
    strncpy_s(g_modelPath, modelPath.c_str(), _TRUNCATE);
    return true;
  }

  return false;
}


static bool WriteWav(const char *path, const std::vector<float> &samples,
                     int sampleRate) {
  std::ofstream out(path, std::ios::binary);
  if (!out)
    return false;

  int16_t bitsPerSample = 16;
  int16_t channels = 1;
  int32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  int16_t blockAlign = channels * bitsPerSample / 8;
  int32_t dataSize = (int32_t)(samples.size() * sizeof(int16_t));
  int32_t riffSize = 36 + dataSize;

  out.write("RIFF", 4);
  out.write((char *)&riffSize, 4);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  int32_t fmtSize = 16;
  int16_t audioFormat = 1;
  out.write((char *)&fmtSize, 4);
  out.write((char *)&audioFormat, 2);
  out.write((char *)&channels, 2);
  out.write((char *)&sampleRate, 4);
  out.write((char *)&byteRate, 4);
  out.write((char *)&blockAlign, 2);
  out.write((char *)&bitsPerSample, 2);
  out.write("data", 4);
  out.write((char *)&dataSize, 4);

  for (float sample : samples) {
    if (sample > 1.0f)
      sample = 1.0f;
    if (sample < -1.0f)
      sample = -1.0f;
    int16_t pcm = (int16_t)(sample * 32767.0f);
    out.write((char *)&pcm, sizeof(pcm));
  }

  return true;
}

static std::string ReadFileText(const char *path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
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

} // namespace

bool whisper_engine::Init() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_ready = FindWhisperFiles();
  if (g_ready)
    SetStatus("Whisper ready");
  return g_ready;
}

void whisper_engine::Shutdown() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_ready = false;
  g_cliPath[0] = '\0';
  g_modelPath[0] = '\0';
  SetStatus("Whisper stopped");
}

bool whisper_engine::IsReady() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_ready;
}

std::string whisper_engine::Transcribe(const std::vector<float> &samples,
                                       int sampleRate) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_ready && !FindWhisperFiles())
    return {};

  char tempDir[MAX_PATH] = {};
  GetTempPathA(MAX_PATH, tempDir);
  char wavPath[MAX_PATH] = {};
  char outBase[MAX_PATH] = {};
  snprintf(wavPath, sizeof(wavPath), "%scs2_voice_%lu.wav", tempDir,
           GetCurrentThreadId());
  snprintf(outBase, sizeof(outBase), "%scs2_voice_%lu", tempDir,
           GetCurrentThreadId());

  if (!WriteWav(wavPath, samples, sampleRate)) {
    SetStatus("Failed to write temp WAV");
    return {};
  }

  char cmdLine[2048];
  snprintf(cmdLine, sizeof(cmdLine),
           "\"%s\" -m \"%s\" -f \"%s\" -otxt -of \"%s\" -nt -l auto -t 2",
           g_cliPath, g_modelPath, wavPath, outBase);

  // Extract the directory of whisper-cli.exe so it can find its companion DLLs
  char cliDir[MAX_PATH] = {};
  strncpy_s(cliDir, g_cliPath, _TRUNCATE);
  char *lastSlash = strrchr(cliDir, '\\');
  if (lastSlash)
    *lastSlash = '\0';

  STARTUPINFOA si = {};
  PROCESS_INFORMATION pi = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  BOOL ok = CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, cliDir, &si, &pi);
  if (!ok) {
    DeleteFileA(wavPath);
    SetStatus("Failed to launch whisper-cli.exe (err=%lu)", GetLastError());
    return {};
  }

  DWORD wait = WaitForSingleObject(pi.hProcess, 30000);
  DWORD exitCode = 1;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  DeleteFileA(wavPath);

  if (wait != WAIT_OBJECT_0 || exitCode != 0) {
    SetStatus("Whisper process failed or timed out");
    return {};
  }

  char txtPath[MAX_PATH];
  snprintf(txtPath, sizeof(txtPath), "%s.txt", outBase);
  std::string text = Trim(ReadFileText(txtPath));
  DeleteFileA(txtPath);
  if (!text.empty())
    SetStatus("Whisper ready");
  return text;
}

const char *whisper_engine::Status() {
  return g_status;
}
