#pragma once
#ifndef CAPSCRIPT_DONATEBUTTON_H
#define CAPSCRIPT_DONATEBUTTON_H

#include <QPropertyAnimation>
#include <QTimer>
#include <QToolButton>

namespace CapScript {

class DonateButton : public QToolButton {
  Q_OBJECT
  Q_PROPERTY(qreal iconScale READ iconScale WRITE setIconScale)

public:
  explicit DonateButton(QWidget *parent = nullptr);
  void setCollapsed(bool collapsed);

  qreal iconScale() const { return m_iconScale; }
  void setIconScale(qreal s);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QPropertyAnimation *m_pulseAnim = nullptr;
  qreal m_iconScale = 1.0;
  QSize m_baseIconSize{20, 20};
};

}

#endif
