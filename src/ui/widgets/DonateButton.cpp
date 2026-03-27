#include "DonateButton.h"
#include <QEnterEvent>

namespace CapScript {

DonateButton::DonateButton(QWidget *parent) : QToolButton(parent) {
  setObjectName("donateButton");
  setText("♥  Donate");
  setIcon(QIcon(":/icons/donate.svg"));
  setIconSize(m_baseIconSize);
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFixedHeight(36);
  setCursor(Qt::PointingHandCursor);

  m_pulseAnim = new QPropertyAnimation(this, "iconScale", this);
  m_pulseAnim->setDuration(600);
  m_pulseAnim->setStartValue(1.0);
  m_pulseAnim->setKeyValueAt(0.5, 1.3);
  m_pulseAnim->setEndValue(1.0);
  m_pulseAnim->setLoopCount(-1);
}

void DonateButton::setCollapsed(bool collapsed) {
  setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly
                               : Qt::ToolButtonTextBesideIcon);
}

void DonateButton::setIconScale(qreal s) {
  m_iconScale = s;
  int sz = static_cast<int>(m_baseIconSize.width() * s);
  setIconSize(QSize(sz, sz));
}

void DonateButton::enterEvent(QEnterEvent *event) {
  m_pulseAnim->start();
  QToolButton::enterEvent(event);
}

void DonateButton::leaveEvent(QEvent *event) {
  m_pulseAnim->stop();
  setIconScale(1.0);
  QToolButton::leaveEvent(event);
}

}
