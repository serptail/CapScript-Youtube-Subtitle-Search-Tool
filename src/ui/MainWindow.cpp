#include "MainWindow.h"
#include "../core/AutoUpdater.h"
#include "../core/Settings.h"
#include "TitleBar.h"
#include "pages/AboutPage.h"
#include "pages/ClipDownloaderPage.h"
#include "pages/ListCreatorPage.h"
#include "pages/RendererPage.h"
#include "pages/SearchPage.h"
#include "pages/ViewerPage.h"
#include "styles/ThemeManager.h"
#include "widgets/SupportPopup.h"
#include <QTimer>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

struct ScreenshotContext {
  QWidget *mainWindow;
  QPainter *painter;
  RECT mainRect;
  double dpr;
};
#endif

namespace CapScript {

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  setObjectName("mainWindow");
  setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
  setAttribute(Qt::WA_TranslucentBackground, false);

  setFixedSize(1100, 950);

  if (auto *app = qobject_cast<QApplication *>(qApp))
    app->setStyleSheet(ThemeManager::generateQSS());

  setupUi();
  wireSignals();

  if (auto *screen = QGuiApplication::primaryScreen()) {
    QRect available = screen->availableGeometry();
    int x = available.x() + (available.width() - 1000) / 2;
    int y = available.y() + (available.height() - 700) / 2;
    move(x, y);
  }

  QTimer::singleShot(800, this, [this]() {
    if (m_updater)
      m_updater->checkForUpdates(true);
  });

  auto *screenshotShortcut = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
  connect(screenshotShortcut, &QShortcut::activated, this, [this]() {
    QString timestamp =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString defaultPath = "CapScriptUI_HQ_" + timestamp + ".png";
    QString dest = QFileDialog::getSaveFileName(
        this, "Save UI Snapshot (High DPI)", defaultPath, "PNG Images (*.png)");
    if (dest.isEmpty())
      return;

    QCoreApplication::processEvents();

    QPixmap snapshot = this->grab();

#ifdef Q_OS_WIN

    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    QPainter painter(&snapshot);

    RECT mainWindowRect;
    GetWindowRect(hwnd, &mainWindowRect);

    ScreenshotContext ctx;
    ctx.mainWindow = this;
    ctx.painter = &painter;
    ctx.mainRect = mainWindowRect;
    ctx.dpr =
        this->windowHandle() ? this->windowHandle()->devicePixelRatio() : 1.0;

    EnumChildWindows(
        hwnd,
        [](HWND childHwnd, LPARAM lParam) -> BOOL {
          if (!IsWindowVisible(childHwnd))
            return TRUE;

          wchar_t className[256];
          GetClassNameW(childHwnd, className, 256);
          QString cName = QString::fromWCharArray(className);

          if (cName == "Chrome_WidgetWin_0" || cName == "Chrome_WidgetWin_1" ||
              cName == "WebView2") {
            ScreenshotContext *c =
                reinterpret_cast<ScreenshotContext *>(lParam);

            RECT r;
            GetWindowRect(childHwnd, &r);

            int x = r.left - c->mainRect.left;
            int y = r.top - c->mainRect.top;
            int width = r.right - r.left;
            int height = r.bottom - r.top;

            if (width > 0 && height > 0) {
              HDC hdcExt = GetDC(nullptr);
              HDC hdcMem = CreateCompatibleDC(hdcExt);
              HBITMAP hBitmap = CreateCompatibleBitmap(hdcExt, width, height);
              HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

              PrintWindow(childHwnd, hdcMem, 2 );

              QImage img = QImage::fromHBITMAP(hBitmap).convertToFormat(
                  QImage::Format_ARGB32);
              QPixmap webViewPixmap = QPixmap::fromImage(img);

              SelectObject(hdcMem, hOldBitmap);
              DeleteObject(hBitmap);
              DeleteDC(hdcMem);
              ReleaseDC(nullptr, hdcExt);

              c->painter->drawPixmap(x * c->dpr, y * c->dpr, width * c->dpr,
                                     height * c->dpr, webViewPixmap);
            }
          }
          return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));
#endif

    if (!dest.isEmpty()) {
      snapshot.save(dest, "PNG", 100);
    }
  });

#ifdef Q_OS_WIN

  HWND hwnd = reinterpret_cast<HWND>(winId());
  MARGINS margins = {1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
  BOOL darkMode = TRUE;
  DwmSetWindowAttribute(hwnd, 20 , &darkMode,
                        sizeof(darkMode));
#endif
}

MainWindow::~MainWindow() { saveGeometry(); }

void MainWindow::setupUi() {
  m_rootLayout = new QVBoxLayout(this);
  m_rootLayout->setContentsMargins(0, 0, 0, 0);
  m_rootLayout->setSpacing(0);

  m_centralContainer = new QWidget(this);
  m_centralContainer->setObjectName("centralContainer");
  auto *containerLayout = new QVBoxLayout(m_centralContainer);
  containerLayout->setContentsMargins(1, 1, 1, 1);
  containerLayout->setSpacing(0);

  m_titleBar = new TitleBar(m_centralContainer);
  containerLayout->addWidget(m_titleBar);

  m_tabBar = new QTabBar(m_centralContainer);
  m_tabBar->setObjectName("tabBar");
  m_tabBar->setExpanding(false);
  m_tabBar->setDrawBase(false);
  m_tabBar->setDocumentMode(true);

  m_tabBar->addTab("  Search  ");
  m_tabBar->addTab("  Viewer  ");
  m_tabBar->addTab("  Clip Downloader  ");
  m_tabBar->addTab("  Renderer  ");
  m_tabBar->addTab("  List Creator  ");
  m_tabBar->addTab("  About  ");

  containerLayout->addWidget(m_tabBar);

  m_pageStack = new QStackedWidget(m_centralContainer);
  m_pageStack->setObjectName("pageStack");

  m_searchPage = new SearchPage(m_pageStack);
  m_viewerPage = new ViewerPage(m_pageStack);
  m_clipDownloaderPage = new ClipDownloaderPage(m_pageStack);
  m_rendererPage = new RendererPage(m_pageStack);
  m_listCreatorPage = new ListCreatorPage(m_pageStack);
  m_aboutPage = new AboutPage(m_pageStack);
  m_updater = new AutoUpdater(this);

  m_pageStack->addWidget(m_searchPage);
  m_pageStack->addWidget(m_viewerPage);
  m_pageStack->addWidget(m_clipDownloaderPage);
  m_pageStack->addWidget(m_rendererPage);
  m_pageStack->addWidget(m_listCreatorPage);
  m_pageStack->addWidget(m_aboutPage);

  containerLayout->addWidget(m_pageStack, 1);

  m_rootLayout->addWidget(m_centralContainer);

  m_supportPopup = new SupportPopup(this);

  m_supportTimer = new QTimer(this);
  m_supportTimer->setSingleShot(true);
  m_supportTimer->setInterval(3 * 60 * 1000);
  connect(m_supportTimer, &QTimer::timeout, this, [this]() {
    if (!m_supportShown) {
      m_supportShown = true;
      m_supportPopup->showPopup();
    }
  });
  m_supportTimer->start();
}

void MainWindow::wireSignals() {

  connect(m_titleBar, &TitleBar::minimizeClicked, this,
          &QWidget::showMinimized);
  connect(m_titleBar, &TitleBar::maximizeClicked, this,
          [this]() { isMaximized() ? showNormal() : showMaximized(); });
  connect(m_titleBar, &TitleBar::closeClicked, this, &QWidget::close);

  connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);

  connect(m_searchPage, &SearchPage::searchFinished, this,
          &MainWindow::onSearchFinished);

  connect(m_viewerPage, &ViewerPage::resultsLoaded, m_clipDownloaderPage,
          &ClipDownloaderPage::loadFromViewer);

  connect(m_aboutPage, &AboutPage::checkForUpdatesRequested, this,
          &MainWindow::onManualCheckUpdates);
  connect(m_updater, &AutoUpdater::statusChanged, m_aboutPage,
          &AboutPage::setUpdateStatus);
  connect(m_updater, &AutoUpdater::updateAvailable, this,
          &MainWindow::onUpdateAvailable);
  connect(m_updater, &AutoUpdater::noUpdateAvailable, this,
          &MainWindow::onNoUpdateAvailable);
  connect(m_updater, &AutoUpdater::updateError, this,
          &MainWindow::onUpdateError);
  connect(m_updater, &AutoUpdater::downloadProgress, this,
          &MainWindow::onDownloadProgress);
  connect(m_updater, &AutoUpdater::downloadFinished, this,
          &MainWindow::onDownloadFinished);
}

void MainWindow::onTabChanged(int index) {
  m_pageStack->setCurrentIndex(index);
}

void MainWindow::onSearchFinished(int count, const QStringList &results) {
  if (count > 0) {
    m_viewerPage->updateViewer(results);
    m_viewerPage->setOutputDir(m_searchPage->outputDir());
    m_rendererPage->setDefaultOutputDir(m_searchPage->outputDir());

    m_clipDownloaderPage->loadFromViewer(results, m_searchPage->outputDir());

    m_tabBar->setCurrentIndex(1);

    if (!m_supportShown) {
      m_supportShown = true;
      if (m_supportTimer)
        m_supportTimer->stop();
      QTimer::singleShot(1200, this, [this]() { m_supportPopup->showPopup(); });
    }
  }
}

void MainWindow::onManualCheckUpdates() {
  if (!m_updater || !m_aboutPage)
    return;

  m_manualUpdateCheck = true;
  m_aboutPage->setBusy(true);
  m_aboutPage->setUpdateProgressVisible(false);
  m_aboutPage->setUpdateStatus("Checking GitHub releases...");
  m_updater->checkForUpdates(false);
}

void MainWindow::onUpdateAvailable(const QString &latestVersion,
                                   const QString &releaseNotes) {
  if (!m_updater || !m_aboutPage)
    return;

  const QString notes = releaseNotes.trimmed().isEmpty()
                            ? QString("No release notes provided.")
                            : releaseNotes.left(1200);

  QMessageBox prompt(this);
  prompt.setIcon(QMessageBox::Information);
  prompt.setWindowTitle("Update Available");
  prompt.setText(QString("A newer version (%1) is available. Download now?")
                     .arg(latestVersion));
  prompt.setInformativeText(notes);
  prompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  prompt.setDefaultButton(QMessageBox::Yes);

  if (prompt.exec() != QMessageBox::Yes) {
    m_aboutPage->setBusy(false);
    m_aboutPage->setUpdateProgressVisible(false);
    m_aboutPage->setUpdateStatus("Update skipped.");
    return;
  }

  m_aboutPage->setBusy(true);
  m_aboutPage->setUpdateProgressVisible(true);
  m_aboutPage->setUpdateProgress(0);
  m_aboutPage->setUpdateStatus(QString("Downloading %1...").arg(latestVersion));
  m_updater->downloadLatestRelease();
}

void MainWindow::onNoUpdateAvailable(const QString &message) {
  if (m_aboutPage) {
    m_aboutPage->setBusy(false);
    m_aboutPage->setUpdateProgressVisible(false);
    m_aboutPage->setUpdateStatus(message);
  }

  if (m_manualUpdateCheck)
    QMessageBox::information(this, "No Updates", message);

  m_manualUpdateCheck = false;
}

void MainWindow::onUpdateError(const QString &message) {
  if (m_aboutPage) {
    m_aboutPage->setBusy(false);
    m_aboutPage->setUpdateProgressVisible(false);
    m_aboutPage->setUpdateStatus("Update error. See message box for details.");
  }

  QMessageBox::warning(this, "Update Error", message);
  m_manualUpdateCheck = false;
}

void MainWindow::onDownloadProgress(int percent) {
  if (!m_aboutPage)
    return;

  m_aboutPage->setUpdateProgressVisible(true);
  m_aboutPage->setUpdateProgress(percent);
  m_aboutPage->setUpdateStatus(
      QString("Downloading update... %1%").arg(percent));
}

void MainWindow::onDownloadFinished(const QString &zipPath,
                                    const QString &version) {
  if (!m_updater || !m_aboutPage)
    return;

  m_aboutPage->setBusy(false);
  m_aboutPage->setUpdateProgressVisible(true);
  m_aboutPage->setUpdateProgress(100);
  m_aboutPage->setUpdateStatus(QString("Update %1 downloaded.").arg(version));

  if (!m_updater->stagePendingUpdate(zipPath, version)) {
    QMessageBox::warning(this, "Update Error",
                         "The update downloaded but could not be staged.");
    m_manualUpdateCheck = false;
    return;
  }

  QMessageBox restartPrompt(this);
  restartPrompt.setIcon(QMessageBox::Question);
  restartPrompt.setWindowTitle("Update Ready");
  restartPrompt.setText("The update has been downloaded.");
  restartPrompt.setInformativeText(
      "Restart now to apply it, or apply automatically when you exit later.");
  QPushButton *restartNow =
      restartPrompt.addButton("Restart now", QMessageBox::AcceptRole);
  QPushButton *later =
      restartPrompt.addButton("Later", QMessageBox::RejectRole);
  restartPrompt.exec();

  if (restartPrompt.clickedButton() == restartNow) {
    if (m_updater->applyPendingUpdate(true)) {
      m_updateApplyOnExit = false;
      close();
      return;
    }
    QMessageBox::warning(this, "Update Error",
                         "Failed to launch updater bootstrapper.");
  } else if (restartPrompt.clickedButton() == later) {
    m_updateApplyOnExit = true;
    m_aboutPage->setUpdateStatus("Update will be applied when the app exits.");
  }

  m_manualUpdateCheck = false;
}

void MainWindow::saveGeometry() {
  Settings::setWindowGeometry(QWidget::saveGeometry());
}

void MainWindow::restoreGeometryFromSettings() {

  int savedVer =
      Settings::get().value(Settings::GEOMETRY_VERSION_KEY, 0).toInt();
  if (savedVer != Settings::GEOMETRY_VERSION) {
    Settings::get().remove(Settings::WINDOW_GEOMETRY);
    Settings::get().setValue(Settings::GEOMETRY_VERSION_KEY,
                             Settings::GEOMETRY_VERSION);
  }

  QByteArray geo = Settings::windowGeometry();
  if (!geo.isEmpty()) {
    QWidget::restoreGeometry(geo);
  } else {

    if (auto *screen = QGuiApplication::primaryScreen()) {
      QRect available = screen->availableGeometry();
      int w = qMax(900, qMin(1200, static_cast<int>(available.width() * 0.74)));
      int h = qMax(620, qMin(900, static_cast<int>(available.height() * 0.74)));
      int x = available.x() + (available.width() - w) / 2;
      int y = available.y() + (available.height() - h) / 2;
      setGeometry(x, y, w, h);
    }
  }
}

void MainWindow::closeEvent(QCloseEvent *e) {
  if (m_updater && m_updateApplyOnExit && m_updater->hasPendingUpdate()) {
    m_updater->applyPendingUpdate(false);
  }

  e->accept();
}

void MainWindow::changeEvent(QEvent *e) {
  QWidget::changeEvent(e);
  if (e->type() == QEvent::WindowStateChange)
    applyWindowMask();
}

void MainWindow::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  applyWindowMask();
}

void MainWindow::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  applyWindowMask();
}

void MainWindow::applyWindowMask() {
  if (isMaximized() || isFullScreen()) {
    clearMask();
    return;
  }

  const int radius = 10;
  QPainterPath path;
  path.addRoundedRect(rect(), radius, radius);
  setMask(QRegion(path.toFillPolygon().toPolygon()));
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message,
                             qintptr *result) {
  Q_UNUSED(eventType)
  MSG *msg = static_cast<MSG *>(message);

  if (msg->message == WM_NCHITTEST) {

    const int borderWidth = 10;

    RECT winRect;
    GetWindowRect(msg->hwnd, &winRect);

    int x = GET_X_LPARAM(msg->lParam);
    int y = GET_Y_LPARAM(msg->lParam);

    if (isMaximized()) {
      return QWidget::nativeEvent(eventType, message, result);
    }

    bool left = x < winRect.left + borderWidth;
    bool right = x >= winRect.right - borderWidth;
    bool top = y < winRect.top + borderWidth;
    bool bottom = y >= winRect.bottom - borderWidth;

    if (top && left) {
      *result = HTTOPLEFT;
      return true;
    }
    if (top && right) {
      *result = HTTOPRIGHT;
      return true;
    }
    if (bottom && left) {
      *result = HTBOTTOMLEFT;
      return true;
    }
    if (bottom && right) {
      *result = HTBOTTOMRIGHT;
      return true;
    }

    if (left) {
      *result = HTLEFT;
      return true;
    }
    if (right) {
      *result = HTRIGHT;
      return true;
    }
    if (top) {
      *result = HTTOP;
      return true;
    }
    if (bottom) {
      *result = HTBOTTOM;
      return true;
    }
  }
  return QWidget::nativeEvent(eventType, message, result);
}

}
