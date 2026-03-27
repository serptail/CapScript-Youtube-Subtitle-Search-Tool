#pragma once
#ifndef CAPSCRIPT_TITLEBAR_H
#define CAPSCRIPT_TITLEBAR_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace CapScript {

class TitleBar : public QWidget {
  Q_OBJECT

public:
  explicit TitleBar(QWidget *parent = nullptr);

  void setTitle(const QString &title);

signals:
  void minimizeClicked();
  void maximizeClicked();
  void closeClicked();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
  QLabel *m_appIconLabel = nullptr;
  QLabel *m_titleLabel = nullptr;
  QPushButton *m_githubBtn = nullptr;
  QPushButton *m_donateBtn = nullptr;
  QPushButton *m_minimizeBtn = nullptr;
  QPushButton *m_maximizeBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;

  QPoint m_dragStart;
  bool m_dragging = false;
};

}

#endif
