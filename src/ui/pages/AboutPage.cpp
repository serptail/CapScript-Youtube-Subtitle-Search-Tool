#include "AboutPage.h"
#include "../../core/UrlLauncher.h"
#include <QApplication>
#include <QDate>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRadialGradient>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr const char *kRepoUrl =
    "https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool";
}

namespace CapScript {

AboutPage::AboutPage(QWidget *parent) : QWidget(parent) {
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setupUi();
}

void AboutPage::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(0x0a, 0x0a, 0x0a));

  const int w = width();
  const int h = height();

  QRadialGradient glow(w * 0.5, -80.0, w * 0.9);
  glow.setColorAt(0.0, QColor(255, 0, 51, 48));
  glow.setColorAt(0.5, QColor(255, 0, 51, 12));
  glow.setColorAt(1.0, QColor(255, 0, 51, 0));

  p.save();
  p.setClipRect(0, 0, w, h);
  p.fillRect(rect(), QBrush(glow));
  p.restore();
}

void AboutPage::setupUi() {
  setObjectName("aboutPage");
  setStyleSheet("QWidget#aboutPage { background: transparent; }"
                "QPushButton#primaryBtn {"
                " background-color: #dc0f2c;"
                " color: #ffffff;"
                " border: none;"
                " border-radius: 7px;"
                " font-size: 10pt;"
                " font-weight: 700;"
                " letter-spacing: 0.4px;"
                " padding: 0 24px;"
                " outline: none;"
                "}"
                "QPushButton#primaryBtn:hover {"
                " background-color: #ff1744;"
                "}"
                "QPushButton#primaryBtn:pressed {"
                " background-color: #b00820;"
                "}"
                "QPushButton#primaryBtn:disabled {"
                " background-color: #5a2330;"
                " color: #a06d7e;"
                "}"
                "QPushButton#secondaryBtn {"
                " background-color: #0f0f0f;"
                " color: #e0e0e0;"
                " border: 1.5px solid #3a3a3a;"
                " border-radius: 7px;"
                " font-size: 10pt;"
                " font-weight: 600;"
                " letter-spacing: 0.4px;"
                " padding: 0 24px;"
                " outline: none;"
                "}"
                "QPushButton#secondaryBtn:hover {"
                " background-color: #1f1f1f;"
                " border-color: #ff0033;"
                " color: #ffffff;"
                "}"
                "QPushButton#secondaryBtn:pressed {"
                " background-color: #0a0a0a;"
                " border-color: #ff1744;"
                " color: #ff1744;"
                "}");

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 14);
  root->setSpacing(10);

  root->addStretch(1);

  auto *logo = new QLabel(this);
  logo->setAlignment(Qt::AlignCenter);
  logo->setPixmap(
      QPixmap(":/icons/app_icon.png")
          .scaled(132, 132, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  root->addWidget(logo, 0, Qt::AlignHCenter);

  auto *title = new QLabel("CapScript Pro", this);
  title->setAlignment(Qt::AlignCenter);
  title->setStyleSheet("font-family: 'Akrobat Black', sans-serif;"
                       "font-size: 28pt;"
                       "font-weight: 900;"
                       "letter-spacing: 1.2px;"
                       "color: #f5f5f5;"
                       "background: transparent;");
  title->setMinimumHeight(50);
  root->addWidget(title);

  auto *subtitle = new QLabel("YouTube Subtitle Search Tool", this);
  subtitle->setAlignment(Qt::AlignCenter);
  subtitle->setStyleSheet("color: #a8a8a8; font-size: 10pt; letter-spacing: "
                          "0.7px; font-weight: 500;");
  root->addWidget(subtitle);

  const QString appVersion =
      qApp ? qApp->applicationVersion() : QStringLiteral("unknown");

  auto *body = new QLabel(this);
  body->setAlignment(Qt::AlignCenter);
  body->setWordWrap(true);
  body->setOpenExternalLinks(false);
  body->setTextInteractionFlags(Qt::TextBrowserInteraction);
  body->setText(
      QString(
          "<span style='color:#bfbfbf;'>Author</span>: <span "
          "style='color:#f0f0f0;'>~serptail</span><br/>"
          "<span style='color:#bfbfbf;'>Version</span>: <span "
          "style='color:#f0f0f0;'>%1</span><br/>"
          "<span style='color:#bfbfbf;'>License</span>: <span "
          "style='color:#f0f0f0;'>MIT + Commons Clause v1.0</span><br/><br/>"
          "<span style='color:#a6a6a6;'>Source-available software.<br/>"
          "Viewing and modification permitted.<br/>"
          "Selling or redistribution prohibited.</span>")
          .arg(appVersion));
  body->setStyleSheet(
      "font-size: 10pt; line-height: 1.5; color: #c5c5c5; font-weight: 500;");
  root->addWidget(body, 0, Qt::AlignHCenter);

  m_statusLabel = new QLabel(
      "Press \"Check for updates\" to query the latest release.", this);
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_statusLabel->setStyleSheet(
      "color: #9a9a9a; font-size: 9.5pt; font-weight: 500;");
  root->addWidget(m_statusLabel, 0, Qt::AlignHCenter);

  root->addStretch(1);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->addStretch(1);

  m_checkUpdatesBtn = new QPushButton("Check for updates", this);
  m_checkUpdatesBtn->setObjectName("primaryBtn");
  m_checkUpdatesBtn->setMinimumWidth(170);
  m_checkUpdatesBtn->setFixedHeight(40);
  connect(m_checkUpdatesBtn, &QPushButton::clicked, this,
          &AboutPage::checkForUpdatesRequested);
  buttonRow->addWidget(m_checkUpdatesBtn);

  auto *githubBtn = new QPushButton("GitHub Repository", this);
  githubBtn->setMinimumWidth(170);
  githubBtn->setFixedHeight(40);
  githubBtn->setObjectName("secondaryBtn");
  connect(githubBtn, &QPushButton::clicked, this,
          []() { openExternalUrl(QUrl(QString::fromUtf8(kRepoUrl))); });
  buttonRow->addWidget(githubBtn);

  buttonRow->addStretch(1);
  root->addLayout(buttonRow);
  root->addSpacing(16);

  auto *progressContainer = new QWidget(this);
  auto *progressLayout = new QVBoxLayout(progressContainer);
  progressLayout->setContentsMargins(0, 0, 0, 0);
  progressLayout->setSpacing(4);

  m_updateProgressLabel = new QLabel(this);
  m_updateProgressLabel->setAlignment(Qt::AlignCenter);
  m_updateProgressLabel->setStyleSheet(
      "color: #f0f0f0; font-weight: bold; font-size: 10pt;");
  m_updateProgressLabel->setVisible(false);
  m_updateProgressLabel->setText("0%");
  progressLayout->addWidget(m_updateProgressLabel);

  m_updateProgressBar = new QProgressBar(this);
  m_updateProgressBar->setRange(0, 100);
  m_updateProgressBar->setValue(0);
  m_updateProgressBar->setTextVisible(false);
  m_updateProgressBar->setFixedHeight(8);
  m_updateProgressBar->setVisible(false);
  m_updateProgressBar->setStyleSheet(
      "QProgressBar { background-color: #141414; border: 1px solid #2a2a2a; "
      "border-radius: 3px; }"
      "QProgressBar::chunk { background-color: #FF0033; border-radius: 2px; }");
  progressLayout->addWidget(m_updateProgressBar);

  root->addWidget(progressContainer);

  root->addStretch(1);

  const int year = QDate::currentDate().year();
  auto *footer =
      new QLabel(QString("%1 Serptail. All rights reserved.").arg(year), this);
  footer->setAlignment(Qt::AlignCenter);
  footer->setStyleSheet("color: #5a5a5a; font-size: 9pt; letter-spacing: "
                        "0.5px; font-weight: 500;");
  root->addWidget(footer);
}

void AboutPage::setUpdateStatus(const QString &status) {
  if (m_statusLabel)
    m_statusLabel->setText(status);
}

void AboutPage::setUpdateProgressVisible(bool visible) {
  if (m_updateProgressBar)
    m_updateProgressBar->setVisible(visible);
  if (m_updateProgressLabel)
    m_updateProgressLabel->setVisible(visible);
}

void AboutPage::setUpdateProgress(int percent) {
  if (!m_updateProgressBar)
    return;

  percent = qBound(0, percent, 100);
  m_updateProgressBar->setRange(0, 100);
  m_updateProgressBar->setValue(percent);

  if (m_updateProgressLabel)
    m_updateProgressLabel->setText(QString("%1%").arg(percent));
}

void AboutPage::setBusy(bool busy) {
  if (m_checkUpdatesBtn)
    m_checkUpdatesBtn->setEnabled(!busy);
}

}
