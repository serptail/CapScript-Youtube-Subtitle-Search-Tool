#include "ThemeManager.h"

namespace CapScript {

QString ThemeManager::generateQSS(const QString&) {
    using namespace Colors;

    QString qss = QStringLiteral(
        R"(
QWidget {
 
    font-family: 'Inter', 'Segoe UI', -apple-system, 'Helvetica Neue', Arial, sans-serif;
    font-size: 9pt;
    color: %1;
}


#mainWindow {
  
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


#renderConfigGroup {
   
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

QLabel#pageHeading {
    color: %1;
    font-size: 10.5pt;
    font-weight: 600;
}

QLabel#progressLabel {
    color: %8;
    font-weight: 700;
    font-size: 9pt;
}


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