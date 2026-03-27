#include "ThemeManager.h"

namespace CapScript {

QString ThemeManager::generateQSS(const QString&) {
    using namespace Colors;

    QString qss = QStringLiteral(R"(
* {
    font-family: 'Inter', 'Segoe UI', -apple-system, 'Helvetica Neue', Arial, sans-serif;
    font-size: 10pt;
    color: %1;
    outline: none;
    box-sizing: border-box;
}

QWidget {
    background-color: transparent;
}

#mainWindow {
    background-color: %2;
    margin: 0px;
    padding: 0px;
}

#centralContainer {
    background-color: %2;
    border: 1px solid %3;
    border-radius: 10px;
    margin: 0px;
    padding: 0px;
}

#titleBar {
    background-color: %4;
    border-top-left-radius: 9px;
    border-top-right-radius: 9px;
    border-bottom: 1px solid %20;
    min-height: 2.2em;
    max-height: 2.6em;
}

#titleBar QLabel#titleLabel {
    font-family: 'Akrobat Black', 'Segoe UI Variable Display', 'Segoe UI', sans-serif;
    font-weight: 700;
    font-size: 13pt;
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
    border-radius: 5px;
    font-size: 11pt;
    padding: 2px 6px;
    min-width: 28px;
    min-height: 22px;
    margin: 4px 2px;
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
    margin: 0px 0px 6px 0px;
    min-width: 2.0em;
    min-height: 1.4em;
    border-radius: 4px;
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
    padding: 10px 28px;
    font-size: 9.5pt;
    font-weight: 500;
    min-width: 88px;
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
    border-bottom-left-radius: 9px;
    border-bottom-right-radius: 9px;
}

QGroupBox {
    background-color: %10;
    border: 1px solid %3;
    border-radius: 8px;
    margin-top: 16px;
    padding: 14px 12px 10px 12px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 14px;
    top: -1px;
    padding: 1px 8px 2px 8px;
    background-color: %10;
    color: %6;
    border: none;
    border-radius: 3px;
    font-size: 7.5pt;
    font-weight: 700;
    letter-spacing: 0.8px;
    text-transform: uppercase;
}

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

QPushButton {
    background-color: %8;
    color: %9;
    border: none;
    border-radius: 6px;
    padding: 7px 22px;
    font-weight: 700;
    font-size: 9.5pt;
    min-height: 22px;
    letter-spacing: 0.2px;
}

QPushButton:hover {
    background-color: %11;
}

QPushButton:pressed {
    background-color: %12;
}

QPushButton:focus {
    border: 1px solid %23;
}

QPushButton:disabled {
    background-color: %13;
    color: %14;
    border: 1px solid %15;
}

QPushButton#secondaryBtn,
QPushButton#cancel_btn {
    background-color: transparent;
    color: %5;
    border: 1px solid %3;
    font-weight: 600;
}

QPushButton#secondaryBtn:hover,
QPushButton#cancel_btn:hover {
    background-color: %7;
    color: %1;
    border-color: %16;
}

QPushButton#secondaryBtn:pressed,
QPushButton#cancel_btn:pressed {
    background-color: %3;
    border-color: %16;
}

QPushButton#secondaryBtn:disabled,
QPushButton#cancel_btn:disabled {
    background-color: transparent;
    color: %14;
    border-color: %15;
}

QPushButton#ghostBtn {
    background-color: transparent;
    color: %5;
    border: none;
    padding: 6px 14px;
    font-weight: 500;
}

QPushButton#ghostBtn:hover {
    color: %1;
    background-color: %7;
}

QPushButton#ghostBtn:pressed {
    background-color: %3;
}

QPushButton#dangerBtn {
    background-color: transparent;
    color: %8;
    border: 1px solid %8;
    font-weight: 600;
}

QPushButton#dangerBtn:hover {
    background-color: %18;
    border-color: %11;
    color: %11;
}

QPushButton#dangerBtn:pressed {
    background-color: %8;
    color: %9;
}

QLineEdit, QComboBox, QSpinBox, QDateEdit {
    background-color: %17;
    color: %1;
    border: 1px solid %3;
    border-radius: 6px;
    padding: 6px 10px;
    min-height: 22px;
    selection-background-color: %18;
    selection-color: %1;
}

QLineEdit:hover, QComboBox:hover,
QSpinBox:hover, QDateEdit:hover {
    border-color: %16;
}

QLineEdit:focus, QComboBox:focus,
QSpinBox:focus, QDateEdit:focus {
    border-color: %23;
    background-color: %17;
}

QLineEdit:disabled, QComboBox:disabled,
QSpinBox:disabled, QDateEdit:disabled {
    background-color: %13;
    color: %25;
    border-color: %15;
}

QLineEdit[readOnly="true"] {
    background-color: %13;
    color: %5;
    border-color: %20;
}

QLineEdit QToolButton {
    background: transparent;
    border: none;
    padding: 0 2px;
    border-radius: 3px;
    color: %5;
}

QLineEdit QToolButton:hover {
    background-color: %7;
    color: %1;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 28px;
    border-left: 1px solid %3;
    background: transparent;
    border-top-right-radius: 5px;
    border-bottom-right-radius: 5px;
}

QComboBox::down-arrow {
    image: url(:/icons/arrow-down.svg);
    width: 10px;
    height: 10px;
}

QComboBox QAbstractItemView {
    background-color: %19;
    border: 1px solid %16;
    color: %1;
    selection-background-color: %7;
    selection-color: %1;
    padding: 4px;
    outline: none;
    border-radius: 6px;
}

QComboBox QAbstractItemView::item {
    padding: 6px 12px;
    border-radius: 4px;
    min-height: 24px;
}

QComboBox QAbstractItemView::item:hover {
    background-color: %7;
}

QSpinBox::up-button, QDateEdit::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 22px;
    border-left: 1px solid %3;
    background: transparent;
    border-top-right-radius: 5px;
}

QSpinBox::down-button, QDateEdit::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 22px;
    border-left: 1px solid %3;
    background: transparent;
    border-bottom-right-radius: 5px;
}

QSpinBox::up-arrow, QDateEdit::up-arrow {
    image: url(:/icons/arrow-up.svg);
    width: 8px; height: 8px;
}

QSpinBox::down-arrow, QDateEdit::down-arrow {
    image: url(:/icons/arrow-down.svg);
    width: 8px; height: 8px;
}

QSpinBox::up-button:hover, QSpinBox::down-button:hover,
QDateEdit::up-button:hover, QDateEdit::down-button:hover {
    background-color: %7;
}

QSpinBox::up-button:pressed, QSpinBox::down-button:pressed,
QDateEdit::up-button:pressed, QDateEdit::down-button:pressed {
    background-color: %3;
}

QRadioButton {
    spacing: 8px;
    color: %1;
    background: transparent;
}

QRadioButton:disabled {
    color: %25;
}

QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border: none;
    background: transparent;
    image: url(:/icons/radio-unchecked.svg);
}

QRadioButton::indicator:checked {
    image: url(:/icons/radio-checked.svg);
}

QRadioButton::indicator:disabled {
    opacity: 0.4;
}

QCheckBox {
    spacing: 8px;
    color: %1;
    background: transparent;
}

QCheckBox:disabled {
    color: %25;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1.5px solid %16;
    border-radius: 4px;
    background: transparent;
}

QCheckBox::indicator:hover {
    border-color: %5;
}

QCheckBox::indicator:checked {
    border-color: %8;
    background-color: %8;
    image: url(:/icons/check-white.svg);
}

QCheckBox::indicator:indeterminate {
    border-color: %22;
    background-color: %22;
}

QCheckBox::indicator:disabled {
    border-color: %15;
    background-color: %13;
}

QTextEdit, QTextBrowser {
    background-color: %17;
    color: %1;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 8px 10px;
    font-family: 'Cascadia Code', 'JetBrains Mono', 'Consolas', monospace;
    font-size: 9pt;
    selection-background-color: %18;
    selection-color: %1;
    line-height: 1.5;
}

QTextEdit:hover, QTextBrowser:hover {
    border-color: %16;
}

QTextEdit:focus, QTextBrowser:focus {
    border-color: %23;
}

QTextBrowser a {
    color: %8;
    text-decoration: none;
    font-weight: 600;
}

QTextBrowser a:hover {
    text-decoration: underline;
}

QProgressBar {
    background-color: %20;
    border: none;
    border-radius: 100px;
    text-align: center;
    color: transparent;
    min-height: 4px;
    max-height: 4px;
}

QProgressBar::chunk {
    background-color: %8;
    border-radius: 100px;
}

QProgressBar[labeled="true"] {
    max-height: 18px;
    min-height: 18px;
    border-radius: 4px;
    background-color: %17;
    border: 1px solid %3;
    color: %5;
    font-size: 7.5pt;
    font-weight: 700;
    letter-spacing: 0.4px;
}

QProgressBar[labeled="true"]::chunk {
    border-radius: 3px;
}

QListWidget {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 4px;
    outline: none;
}

QListWidget::item {
    color: %1;
    padding: 7px 12px;
    border-radius: 5px;
    margin: 1px 2px;
    min-height: 20px;
}

QListWidget::item:hover {
    background-color: %7;
}

QListWidget::item:selected {
    background-color: %18;
    color: %1;
    border-left: 2px solid %8;
}

QListWidget::item:selected:hover {
    background-color: %7;
}

QTreeWidget {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 4px;
    outline: none;
    font-size: 9pt;
}

QTreeWidget::item {
    color: %1;
    padding: 4px 4px;
    border-radius: 4px;
    margin: 1px 2px;
    min-height: 22px;
}

QTreeWidget::item:hover {
    background-color: %7;
}

QTreeWidget#timestampTree::item:hover {
    background-color: transparent;
}

QTreeWidget::item:selected {
    background-color: %18;
    color: %1;
}

QTreeWidget::item:selected:hover {
    background-color: %18;
}

QTreeWidget::branch {
    background: transparent;
}

QTreeWidget::indicator {
    width: 15px; height: 15px;
    border: none;
    background: transparent;
}

QTreeWidget::indicator:unchecked       { image: url(:/icons/checkbox-unchecked.svg); }
QTreeWidget::indicator:unchecked:hover { image: url(:/icons/checkbox-unchecked.svg); }
QTreeWidget::indicator:checked         { image: url(:/icons/checkbox-checked.svg); }
QTreeWidget::indicator:indeterminate   { image: url(:/icons/checkbox-indeterminate.svg); }

QHeaderView {
    background-color: %10;
    border: none;
    border-bottom: 1px solid %3;
}

QHeaderView::section {
    background-color: %10;
    color: %6;
    border: none;
    border-right: 1px solid %3;
    padding: 5px 10px;
    font-size: 8pt;
    font-weight: 700;
    letter-spacing: 0.6px;
    text-transform: uppercase;
}

QHeaderView::section:hover {
    background-color: %7;
    color: %1;
}

QHeaderView::section:first {
    border-top-left-radius: 7px;
}

QHeaderView::section:last {
    border-right: none;
    border-top-right-radius: 7px;
}

QTableWidget {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 8px;
    gridline-color: %20;
    outline: none;
    font-size: 9pt;
    selection-background-color: %18;
    selection-color: %1;
}

QTableWidget::item {
    color: %1;
    padding: 5px 10px;
    border: none;
}

QTableWidget::item:hover {
    background-color: %7;
}

QTableWidget::item:selected {
    background-color: %18;
    color: %1;
}

QSplitter::handle {
    background-color: transparent;
}

QSplitter::handle:horizontal {
    width: 1px;
}

QSplitter::handle:vertical {

}

QSplitter::handle:hover {
    background-color: %8;
}

QScrollBar:vertical {
    background: transparent;
    width: 6px;
    margin: 4px 0;
    border-radius: 3px;
}

QScrollBar::handle:vertical {
    background: %3;
    min-height: 32px;
    border-radius: 3px;
}

QScrollBar::handle:vertical:hover   { background: %16; }
QScrollBar::handle:vertical:pressed { background: %5; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

QScrollBar:horizontal {
    background: transparent;
    height: 6px;
    margin: 0 4px;
    border-radius: 3px;
}

QScrollBar::handle:horizontal {
    background: %3;
    min-width: 32px;
    border-radius: 3px;
}

QScrollBar::handle:horizontal:hover   { background: %16; }
QScrollBar::handle:horizontal:pressed { background: %5; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

QToolTip {
    background-color: %19;
    color: %1;
    border: 1px solid %16;
    padding: 4px 10px;
    border-radius: 5px;
    font-size: 8.5pt;
    font-weight: 500;
}

QMenuBar {
    background-color: %4;
    color: %5;
    border-bottom: 1px solid %20;
    padding: 2px 4px;
}

QMenuBar::item {
    background: transparent;
    padding: 4px 10px;
    border-radius: 4px;
}

QMenuBar::item:selected,
QMenuBar::item:pressed {
    background-color: %7;
    color: %1;
}

QMenu {
    background-color: %19;
    border: 1px solid %16;
    border-radius: 8px;
    padding: 5px;
}

QMenu::item {
    color: %1;
    padding: 7px 32px 7px 14px;
    border-radius: 5px;
    min-width: 140px;
}

QMenu::item:selected {
    background-color: %7;
    color: %1;
}

QMenu::item:disabled {
    color: %25;
}

QMenu::separator {
    height: 1px;
    background-color: %3;
    margin: 4px 8px;
}

QMenu::indicator {
    width: 14px;
    height: 14px;
    margin-left: 6px;
}

QSlider::groove:horizontal {
    height: 4px;
    background: %3;
    border-radius: 2px;
    margin: 0 4px;
}

QSlider::handle:horizontal {
    background: %8;
    border: none;
    width: 14px;
    height: 14px;
    margin: -5px -2px;
    border-radius: 7px;
}

QSlider::handle:horizontal:hover {
    background: %11;
    width: 16px;
    height: 16px;
    margin: -6px -3px;
    border-radius: 8px;
}

QSlider::sub-page:horizontal {
    background: %8;
    border-radius: 2px;
}

QSlider::groove:vertical {
    width: 4px;
    background: %3;
    border-radius: 2px;
    margin: 4px 0;
}

QSlider::handle:vertical {
    background: %8;
    border: none;
    width: 14px;
    height: 14px;
    margin: -2px -5px;
    border-radius: 7px;
}

QSlider::add-page:vertical {
    background: %8;
    border-radius: 2px;
}

QStatusBar {
    background-color: %4;
    color: %6;
    border-top: 1px solid %20;
    font-size: 8.5pt;
    padding: 0 8px;
    min-height: 22px;
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
    border-radius: 10px;
}

QDialogButtonBox QPushButton {
    min-width: 80px;
}

QTextEdit#logDisplay {
    background-color: %17;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 10px 12px;
    font-size: 8.5pt;
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
    border-radius: 10px;
    padding: 16px 18px;
    font-size: 10pt;
    line-height: 1.7;
}

QLabel#videoPlaceholder {
    background-color: %10;
    border: 2px solid %8;
    border-radius: 10px;
    color: %6;
    font-size: 10pt;
    padding: 12px;
}

QLabel#thumbnailPreview {
    background-color: %10;
    border: 2px dashed %16;
    border-radius: 10px;
    color: %6;
    font-size: 10pt;
    padding: 12px;
}

QLabel#infoBanner {
    background-color: %18;
    border: 1px solid %8;
    border-left: 3px solid %8;
    border-radius: 6px;
    color: %1;
    padding: 8px 12px;
    font-size: 9pt;
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
)")
        .arg(TextPrimary)
        .arg(BgBase)
        .arg(Border)
        .arg(BgSurface)
        .arg(TextSecondary)
        .arg(TextMuted)
        .arg(BgHover)
        .arg(Accent)
        .arg(TextOnAccent)
        .arg(BgCard)
        .arg(AccentHover)
        .arg(AccentPress)
        .arg(DisabledBg)
        .arg(DisabledFg)
        .arg(DisabledBdr)
        .arg(BorderStrong)
        .arg(BgInput)
        .arg(AccentSubtle)
        .arg(BgElevated)
        .arg(BorderSubtle)
        .arg(AccentGlow)
        .arg(AccentMid)
        .arg(BorderFocus)
        .arg(DonateHover)
        .arg(TextDisabled)
        .arg(BgOverlay);

    return qss;
}

}