#include "Sidebar.h"
#include "widgets/DonateButton.h"
#include "widgets/GitHubButton.h"

#include <QParallelAnimationGroup>
#include <QSpacerItem>
#include <QToolButton>

namespace CapScript {

Sidebar::Sidebar(QWidget *parent) : QWidget(parent) {
  setObjectName("sidebar");
  setFixedWidth(EXPANDED_WIDTH);
  setMinimumWidth(COLLAPSED_WIDTH);
  setMaximumWidth(EXPANDED_WIDTH);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(4, 8, 4, 8);
  m_layout->setSpacing(4);

  m_toggleBtn = new QToolButton(this);
  m_toggleBtn->setObjectName("toggleSidebarButton");
  m_toggleBtn->setText("☰  CapScript");
  m_toggleBtn->setCheckable(true);
  m_toggleBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_toggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_toggleBtn->setFixedHeight(40);
  connect(m_toggleBtn, &QToolButton::toggled, this, &Sidebar::onToggle);
  m_layout->addWidget(m_toggleBtn);

  auto *sep = new QFrame(this);
  sep->setFrameShape(QFrame::HLine);
  sep->setObjectName("SidebarSeparator");
  m_layout->addWidget(sep);

  m_animMin = new QPropertyAnimation(this, "minimumWidth", this);
  m_animMin->setDuration(200);
  m_animMax = new QPropertyAnimation(this, "maximumWidth", this);
  m_animMax->setDuration(200);
}

void Sidebar::addNavButton(int pageIndex, const QString &text,
                           const QString &iconPath) {
  auto *btn = new QToolButton(this);
  btn->setObjectName("navButton");
  btn->setText(text);
  if (!iconPath.isEmpty())
    btn->setIcon(QIcon(iconPath));
  btn->setIconSize(QSize(24, 24));
  btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  btn->setCheckable(true);
  btn->setAutoExclusive(true);
  btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btn->setFixedHeight(40);

  connect(btn, &QToolButton::clicked, this,
          [this, pageIndex]() { emit pageChangeRequested(pageIndex); });

  m_navButtons.insert(pageIndex, btn);
  m_layout->addWidget(btn);
}

void Sidebar::finalize() {
  m_layout->addStretch(1);

  m_githubBtn = new GitHubButton(this);
  connect(m_githubBtn, &GitHubButton::clicked, this, &Sidebar::githubClicked);
  m_layout->addWidget(m_githubBtn);

  m_donateBtn = new DonateButton(this);
  connect(m_donateBtn, &DonateButton::clicked, this, &Sidebar::donateClicked);
  m_layout->addWidget(m_donateBtn);

  m_versionLabel = new QLabel("v2.0", this);
  m_versionLabel->setObjectName("versionLabel");
  m_versionLabel->setAlignment(Qt::AlignCenter);
  m_layout->addWidget(m_versionLabel);

  if (!m_navButtons.isEmpty())
    m_navButtons.first()->setChecked(true);
}

void Sidebar::setCollapsed(bool collapsed, bool animate) {
  if (m_collapsed == collapsed)
    return;
  m_collapsed = collapsed;
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(collapsed);
  m_toggleBtn->blockSignals(false);

  int targetWidth = collapsed ? COLLAPSED_WIDTH : EXPANDED_WIDTH;

  if (animate) {
    m_animMin->stop();
    m_animMax->stop();
    m_animMin->setStartValue(minimumWidth());
    m_animMin->setEndValue(targetWidth);
    m_animMax->setStartValue(maximumWidth());
    m_animMax->setEndValue(targetWidth);
    m_animMin->start();
    m_animMax->start();
  } else {
    setFixedWidth(targetWidth);
  }

  updateButtonStates(collapsed);
}

void Sidebar::onToggle(bool checked) { setCollapsed(checked); }

void Sidebar::updateButtonStates(bool collapsed) {
  for (auto *btn : m_navButtons) {
    btn->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly
                                      : Qt::ToolButtonTextBesideIcon);
  }

  m_toggleBtn->setText(collapsed ? "☰" : "☰  CapScript");
  m_toggleBtn->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly
                                            : Qt::ToolButtonTextBesideIcon);

  m_versionLabel->setVisible(!collapsed);
  m_donateBtn->setCollapsed(collapsed);
  m_githubBtn->setCollapsed(collapsed);
}

}
