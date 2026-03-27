#include "Application.h"
#include "../core/PythonBridge.h"
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>

namespace CapScript {

Application::Application(int &argc, char **argv) : QApplication(argc, argv) {
  setOrganizationName("CapScript");
  setApplicationName("CapScript Pro");
#ifdef APP_VERSION
  setApplicationVersion(QStringLiteral(APP_VERSION));
#else
  setApplicationVersion("2.0.0");
#endif
  setStyle("Fusion");

  setWindowIcon(QIcon(":/icons/app_icon.ico"));

  setupFonts();
  setupPalette();
}

QString Application::setFontFromFile(const QString &fontPath, int pointSize) {
  int id = QFontDatabase::addApplicationFont(fontPath);
  if (id == -1)
    return {};
  const QStringList families = QFontDatabase::applicationFontFamilies(id);
  if (families.isEmpty())
    return {};
  qApp->setFont(QFont(families.first(), pointSize));
  return families.first();
}

void Application::setupFonts() {

  int fontId = QFontDatabase::addApplicationFont(":/fonts/Akrobat-Black.ttf");
  if (fontId == -1) {
    qWarning() << "Failed to load custom font, using system fallback";
  } else {
    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty()) {
      qDebug() << "Loaded font family:" << families.first();
    }
  }

  QFont base = font();
  base.setStyleStrategy(
      QFont::StyleStrategy(QFont::PreferQuality | QFont::PreferAntialias));
  setFont(base);
}

Application::~Application() { PythonBridge::instance().shutdown(); }

bool Application::initPython(const QString &scriptsPath) {
  return PythonBridge::instance().init(scriptsPath);
}

void Application::setupPalette() {
  QPalette p;

  p.setColor(QPalette::Window, QColor(0x11, 0x11, 0x11));
  p.setColor(QPalette::WindowText, QColor(0xf0, 0xf0, 0xf0));
  p.setColor(QPalette::Base, QColor(0x14, 0x14, 0x14));
  p.setColor(QPalette::AlternateBase, QColor(0x1e, 0x1e, 0x1e));
  p.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
  p.setColor(QPalette::ToolTipText, QColor(0xf0, 0xf0, 0xf0));
  p.setColor(QPalette::Text, QColor(0xf0, 0xf0, 0xf0));
  p.setColor(QPalette::BrightText, QColor(0xFF, 0x00, 0x33));
  p.setColor(QPalette::Button, QColor(0x1e, 0x1e, 0x1e));
  p.setColor(QPalette::ButtonText, QColor(0xf0, 0xf0, 0xf0));
  p.setColor(QPalette::Highlight, QColor(0xFF, 0x00, 0x33));
  p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::Link, QColor(0xFF, 0x00, 0x33));
  p.setColor(QPalette::LinkVisited, QColor(0xe6, 0x00, 0x2e));

  p.setColor(QPalette::Disabled, QPalette::WindowText,
             QColor(0x44, 0x44, 0x44));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x44, 0x44, 0x44));
  p.setColor(QPalette::Disabled, QPalette::ButtonText,
             QColor(0x44, 0x44, 0x44));

  setPalette(p);
}

}
