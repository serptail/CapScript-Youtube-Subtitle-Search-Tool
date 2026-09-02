#include "AboutPage.h"
#include "../../core/UrlLauncher.h"
#include <QApplication>
#include <QDate>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr const char *kRepoUrl =
    "https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool";
}

namespace CapScript {

AboutPage::AboutPage(QWidget *parent) : QWidget(parent) { setupUi(); }

void AboutPage::setupUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 16, 24, 16);
  root->setSpacing(14);

  auto *hero = new QHBoxLayout;
  hero->setSpacing(14);

  auto *logo = new QLabel(this);
  logo->setPixmap(QPixmap(":/icons/app_icon.png")
                      .scaled(56, 56, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
  hero->addWidget(logo, 0, Qt::AlignTop);

  auto *heroText = new QVBoxLayout;
  heroText->setSpacing(2);

  auto *title = new QLabel("CapScript Pro", this);
  title->setObjectName("pageHeading");
  heroText->addWidget(title);

  auto *subtitle =
      new QLabel("YouTube subtitle search, clipping, and rendering.", this);
  subtitle->setProperty("secondary", true);
  heroText->addWidget(subtitle);

  hero->addLayout(heroText, 1);
  root->addLayout(hero);

  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(14);

  // -- About card --
  auto *aboutGroup = new QGroupBox("About", this);
  aboutGroup->setObjectName("aboutGroup");

  auto *aboutLayout = new QVBoxLayout(aboutGroup);
  aboutLayout->setContentsMargins(12, 22, 12, 12);
  aboutLayout->setSpacing(10);

  auto *metaGrid = new QGridLayout;
  metaGrid->setSpacing(6);
  metaGrid->setColumnStretch(1, 1);

  const QString appVersion =
      qApp ? qApp->applicationVersion() : QStringLiteral("unknown");

  auto addMetaRow = [metaGrid](int row, const QString &key,
                                const QString &value) {
    auto *k = new QLabel(key);
    k->setProperty("muted", true);
    metaGrid->addWidget(k, row, 0);

    auto *v = new QLabel(value);
    v->setProperty("secondary", true);
    v->setWordWrap(true);
    metaGrid->addWidget(v, row, 1);
  };

  addMetaRow(0, "Version:", appVersion);
  addMetaRow(1, "Author:", "~serptail");
  addMetaRow(2, "License:", "MIT + Commons Clause");

  aboutLayout->addLayout(metaGrid);

  auto *licenseHint = new QLabel(
      "Source-available. Viewing and modification permitted — "
      "selling or redistribution is not.",
      aboutGroup);
  licenseHint->setObjectName("infoBanner");
  licenseHint->setWordWrap(true);
  aboutLayout->addWidget(licenseHint);

  auto *githubBtn = new QPushButton("View source on GitHub", aboutGroup);
  githubBtn->setObjectName("secondaryBtn");
  githubBtn->setCursor(Qt::PointingHandCursor);
  connect(githubBtn, &QPushButton::clicked, this,
          []() { openExternalUrl(QUrl(QString::fromUtf8(kRepoUrl))); });
  aboutLayout->addWidget(githubBtn, 0, Qt::AlignLeft);

  aboutLayout->addStretch(1);

  topRow->addWidget(aboutGroup, 1);

  auto *updateGroup = new QGroupBox("Software Updates", this);
  updateGroup->setObjectName("aboutGroup");

  auto *updateLayout = new QVBoxLayout(updateGroup);
  updateLayout->setContentsMargins(12, 22, 12, 12);
  updateLayout->setSpacing(10);

  m_statusLabel = new QLabel(
      "Press \"Check for updates\" to query the latest release.",
      updateGroup);
  m_statusLabel->setProperty("secondary", true);
  m_statusLabel->setWordWrap(true);
  updateLayout->addWidget(m_statusLabel);

  auto *progressRow = new QHBoxLayout;
  progressRow->setSpacing(10);

  m_updateProgressBar = new QProgressBar(updateGroup);
  m_updateProgressBar->setRange(0, 100);
  m_updateProgressBar->setValue(0);
  m_updateProgressBar->setTextVisible(false);
  m_updateProgressBar->setVisible(false);
  progressRow->addWidget(m_updateProgressBar, 1);

  m_updateProgressLabel = new QLabel("0%", updateGroup);
  m_updateProgressLabel->setObjectName("progressLabel");
  m_updateProgressLabel->setVisible(false);
  progressRow->addWidget(m_updateProgressLabel, 0);

  updateLayout->addLayout(progressRow);

  updateLayout->addStretch(1);

  m_checkUpdatesBtn = new QPushButton("Check for updates", updateGroup);
  m_checkUpdatesBtn->setMinimumHeight(34);
  connect(m_checkUpdatesBtn, &QPushButton::clicked, this,
          &AboutPage::checkForUpdatesRequested);
  updateLayout->addWidget(m_checkUpdatesBtn, 0, Qt::AlignLeft);

  topRow->addWidget(updateGroup, 1);

  root->addLayout(topRow, 1);

  const int year = QDate::currentDate().year();
  auto *footer =
      new QLabel(QString("%1 Serptail. All rights reserved.").arg(year), this);
  footer->setProperty("muted", true);
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