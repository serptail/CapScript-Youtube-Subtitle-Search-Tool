#include "ThemeManager.h"

namespace CapScript {

QString ThemeManager::generateQSS(const QString&) {
    using namespace Colors;

    // Phantom now paints every stock control (buttons, checkboxes, radios,
    // combos, spinboxes, sliders, scrollbars, menus, list/tree/table rows,
    // headers) straight from the QPalette set in Application::setupPalette.
    // This file only covers what Phantom has no opinion on: the frameless
    // titlebar, the custom tab strip, and a handful of bespoke display
    // widgets that aren't standard Qt controls. Do NOT add border/
    // background-color/padding rules for stock widget classes here —
    // any box-model QSS on a widget class makes Qt fall back to its own
    // stylesheet painter for that class, silently un-Phantom-ing it.

    QString qss = QStringLiteral(
        R"(
QWidget {
    /* font-family/font-size/color are NOT box-model properties, so setting
       them here does not flag matched widgets as "styled" in
       QStyleSheetStyle and does not disable PhantomStyle's painting for
       them. Do NOT add background/border/padding/margin to this rule —
       see note below. */
    font-family: 'Inter', 'Segoe UI', -apple-system, 'Helvetica Neue', Arial, sans-serif;
    font-size: 9pt;
    color: %1;
}

/* NOTE: there is intentionally no "QWidget { background-color: ... }" or
   "* { ... }" rule here. Both selectors match QComboBox/QPushButton/etc.
   via inherits("QWidget") the same way any type selector matches its
   subclasses. The moment a box-model property (background/border/padding/
   margin) matches a widget through ANY selector, QStyleSheetStyle flags
   that widget instance "styled" and takes over painting it itself,
   bypassing PhantomStyle's CC_ComboBox/PE_PanelButtonCommand entirely —
   which is what produced the flat black combos/buttons. Transparent
   backgrounds for stock controls come from QPalette::Window/Button in
   Application::setupPalette instead; only ID selectors (#foo) or type
   selectors for non-stock-control classes (QLabel, QDialog, ...) are safe
   to give background-color/border/padding here. */

#mainWindow {
    /* Must stay transparent: with WA_TranslucentBackground enabled, this is
       the true (composited) window background. An opaque fill here would
       paint square corners behind #centralContainer's rounded border and
       defeat the anti-aliased rounding DWM now provides. */
    background-color: transparent;
    margin: 0px;
    padding: 0px;
}

#centralContainer {
    background-color: %2;
    border: 1px solid %3;
    border-radius: 5px;
    margin: 0px;
    padding: 0px;
}

/* ---- Titlebar: frameless window chrome, no native equivalent ---- */

#titleBar {
    background-color: %4;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    border-bottom: 1px solid %20;
    min-height: 1.9em;
    max-height: 2.2em;
}

#titleBar QLabel#titleLabel {
    font-family: 'Akrobat Black', 'Segoe UI Variable Display', 'Segoe UI', sans-serif;
    font-weight: 700;
    font-size: 10.5pt;
    letter-spacing: 0.4px;
    color: %1;
    background: transparent;
}

#titleBar QLabel#subtitleLabel {
    font-size: 7.5pt;
    font-weight: 500;
    letter-spacing: 0.8px;
    color: %6;
    background: transparent;
}

#titleBar QPushButton {
    background-color: transparent;
    color: %5;
    border: none;
    border-radius: 3px;
    font-size: 9.5pt;
    padding: 1px 4px;
    min-width: 22px;
    min-height: 18px;
    margin: 3px 1px;
}

#titleBar QPushButton:hover {
    background-color: %7;
    color: %1;
}

#titleBar QPushButton:pressed {
    background-color: %3;
    color: %1;
}

#titleBar QPushButton#minimizeButton,
#titleBar QPushButton#maximizeButton,
#titleBar QPushButton#closeButton {
    margin: 0px 0px 4px 0px;
    min-width: 1.8em;
    min-height: 1.2em;
    border-radius: 3px;
}

#titleBar QPushButton#closeButton:hover {
    background-color: %8;
    color: %9;
}

#titleBar QPushButton#titleExtraBtn,
#titleBar QPushButton#donateBtn {
    margin: 0px 2px 8px 2px;
}

#titleBar QPushButton#donateBtn:hover {
    background-color: %24;
    color: %9;
}

/* ---- Custom tab strip (not QTabBar-driven, hand-rolled) ---- */

#tabBar {
    background-color: %4;
    border-bottom: 1px solid %20;
}

#tabBar::tab {
    background-color: transparent;
    color: %5;
    border: none;
    border-bottom: 2px solid transparent;
    padding: 6px 16px;
    font-size: 8.5pt;
    font-weight: 500;
    min-width: 64px;
}

#tabBar::tab:hover {
    color: %1;
    background-color: rgba(255,255,255, 0.03);
    border-bottom-color: %16;
}

#tabBar::tab:selected {
    color: %1;
    font-weight: 700;
    border-bottom: 2px solid %8;
    background-color: rgba(255,0,51, 0.04);
}

#pageStack {
    background-color: %2;
    border-bottom-left-radius: 4px;
    border-bottom-right-radius: 4px;
}

/* Plain text labels aren't a "control" Phantom paints, so free-form
   color/size variants stay CSS. */

QLabel {
    color: %1;
    background: transparent;
}

QLabel[secondary="true"] {
    color: %5;
    font-size: 9pt;
}

QLabel[muted="true"] {
    color: %6;
    font-size: 8.5pt;
}

QToolTip {
    background-color: %19;
    color: %1;
    border: 1px solid %16;
    padding: 4px 10px;
    border-radius: 5px;
    font-size: 8.5pt;
    font-weight: 500;
}

QStatusBar {
    background-color: %4;
    color: %6;
    border-top: 1px solid %20;
    font-size: 8pt;
    padding: 0 6px;
    min-height: 18px;
}

QStatusBar::item {
    border: none;
}

QStatusBar QLabel {
    color: %6;
    padding: 0 4px;
}

QDialog {
    background-color: %26;
    border: 1px solid %16;
    border-radius: 5px;
}

/* ---- Bespoke display widgets: not standard controls ---- */

#renderConfigGroup {
    /* Group box itself stays Phantom-painted (frame); title now lives in
       the #sectionDivider label placed above the box instead of as the
       QGroupBox title. */
    background-color: #111111;
    border-radius: 5px;
}

#listConfigGroup {
    background-color: #111111;
    border-radius: 5px;
}

#aboutGroup {
    background-color: #111111;
    border-radius: 5px;
}

#timestampsGroup {
    background-color: #111111;
    border-radius: 5px;
}

#optionsGroup {
    background-color: #111111;
    border-radius: 5px;
}

#searchModeGroup {
    background-color: #111111;
    border-radius: 5px;
}

#outputGroup {
    background-color: #111111;
    border-radius: 5px;
}

#proxyGroup {
    background-color: #111111;
    border-radius: 5px;
}

QTableWidget#resultsTable {
    /* Table frame, header, and the checkbox column background all read as
       #111111; the individual title/video-id row cells are set to #111111
       via item background in code so only the checkbox column contrasts
       against the header/edges. */
    background-color: #111111;
    border: 1px solid %3;
    border-radius: 4px;
    gridline-color: transparent;
}

QTableWidget#resultsTable QHeaderView::section {
    background-color: #111111;
    color: %5;
    border: none;
    border-bottom: 1px solid %20;
    padding: 4px 8px;
    font-size: 8.5pt;
    font-weight: 600;
}

QTextEdit#logDisplay {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 4px;
    padding: 6px 8px;
    font-family: 'Cascadia Code', 'JetBrains Mono', 'Consolas', monospace;
    font-size: 8pt;
}

QTextEdit#logDisplay[hovered="true"] {
    border-color: %8;
    background-color: #111111;
}

QTextEdit#logDisplay[expanded="true"] {
    border-color: %8;
}

QTextBrowser#viewerDisplay {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 5px;
    padding: 10px 12px;
    font-size: 9pt;
    line-height: 1.5;
}

QLabel#videoPlaceholder {
    background-color: %10;
    border: 1px solid %8;
    border-radius: 5px;
    color: %6;
    font-size: 9pt;
    padding: 8px;
}

QLabel#thumbnailPreview {
    background-color: %10;
    border: 1px solid %3;
    border-radius: 5px;
    color: %6;
    font-size: 9pt;
    padding: 8px;
}

QLabel#thumbnailPreview:focus {
    outline: none;
}

QLabel#infoBanner {
    background-color: %18;
    border: 1px solid %8;
    border-left: 3px solid %8;
    border-radius: 3px;
    color: %1;
    padding: 5px 8px;
    font-size: 8.5pt;
}

QLabel#sectionDivider {
    color: %6;
    font-size: 7.5pt;
    font-weight: 700;
    letter-spacing: 1px;
    text-transform: uppercase;
    border-bottom: 1px solid %20;
    padding-bottom: 4px;
}

/* Plain page/section heading (e.g. "Render Configuration") — normal case,
   no underline, matches the weight used for headings elsewhere. */
QLabel#pageHeading {
    color: %1;
    font-size: 10.5pt;
    font-weight: 600;
}

/* Accent-colored percentage readout next to an update/progress bar. */
QLabel#progressLabel {
    color: %8;
    font-weight: 700;
    font-size: 9pt;
}

/* ---- Quality slider: groove (track), filled portion, and handle each get
   their own color so the fill level reads clearly against the track. ---- */

QSlider#qualitySlider::groove:horizontal {
    height: 4px;
    background-color: %19;
    border-radius: 2px;
}

QSlider#qualitySlider::add-page:horizontal {
    height: 4px;
    background-color: %19;
    border-radius: 2px;
}

QSlider#qualitySlider::sub-page:horizontal {
    height: 4px;
    background-color: %8;
    border-radius: 2px;
}

QSlider#qualitySlider::handle:horizontal {
    width: 12px;
    height: 12px;
    margin: -4px 0;
    background-color: #ffffff;
    border: 2px solid %8;
    border-radius: 3px;
}

QSlider#qualitySlider::handle:horizontal:hover {
    background-color: #ffffff;
    border-color: %1;
}

QSlider#qualitySlider::handle:horizontal:pressed {
    background-color: #ffffff;
    border-color: %8;
}
)")
        // NOTE: QString::arg() binds to the lowest-numbered %N still
        // unresolved in the string, not to a fixed "argument slot" — so
        // this chain must list only the placeholders that actually
        // survived the trim (%1-%10, %16-%20, %24, %26), in that order.
        .arg(TextPrimary)     // %1
        .arg(BgBase)          // %2
        .arg(Border)          // %3
        .arg(BgSurface)       // %4
        .arg(TextSecondary)   // %5
        .arg(TextMuted)       // %6
        .arg(BgHover)         // %7
        .arg(Accent)          // %8
        .arg(TextOnAccent)    // %9
        .arg(BgCard)          // %10
        .arg(BorderStrong)    // %16
        .arg(BgInput)         // %17
        .arg(AccentSubtle)    // %18
        .arg(BgElevated)      // %19
        .arg(BorderSubtle)    // %20
        .arg(DonateHover)     // %24
        .arg(BgOverlay);      // %26

    return qss;
}

}