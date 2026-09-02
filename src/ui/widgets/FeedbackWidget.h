#pragma once
#ifndef CAPSCRIPT_FEEDBACKWIDGET_H
#define CAPSCRIPT_FEEDBACKWIDGET_H

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

class QPropertyAnimation;
class QNetworkAccessManager;
class QNetworkReply;

namespace CapScript {

class HeartWidget : public QWidget {
  Q_OBJECT
public:
  explicit HeartWidget(int index, QWidget *parent = nullptr);
  int index() const { return m_index; }
  void setFilled(bool filled);

signals:
  void hovered(int index);
  void clicked(int index);
  void unhovered();

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  int m_index;
  bool m_filled = false;
};

class FeedbackPanel : public QWidget {
  Q_OBJECT
public:
  explicit FeedbackPanel(QWidget *parent = nullptr);
  void togglePanel();
  void showPanel();
  void hidePanel();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onHeartHovered(int index);
  void onHeartClicked(int index);
  void onHeartUnhovered();
  bool isValidEmail(const QString &email);
  void onSend();
  void onNetworkReply(QNetworkReply *reply);

private:
  void updateHearts();
  void animatePanel(bool open);
  void reanchor();

  bool m_isOpen = false;
  int m_hoveredIndex = -1;
  int m_selectedIndex = -1;

  QLabel *m_titleLabel = nullptr;
  QLabel *m_statusLabel = nullptr;
  QList<HeartWidget *> m_hearts;
  QLineEdit *m_emailInput = nullptr;
  QTextEdit *m_feedbackText = nullptr;
  QPushButton *m_sendBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;

  QNetworkAccessManager *m_networkManager = nullptr;
  QNetworkReply *m_pendingReply = nullptr;
};

}
#endif
