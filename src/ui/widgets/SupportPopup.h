#pragma once

#include <QDesktopServices>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace CapScript {

class SupportPopup : public QWidget {
  Q_OBJECT
public:
  explicit SupportPopup(QWidget *parent = nullptr);
  void showPopup();
  void hidePopup();

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  QLabel *m_iconLabel;
  QLabel *m_titleLabel;
  QLabel *m_descLabel;
  QPushButton *m_supportBtn;
  QPushButton *m_closeBtn;

  QPropertyAnimation *m_opacityAnimation;
  QGraphicsOpacityEffect *m_opacityEffect;
};

}
