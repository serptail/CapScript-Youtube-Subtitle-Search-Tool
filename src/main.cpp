

#include "app/Application.h"
#include "ui/MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>

#ifdef CAPSCRIPT_WEBVIEW
#include <QtWebView/QtWebView>
#endif

int main(int argc, char *argv[]) {

  qputenv("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
          "--autoplay-policy=no-user-gesture-required");

  if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
    qputenv("QSG_RHI_BACKEND", "d3d11");
  }

  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

#ifdef CAPSCRIPT_WEBVIEW

  QtWebView::initialize();

  QString qmlPath = QFileInfo(argv[0]).absolutePath() + "/qml";
  if (QDir(qmlPath).exists())
    qputenv("QML_IMPORT_PATH", qmlPath.toUtf8());
#endif

  CapScript::Application app(argc, argv);
  app.setWindowIcon(QIcon(":/icons/app_icon.ico"));

  QString pythonDir = QDir(app.applicationDirPath()).filePath("python");

  if (!QDir(pythonDir).exists())
    pythonDir = QDir(app.applicationDirPath()).filePath("../python");

  if (!QDir(pythonDir).exists())
    pythonDir = QDir(app.applicationDirPath()).filePath("../../python");

  bool pythonOk = app.initPython(pythonDir);

  CapScript::MainWindow window;
  window.show();
  window.raise();
  window.activateWindow();

  if (!pythonOk) {
    QMessageBox::warning(
        &window, "Python Engine Warning",
        "The Python engine failed to initialise.\n\n"
        "Transcript search and API features will not work.\n"
        "Please ensure the embedded Python runtime and scripts are\n"
        "present in the 'python/' folder next to the executable\n"
        "(including Lib/, python311.zip, and .pyd modules).\n\n"
        "Path searched: " +
            pythonDir);
  }

  return app.exec();
}