#pragma once
#ifndef CAPSCRIPT_THEMEMANAGER_H
#define CAPSCRIPT_THEMEMANAGER_H

#include <QString>

namespace CapScript {

namespace Colors {

constexpr const char *BgBase = "#0d0d0d";
constexpr const char *BgSurface = "#141414";
constexpr const char *BgCard = "#1a1a1a";
constexpr const char *BgElevated = "#212121";
constexpr const char *BgInput = "#111111";
constexpr const char *BgHover = "#262626";
constexpr const char *BgOverlay = "#1c1c1c";

constexpr const char *BorderSubtle = "#202020";
constexpr const char *Border = "#2c2c2c";
constexpr const char *BorderStrong = "#3a3a3a";
constexpr const char *BorderFocus = "#FF0033";

constexpr const char *TextPrimary = "#efefef";
constexpr const char *TextSecondary = "#909090";
constexpr const char *TextMuted = "#505050";
constexpr const char *TextDisabled = "#404040";
constexpr const char *TextOnAccent = "#ffffff";

constexpr const char *Accent = "#FF0033";
constexpr const char *AccentHover = "#e6002e";
constexpr const char *AccentPress = "#cc0029";
constexpr const char *AccentGlow = "#ff003326";
constexpr const char *AccentSubtle = "#200b0e";
constexpr const char *AccentMid = "#80001a";

constexpr const char *Success = "#3fb950";
constexpr const char *SuccessBg = "#0d2114";
constexpr const char *Warning = "#d29922";
constexpr const char *WarningBg = "#1c1700";
constexpr const char *Error = "#f85149";
constexpr const char *ErrorBg = "#200b0b";
constexpr const char *Info = "#388bfd";
constexpr const char *InfoBg = "#091930";

constexpr const char *DisabledBg = "#181818";
constexpr const char *DisabledFg = "#3d3d3d";
constexpr const char *DisabledBdr = "#222222";

constexpr const char *DonateHover = "#f472b6";

}

class ThemeManager {
public:
  static QString generateQSS(const QString &themeName = "dark");
};

}

#endif