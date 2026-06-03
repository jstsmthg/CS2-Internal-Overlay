#include "pch.h"
#include <cstring>
#include <cstdio>

static const char *CONFIG_FILE = "cs2_config.ini";

// Helper: write a key=value line
static void WriteKey(FILE *f, const char *key, float val) {
  fprintf(f, "%s=%.4f\n", key, val);
}
static void WriteKey(FILE *f, const char *key, int val) {
  fprintf(f, "%s=%d\n", key, val);
}
static void WriteKey(FILE *f, const char *key, bool val) {
  fprintf(f, "%s=%d\n", key, val ? 1 : 0);
}
static void WriteKey(FILE *f, const char *key, const char *val) {
  fprintf(f, "%s=%s\n", key, val);
}

void config::Save() {
  FILE *f = nullptr;
  fopen_s(&f, CONFIG_FILE, "w");
  if (!f)
    return;

  fprintf(f, "[CS2Overlay]\n");
  WriteKey(f, "espEnabled", hooks::espEnabled);
  WriteKey(f, "espStyle", hooks::espStyle);
  WriteKey(f, "ignoreTeam", hooks::ignoreTeam);
  WriteKey(f, "aimbotEnabled", hooks::aimbotEnabled);
  WriteKey(f, "triggerbotEnabled", hooks::triggerbotEnabled);
  WriteKey(f, "radarEnabled", hooks::radarEnabled);
  WriteKey(f, "rcsEnabled", hooks::rcsEnabled);
  WriteKey(f, "bombTimerEnabled", hooks::bombTimerEnabled);
  WriteKey(f, "aimbotSmoothing", hooks::aimbotSmoothing);
  WriteKey(f, "aimbotFov", hooks::aimbotFov);
  WriteKey(f, "aimbotVisCheck", hooks::aimbotVisCheck);
  WriteKey(f, "aimbotKey", hooks::aimbotKey);
  WriteKey(f, "triggerbotDelay", hooks::triggerbotDelay);

  fclose(f);
}

// Helper: read a value from a line matching "key=value"
static bool ParseLine(const char *line, const char *key, float &out) {
  size_t keyLen = strlen(key);
  if (strncmp(line, key, keyLen) == 0 && line[keyLen] == '=') {
    out = (float)atof(line + keyLen + 1);
    return true;
  }
  return false;
}

static bool ParseLine(const char *line, const char *key, int &out) {
  size_t keyLen = strlen(key);
  if (strncmp(line, key, keyLen) == 0 && line[keyLen] == '=') {
    out = atoi(line + keyLen + 1);
    return true;
  }
  return false;
}

static bool ParseLine(const char *line, const char *key, bool &out) {
  size_t keyLen = strlen(key);
  if (strncmp(line, key, keyLen) == 0 && line[keyLen] == '=') {
    out = (atoi(line + keyLen + 1) != 0);
    return true;
  }
  return false;
}

void config::Load() {
  FILE *f = nullptr;
  fopen_s(&f, CONFIG_FILE, "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    // Strip newline
    size_t len = strlen(line);
    if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';

    // Skip section headers and empty lines
    if (line[0] == '[' || line[0] == '\0')
      continue;

    ParseLine(line, "espEnabled", hooks::espEnabled);
    ParseLine(line, "espStyle", hooks::espStyle);
    ParseLine(line, "ignoreTeam", hooks::ignoreTeam);
    ParseLine(line, "aimbotEnabled", hooks::aimbotEnabled);
    ParseLine(line, "triggerbotEnabled", hooks::triggerbotEnabled);
    ParseLine(line, "radarEnabled", hooks::radarEnabled);
    ParseLine(line, "rcsEnabled", hooks::rcsEnabled);
    ParseLine(line, "bombTimerEnabled", hooks::bombTimerEnabled);
    ParseLine(line, "aimbotSmoothing", hooks::aimbotSmoothing);
    ParseLine(line, "aimbotFov", hooks::aimbotFov);
    ParseLine(line, "aimbotVisCheck", hooks::aimbotVisCheck);
    ParseLine(line, "aimbotKey", hooks::aimbotKey);
    ParseLine(line, "triggerbotDelay", hooks::triggerbotDelay);
  }

  fclose(f);
}
