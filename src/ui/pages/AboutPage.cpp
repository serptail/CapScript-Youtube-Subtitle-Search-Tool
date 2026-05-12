#include "AboutPage.h"
#include "../../core/UrlLauncher.h"
#include <QApplication>
#include <QDate>
#include <QGridLayout>
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
                "QWidget#aboutInfoCard, QWidget#updateCard {"
                " background-color: rgba(16, 16, 16, 0.9);"
                " border: 1px solid #2a2a2a;"
                " border-radius: 11px;"
                "}"
                "QLabel#heroTitle {"
                " font-family: 'Akrobat Black', sans-serif;"
                " font-size: 28pt;"
                " font-weight: 900;"
                " letter-spacing: 1.2px;"
                " color: #f5f5f5;"
                " background: transparent;"
                "}"
                "QLabel#heroSubtitle {"
                " color: #a8a8a8;"
                " font-size: 10pt;"
                " letter-spacing: 0.7px;"
                " font-weight: 500;"
                "}"
                "QLabel#sectionTitle {"
                " color: #ebebeb;"
                " font-size: 10pt;"
                " letter-spacing: 0.8px;"
                " font-weight: 700;"
                " text-transform: uppercase;"
                "}"
                "QLabel#metaLabel {"
                " color: #9a9a9a;"
                " font-size: 9pt;"
                " font-weight: 600;"
                " letter-spacing: 0.5px;"
                "}"
                "QLabel#metaValue {"
                " color: #f0f0f0;"
                " font-size: 9.5pt;"
                " font-weight: 600;"
                "}"
                "QLabel#licenseHint {"
                " color: #aaaaaa;"
                " font-size: 9pt;"
                " font-weight: 500;"
                " line-height: 1.5;"
                " padding-top: 2px;"
                "}"
                "QLabel#statusLabel {"
                " color: #9a9a9a;"
                " font-size: 9.5pt;"
                " font-weight: 500;"
                "}"
                "QLabel#progressLabel {"
                " color: #f0f0f0;"
                " font-weight: 700;"
                " font-size: 10pt;"
                "}"
                "QLabel#footerLabel {"
                " color: #5a5a5a;"
                " font-size: 9pt;"
                " letter-spacing: 0.5px;"
                " font-weight: 500;"
                "}"
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
                "}"
                "QProgressBar#updateProgressBar {"
                " background-color: #141414;"
                " border: 1px solid #2a2a2a;"
                " border-radius: 3px;"
                "}"
                "QProgressBar#updateProgressBar::chunk {"
                " background-color: #FF0033;"
                " border-radius: 2px;"
                "}");

  auto *root = new QVBoxLayout(this);
          root->setContentsMargins(26, 22, 26, 14);
          root->setSpacing(12);

    auto *logo = new QLabel(this);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(
      QPixmap(":/icons/app_icon.png")
        .scaled(108, 108, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    root->addWidget(logo, 0, Qt::AlignHCenter);

    auto *title = new QLabel("CapScript Pro", this);
    title->setObjectName("heroTitle");
    title->setAlignment(Qt::AlignCenter);
    title->setMinimumHeight(44);
    root->addWidget(title);

    auto *subtitle = new QLabel("YouTube Subtitle Search Tool", this);
    subtitle->setObjectName("heroSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(subtitle);

  const QString appVersion =
      qApp ? qApp->applicationVersion() : QStringLiteral("unknown");

          auto *infoCard = new QWidget(this);
          infoCard->setObjectName("aboutInfoCard");
          auto *infoLayout = new QVBoxLayout(infoCard);
          infoLayout->setContentsMargins(16, 14, 16, 14);
          infoLayout->setSpacing(10);

          auto *aboutTitle = new QLabel("About", infoCard);
          aboutTitle->setObjectName("sectionTitle");
          infoLayout->addWidget(aboutTitle);

          auto *metaGrid = new QGridLayout;
          metaGrid->setHorizontalSpacing(18);
          metaGrid->setVerticalSpacing(8);
          metaGrid->setColumnStretch(1, 1);

          auto *authorLabel = new QLabel("Author", infoCard);
          authorLabel->setObjectName("metaLabel");
          auto *authorValue = new QLabel("~serptail", infoCard);
          authorValue->setObjectName("metaValue");
          metaGrid->addWidget(authorLabel, 0, 0);
          metaGrid->addWidget(authorValue, 0, 1);

          auto *versionLabel = new QLabel("Version", infoCard);
          versionLabel->setObjectName("metaLabel");
          auto *versionValue = new QLabel(appVersion, infoCard);
          versionValue->setObjectName("metaValue");
          metaGrid->addWidget(versionLabel, 1, 0);
          metaGrid->addWidget(versionValue, 1, 1);

          auto *licenseLabel = new QLabel("License", infoCard);
          licenseLabel->setObjectName("metaLabel");
          auto *licenseValue = new QLabel("MIT + Commons Clause v1.0", infoCard);
          licenseValue->setObjectName("metaValue");
          metaGrid->addWidget(licenseLabel, 2, 0);
          metaGrid->addWidget(licenseValue, 2, 1);

          infoLayout->addLayout(metaGrid);

          auto *licenseHint = new QLabel(
            "Source-available software. Viewing and modification permitted.\n"
            "Selling or redistribution prohibited.",
            infoCard);
          licenseHint->setObjectName("licenseHint");
          licenseHint->setWordWrap(true);
          infoLayout->addWidget(licenseHint);

          root->addWidget(infoCard);

          auto *updateCard = new QWidget(this);
          updateCard->setObjectName("updateCard");
          auto *updateLayout = new QVBoxLayout(updateCard);
          updateLayout->setContentsMargins(16, 14, 16, 14);
          updateLayout->setSpacing(10);

          auto *updateTitle = new QLabel("Updates", updateCard);
          updateTitle->setObjectName("sectionTitle");
          updateLayout->addWidget(updateTitle);

  m_statusLabel = new QLabel(
            "Press \"Check for updates\" to query the latest release.", updateCard);
          m_statusLabel->setObjectName("statusLabel");
  m_statusLabel->setAlignment(Qt::AlignCenter);
          m_statusLabel->setWordWrap(true);
          updateLayout->addWidget(m_statusLabel);

          auto *buttonRow = new QHBoxLayout;
          buttonRow->setSpacing(10);
          buttonRow->addStretch(1);

          m_checkUpdatesBtn = new QPushButton("Check for updates", updateCard);
  m_checkUpdatesBtn->setObjectName("primaryBtn");
          m_checkUpdatesBtn->setMinimumWidth(182);
  m_checkUpdatesBtn->setFixedHeight(40);
  connect(m_checkUpdatesBtn, &QPushButton::clicked, this,
          &AboutPage::checkForUpdatesRequested);
  buttonRow->addWidget(m_checkUpdatesBtn);

          auto *githubBtn = new QPushButton("GitHub Repository", updateCard);
          githubBtn->setMinimumWidth(182);
  githubBtn->setFixedHeight(40);
  githubBtn->setObjectName("secondaryBtn");
  connect(githubBtn, &QPushButton::clicked, this,
          []() { openExternalUrl(QUrl(QString::fromUtf8(kRepoUrl))); });
  buttonRow->addWidget(githubBtn);

  buttonRow->addStretch(1);
    updateLayout->addLayout(buttonRow);

    auto *progressContainer = new QWidget(updateCard);
  auto *progressLayout = new QVBoxLayout(progressContainer);
  progressLayout->setContentsMargins(0, 0, 0, 0);
  progressLayout->setSpacing(4);

    m_updateProgressLabel = new QLabel(updateCard);
    m_updateProgressLabel->setObjectName("progressLabel");
  m_updateProgressLabel->setAlignment(Qt::AlignCenter);
  m_updateProgressLabel->setVisible(false);
  m_updateProgressLabel->setText("0%");
  progressLayout->addWidget(m_updateProgressLabel);

    m_updateProgressBar = new QProgressBar(updateCard);
    m_updateProgressBar->setObjectName("updateProgressBar");
  m_updateProgressBar->setRange(0, 100);
  m_updateProgressBar->setValue(0);
  m_updateProgressBar->setTextVisible(false);
  m_updateProgressBar->setFixedHeight(8);
  m_updateProgressBar->setVisible(false);
  progressLayout->addWidget(m_updateProgressBar);

    updateLayout->addWidget(progressContainer);

    root->addWidget(updateCard);

  root->addStretch(1);

  const int year = QDate::currentDate().year();
  auto *footer =
      new QLabel(QString("%1 Serptail. All rights reserved.").arg(year), this);
    footer->setObjectName("footerLabel");
  footer->setAlignment(Qt::AlignCenter);
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
