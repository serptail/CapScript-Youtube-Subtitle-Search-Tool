#include "Application.h"
#include "../core/PythonBridge.h"
#include "../ui/styles/ThemeManager.h"
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>

#include "phantom/phantomstyle.h"

namespace CapScript {

Application::Application(int &argc, char **argv) : QApplication(argc, argv) {
  setOrganizationName("CapScript");
  setApplicationName("CapScript Pro");
#ifdef APP_VERSION
  setApplicationVersion(QStringLiteral(APP_VERSION));
#else
  setApplicationVersion("2.6.0");
#endif

  setStyle(new PhantomStyle());

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
  using namespace Colors;
  QPalette p;

  p.setColor(QPalette::Window, QColor(BgBase));
  p.setColor(QPalette::WindowText, QColor(TextPrimary));
  p.setColor(QPalette::Base, QColor(BgInput));
  p.setColor(QPalette::AlternateBase, QColor(BgElevated));
  p.setColor(QPalette::ToolTipBase, QColor(BgOverlay));
  p.setColor(QPalette::ToolTipText, QColor(TextPrimary));
  p.setColor(QPalette::Text, QColor(TextPrimary));
  p.setColor(QPalette::BrightText, QColor(Accent));
  p.setColor(QPalette::Button, QColor(BgCard));
  p.setColor(QPalette::ButtonText, QColor(TextPrimary));

  p.setColor(QPalette::Highlight, QColor(Accent));
  p.setColor(QPalette::HighlightedText, QColor(TextOnAccent));
  p.setColor(QPalette::Link, QColor(Accent));
  p.setColor(QPalette::LinkVisited, QColor(AccentHover));

  p.setColor(QPalette::Light, QColor(BorderStrong));
  p.setColor(QPalette::Midlight, QColor(Border));
  p.setColor(QPalette::Mid, QColor(Border));
  p.setColor(QPalette::Dark, QColor(BgBase));
  p.setColor(QPalette::Shadow, QColor("#000000"));

  p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(TextDisabled));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor(TextDisabled));
  p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(TextDisabled));
  p.setColor(QPalette::Disabled, QPalette::Base, QColor(DisabledBg));
  p.setColor(QPalette::Disabled, QPalette::Button, QColor(DisabledBg));
  p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(DisabledBdr));

  setPalette(p);
}

}