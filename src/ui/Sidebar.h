#pragma once
#ifndef CAPSCRIPT_SIDEBAR_H
#define CAPSCRIPT_SIDEBAR_H

#include <QLabel>
#include <QMap>
#include <QPropertyAnimation>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace CapScript {

class DonateButton;
class GitHubButton;
class Sidebar : public QWidget {
  Q_OBJECT

public:
  static constexpr int EXPANDED_WIDTH = 180;
  static constexpr int COLLAPSED_WIDTH = 50;

  explicit Sidebar(QWidget *parent = nullptr);

  void addNavButton(int pageIndex, const QString &text,
                    const QString &iconPath);
  void finalize();
  void setCollapsed(bool collapsed, bool animate = true);
  bool isCollapsed() const { return m_collapsed; }

signals:
  void pageChangeRequested(int pageIndex);
  void donateClicked();
  void githubClicked();

private slots:
  void onToggle(bool checked);

private:
  void updateButtonStates(bool collapsed);

  QVBoxLayout *m_layout;
  QToolButton *m_toggleBtn;
  QMap<int, QToolButton *> m_navButtons;
  DonateButton *m_donateBtn;
  GitHubButton *m_githubBtn;
  QLabel *m_versionLabel;
  bool m_collapsed = false;

  QPropertyAnimation *m_animMin = nullptr;
  QPropertyAnimation *m_animMax = nullptr;
};

}

#endif
