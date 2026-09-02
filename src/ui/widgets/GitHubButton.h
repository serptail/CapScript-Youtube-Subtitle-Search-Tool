#pragma once
#ifndef CAPSCRIPT_GITHUBBUTTON_H
#define CAPSCRIPT_GITHUBBUTTON_H

#include <QToolButton>

namespace CapScript {

class GitHubButton : public QToolButton {
  Q_OBJECT

public:
  explicit GitHubButton(QWidget *parent = nullptr);
  void setCollapsed(bool collapsed);
};

}

#endif
