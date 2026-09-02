#include "GitHubButton.h"
#include <QIcon>

namespace CapScript {

GitHubButton::GitHubButton(QWidget *parent) : QToolButton(parent) {
  setObjectName("githubButton");
  setText("★  GitHub");
  setIcon(QIcon(":/icons/github.svg"));
  setIconSize(QSize(20, 20));
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFixedHeight(36);
  setCursor(Qt::PointingHandCursor);
}

void GitHubButton::setCollapsed(bool collapsed) {
  setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly
                               : Qt::ToolButtonTextBesideIcon);
}

}
