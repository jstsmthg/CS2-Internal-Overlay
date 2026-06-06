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
static void WriteColor(FILE *f, const char *key, float *col) {
  fprintf(f, "%s=%.4f,%.4f,%.4f,%.4f\n", key, col[0], col[1], col[2], col[3]);
}

void config::Save() {
  FILE *f = nullptr;
  fopen_s(&f, CONFIG_FILE, "w");
  if (!f)
    return;

  fprintf(f, "[CS2Overlay]\n");

  // Aimbot
  WriteKey(f, "aimbotEnabled", hooks::aimbotEnabled);
  WriteKey(f, "aimbotKey", hooks::aimbotKey);
  WriteKey(f, "aimbotSmoothing", hooks::aimbotSmoothing);
  WriteKey(f, "aimbotFov", hooks::aimbotFov);
  WriteKey(f, "aimbotVisCheck", hooks::aimbotVisCheck);
  WriteKey(f, "aimbotHitbox", hooks::aimbotHitbox);
  WriteKey(f, "aimbotClosestHitbox", hooks::aimbotClosestHitbox);
  WriteKey(f, "aimbotLinearSmooth", hooks::aimbotLinearSmooth);
  WriteKey(f, "aimbotShowFov", hooks::aimbotShowFov);
  WriteColor(f, "aimbotFovColor", hooks::aimbotFovColor);
  WriteKey(f, "aimbotFovFollowRecoil", hooks::aimbotFovFollowRecoil);
  WriteColor(f, "aimbotFovFollowColor", hooks::aimbotFovFollowColor);
  WriteKey(f, "aimbotWeaponCat", hooks::aimbotWeaponCat);

  // RCS
  WriteKey(f, "rcsEnabled", hooks::rcsEnabled);
  WriteKey(f, "rcsX", hooks::rcsX);
  WriteKey(f, "rcsY", hooks::rcsY);

  // Triggerbot
  WriteKey(f, "triggerbotEnabled", hooks::triggerbotEnabled);
  WriteKey(f, "triggerbotDelay", hooks::triggerbotDelay);
  WriteKey(f, "triggerbotAim", hooks::triggerbotAim);
  WriteKey(f, "triggerbotAimKey", hooks::triggerbotAimKey);
  WriteKey(f, "triggerbotAimUseRecoil", hooks::triggerbotAimUseRecoil);
  WriteKey(f, "triggerbotAimFov", hooks::triggerbotAimFov);
  WriteKey(f, "triggerbotAimSmooth", hooks::triggerbotAimSmooth);
  WriteKey(f, "triggerbotAimHitbox", hooks::triggerbotAimHitbox);

  // ESP
  WriteKey(f, "espEnabled", hooks::espEnabled);
  WriteKey(f, "espStyle", hooks::espStyle);
  WriteKey(f, "espNameEnabled", hooks::espNameEnabled);
  WriteColor(f, "espNameColor", hooks::espNameColor);
  WriteColor(f, "espBoxColor", hooks::espBoxColor);
  WriteColor(f, "espCorneredColor", hooks::espCorneredColor);
  WriteKey(f, "espHealthBar", hooks::espHealthBar);
  WriteColor(f, "espHealthStartColor", hooks::espHealthStartColor);
  WriteColor(f, "espHealthEndColor", hooks::espHealthEndColor);
  WriteKey(f, "espArmorBar", hooks::espArmorBar);
  WriteColor(f, "espArmorColor", hooks::espArmorColor);
  WriteKey(f, "espWeaponEnabled", hooks::espWeaponEnabled);
  WriteColor(f, "espWeaponColor", hooks::espWeaponColor);
  WriteKey(f, "espSkeletonEnabled", hooks::espSkeletonEnabled);
  WriteColor(f, "espSkeletonColor", hooks::espSkeletonColor);
  WriteKey(f, "espHeadCircle", hooks::espHeadCircle);
  WriteColor(f, "espHeadCircleColor", hooks::espHeadCircleColor);
  WriteKey(f, "espWeaponIcon", hooks::espWeaponIcon);
  WriteKey(f, "espSoundEnabled", hooks::espSoundEnabled);
  WriteKey(f, "espSoundTime", hooks::espSoundTime);
  WriteKey(f, "espToggleKey", hooks::espToggleKey);

  // Radar
  WriteKey(f, "radarEnabled", hooks::radarEnabled);
  WriteKey(f, "radarForceHud", hooks::radarForceHud);
  WriteKey(f, "radarOutlines", hooks::radarOutlines);
  WriteKey(f, "radarPlayerDot", hooks::radarPlayerDot);
  WriteColor(f, "radarDotColor", hooks::radarDotColor);
  WriteKey(f, "radarDroppedWeapons", hooks::radarDroppedWeapons);
  WriteKey(f, "radarDroppedGrenades", hooks::radarDroppedGrenades);

  // World ESP
  WriteKey(f, "bombTimerEnabled", hooks::bombTimerEnabled);
  WriteKey(f, "bombLocation", hooks::bombLocation);
  WriteColor(f, "bombLocationColor", hooks::bombLocationColor);
  WriteKey(f, "bombDefuseCircle", hooks::bombDefuseCircle);
  WriteKey(f, "droppedItemsEnabled", hooks::droppedItemsEnabled);
  WriteColor(f, "droppedItemsColor", hooks::droppedItemsColor);
  WriteKey(f, "droppedWeaponsEnabled", hooks::droppedWeaponsEnabled);
  WriteKey(f, "droppedGrenadesEnabled", hooks::droppedGrenadesEnabled);
  WriteKey(f, "molotovFireEnabled", hooks::molotovFireEnabled);

  // Misc
  WriteKey(f, "ignoreTeam", hooks::ignoreTeam);
  WriteKey(f, "streamproof", hooks::streamproof);
  WriteKey(f, "customGameFov", hooks::customGameFov);
  WriteKey(f, "spectatorList", hooks::spectatorList);
  WriteKey(f, "spectatorListVerbose", hooks::spectatorListVerbose);
  WriteKey(f, "hitmarkerEnabled", hooks::hitmarkerEnabled);
  WriteKey(f, "hitmarkerSound", hooks::hitmarkerSound);
  WriteKey(f, "hitmarkerVolume", hooks::hitmarkerVolume);
  WriteKey(f, "watermarkEnabled", hooks::watermarkEnabled);
  WriteKey(f, "menuKeyBind", hooks::menuKeyBind);
  WriteKey(f, "recoilCrosshair", hooks::recoilCrosshair);
  WriteKey(f, "snaptapEnabled", hooks::snaptapEnabled);
  WriteKey(f, "maxFps", hooks::maxFps);
  WriteKey(f, "vsyncEnabled", hooks::vsyncEnabled);

  // Grenade Helper
  WriteKey(f, "grenadeHelperEnabled", hooks::grenadeHelperEnabled);
  WriteKey(f, "grenadeFilter", hooks::grenadeFilter);
  WriteKey(f, "grenadeDistance", hooks::grenadeDistance);
  WriteColor(f, "grenadeColor", hooks::grenadeColor);
  WriteKey(f, "grenadeAimAssist", hooks::grenadeAimAssist);

  fclose(f);
}

// ============================================================================
// Parsers
// ============================================================================
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

static bool ParseColor(const char *line, const char *key, float *out) {
  size_t keyLen = strlen(key);
  if (strncmp(line, key, keyLen) == 0 && line[keyLen] == '=') {
    sscanf_s(line + keyLen + 1, "%f,%f,%f,%f", &out[0], &out[1], &out[2],
           &out[3]);
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

    // Aimbot
    ParseLine(line, "aimbotEnabled", hooks::aimbotEnabled);
    ParseLine(line, "aimbotKey", hooks::aimbotKey);
    ParseLine(line, "aimbotSmoothing", hooks::aimbotSmoothing);
    ParseLine(line, "aimbotFov", hooks::aimbotFov);
    ParseLine(line, "aimbotVisCheck", hooks::aimbotVisCheck);
    ParseLine(line, "aimbotHitbox", hooks::aimbotHitbox);
    ParseLine(line, "aimbotClosestHitbox", hooks::aimbotClosestHitbox);
    ParseLine(line, "aimbotLinearSmooth", hooks::aimbotLinearSmooth);
    ParseLine(line, "aimbotShowFov", hooks::aimbotShowFov);
    ParseColor(line, "aimbotFovColor", hooks::aimbotFovColor);
    ParseLine(line, "aimbotFovFollowRecoil", hooks::aimbotFovFollowRecoil);
    ParseColor(line, "aimbotFovFollowColor", hooks::aimbotFovFollowColor);
    ParseLine(line, "aimbotWeaponCat", hooks::aimbotWeaponCat);

    // RCS
    ParseLine(line, "rcsEnabled", hooks::rcsEnabled);
    ParseLine(line, "rcsX", hooks::rcsX);
    ParseLine(line, "rcsY", hooks::rcsY);

    // Triggerbot
    ParseLine(line, "triggerbotEnabled", hooks::triggerbotEnabled);
    ParseLine(line, "triggerbotDelay", hooks::triggerbotDelay);
    ParseLine(line, "triggerbotAim", hooks::triggerbotAim);
    ParseLine(line, "triggerbotAimKey", hooks::triggerbotAimKey);
    ParseLine(line, "triggerbotAimUseRecoil", hooks::triggerbotAimUseRecoil);
    ParseLine(line, "triggerbotAimFov", hooks::triggerbotAimFov);
    ParseLine(line, "triggerbotAimSmooth", hooks::triggerbotAimSmooth);
    ParseLine(line, "triggerbotAimHitbox", hooks::triggerbotAimHitbox);

    // ESP
    ParseLine(line, "espEnabled", hooks::espEnabled);
    ParseLine(line, "espStyle", hooks::espStyle);
    ParseLine(line, "espNameEnabled", hooks::espNameEnabled);
    ParseColor(line, "espNameColor", hooks::espNameColor);
    ParseColor(line, "espBoxColor", hooks::espBoxColor);
    ParseColor(line, "espCorneredColor", hooks::espCorneredColor);
    ParseLine(line, "espHealthBar", hooks::espHealthBar);
    ParseColor(line, "espHealthStartColor", hooks::espHealthStartColor);
    ParseColor(line, "espHealthEndColor", hooks::espHealthEndColor);
    ParseLine(line, "espArmorBar", hooks::espArmorBar);
    ParseColor(line, "espArmorColor", hooks::espArmorColor);
    ParseLine(line, "espWeaponEnabled", hooks::espWeaponEnabled);
    ParseColor(line, "espWeaponColor", hooks::espWeaponColor);
    ParseLine(line, "espSkeletonEnabled", hooks::espSkeletonEnabled);
    ParseColor(line, "espSkeletonColor", hooks::espSkeletonColor);
    ParseLine(line, "espHeadCircle", hooks::espHeadCircle);
    ParseColor(line, "espHeadCircleColor", hooks::espHeadCircleColor);
    ParseLine(line, "espWeaponIcon", hooks::espWeaponIcon);
    ParseLine(line, "espSoundEnabled", hooks::espSoundEnabled);
    ParseLine(line, "espSoundTime", hooks::espSoundTime);
    ParseLine(line, "espToggleKey", hooks::espToggleKey);

    // Radar
    ParseLine(line, "radarEnabled", hooks::radarEnabled);
    ParseLine(line, "radarForceHud", hooks::radarForceHud);
    ParseLine(line, "radarOutlines", hooks::radarOutlines);
    ParseLine(line, "radarPlayerDot", hooks::radarPlayerDot);
    ParseColor(line, "radarDotColor", hooks::radarDotColor);
    ParseLine(line, "radarDroppedWeapons", hooks::radarDroppedWeapons);
    ParseLine(line, "radarDroppedGrenades", hooks::radarDroppedGrenades);

    // World ESP
    ParseLine(line, "bombTimerEnabled", hooks::bombTimerEnabled);
    ParseLine(line, "bombLocation", hooks::bombLocation);
    ParseColor(line, "bombLocationColor", hooks::bombLocationColor);
    ParseLine(line, "bombDefuseCircle", hooks::bombDefuseCircle);
    ParseLine(line, "droppedItemsEnabled", hooks::droppedItemsEnabled);
    ParseColor(line, "droppedItemsColor", hooks::droppedItemsColor);
    ParseLine(line, "droppedWeaponsEnabled", hooks::droppedWeaponsEnabled);
    ParseLine(line, "droppedGrenadesEnabled", hooks::droppedGrenadesEnabled);
    ParseLine(line, "molotovFireEnabled", hooks::molotovFireEnabled);

    // Misc
    ParseLine(line, "ignoreTeam", hooks::ignoreTeam);
    ParseLine(line, "streamproof", hooks::streamproof);
    ParseLine(line, "customGameFov", hooks::customGameFov);
    ParseLine(line, "spectatorList", hooks::spectatorList);
    ParseLine(line, "spectatorListVerbose", hooks::spectatorListVerbose);
    ParseLine(line, "hitmarkerEnabled", hooks::hitmarkerEnabled);
    ParseLine(line, "hitmarkerSound", hooks::hitmarkerSound);
    ParseLine(line, "hitmarkerVolume", hooks::hitmarkerVolume);
    ParseLine(line, "watermarkEnabled", hooks::watermarkEnabled);
    ParseLine(line, "menuKeyBind", hooks::menuKeyBind);
    ParseLine(line, "recoilCrosshair", hooks::recoilCrosshair);
    ParseLine(line, "snaptapEnabled", hooks::snaptapEnabled);
    ParseLine(line, "maxFps", hooks::maxFps);
    ParseLine(line, "vsyncEnabled", hooks::vsyncEnabled);

    // Grenade Helper
    ParseLine(line, "grenadeHelperEnabled", hooks::grenadeHelperEnabled);
    ParseLine(line, "grenadeFilter", hooks::grenadeFilter);
    ParseLine(line, "grenadeDistance", hooks::grenadeDistance);
    ParseColor(line, "grenadeColor", hooks::grenadeColor);
    ParseLine(line, "grenadeAimAssist", hooks::grenadeAimAssist);
  }

  fclose(f);
}
