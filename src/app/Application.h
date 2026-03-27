#pragma once
#ifndef CAPSCRIPT_APPLICATION_H
#define CAPSCRIPT_APPLICATION_H

#include <QApplication>

namespace CapScript {

class Application : public QApplication {
  Q_OBJECT

public:
  Application(int &argc, char **argv);
  ~Application() override;

  bool initPython(const QString &scriptsPath);

  static QString setFontFromFile(const QString &fontPath, int pointSize = 10);

private:
  void setupPalette();
  void setupFonts();
};

}

#endif
