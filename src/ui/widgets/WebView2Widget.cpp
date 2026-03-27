#include "WebView2Widget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#ifdef CAPSCRIPT_WEBVIEW2
#include <WebView2.h>
#include <Windows.h>

namespace {

class EnvironmentCompletedHandler final
    : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
  explicit EnvironmentCompletedHandler(CapScript::WebView2Widget *owner)
      : m_owner(owner) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown ||
        riid ==
            IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
      *ppvObject = static_cast<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG value = InterlockedDecrement(&m_refCount);
    if (value == 0)
      delete this;
    return static_cast<ULONG>(value);
  }

  HRESULT STDMETHODCALLTYPE
  Invoke(HRESULT result, ICoreWebView2Environment *environment) override {
    if (!m_owner) {
      return S_OK;
    }

    m_owner->onEnvironmentCreated(static_cast<long>(result), environment);
    return S_OK;
  }

private:
  LONG m_refCount = 1;
  CapScript::WebView2Widget *m_owner = nullptr;
};

class ControllerCompletedHandler final
    : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
  explicit ControllerCompletedHandler(CapScript::WebView2Widget *owner)
      : m_owner(owner) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown ||
        riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
      *ppvObject = static_cast<
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG value = InterlockedDecrement(&m_refCount);
    if (value == 0)
      delete this;
    return static_cast<ULONG>(value);
  }

  HRESULT STDMETHODCALLTYPE
  Invoke(HRESULT result, ICoreWebView2Controller *controller) override {
    if (!m_owner) {
      return S_OK;
    }

    m_owner->onControllerCreated(static_cast<long>(result), controller);
    return S_OK;
  }

private:
  LONG m_refCount = 1;
  CapScript::WebView2Widget *m_owner = nullptr;
};

class WebResourceRequestedHandler final
    : public ICoreWebView2WebResourceRequestedEventHandler {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown ||
        riid == IID_ICoreWebView2WebResourceRequestedEventHandler) {
      *ppvObject =
          static_cast<ICoreWebView2WebResourceRequestedEventHandler *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG value = InterlockedDecrement(&m_refCount);
    if (value == 0)
      delete this;
    return static_cast<ULONG>(value);
  }

  HRESULT STDMETHODCALLTYPE
  Invoke(ICoreWebView2 *sender,
         ICoreWebView2WebResourceRequestedEventArgs *args) override {
    ICoreWebView2WebResourceRequest *request = nullptr;
    if (SUCCEEDED(args->get_Request(&request)) && request) {
      ICoreWebView2HttpRequestHeaders *headers = nullptr;
      if (SUCCEEDED(request->get_Headers(&headers)) && headers) {
        headers->SetHeader(L"Referer", L"https://capscriptpro.local/");
        headers->Release();
      }
      request->Release();
    }
    return S_OK;
  }

private:
  LONG m_refCount = 1;
};

using CreateEnvironmentFn = HRESULT(STDAPICALLTYPE *)(
    PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);

static std::wstring toWide(const QString &value) {
  return value.toStdWString();
}

static QString hrHex(HRESULT hr) {
  return QString("0x%1").arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
}

static void logWebView2(const QString &message) {
  const QString line = QString("[%1] %2").arg(
      QDateTime::currentDateTime().toString(Qt::ISODate), message);
  qWarning() << line;

  const QStringList candidates = {
      QCoreApplication::applicationDirPath(),
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)};

  for (const QString &dirPath : candidates) {
    if (dirPath.isEmpty()) {
      continue;
    }

    QDir().mkpath(dirPath);
    QFile file(QDir(dirPath).filePath("webview2.log"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
      QTextStream out(&file);
      out << line << "\n";
      return;
    }
  }
}

}
#endif

namespace CapScript {

WebView2Widget::WebView2Widget(QWidget *parent) : QWidget(parent) {
#ifdef CAPSCRIPT_WEBVIEW2
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DontCreateNativeAncestors);
  setAttribute(Qt::WA_OpaquePaintEvent);

  m_shouldBeVisible = isVisible();

  const HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  m_comInitialized = SUCCEEDED(coResult) || coResult == S_FALSE;
  m_comReady = m_comInitialized || coResult == RPC_E_CHANGED_MODE;

  if (!m_comReady) {
    logWebView2(QString("CoInitializeEx failed: %1").arg(hrHex(coResult)));
  } else if (coResult == RPC_E_CHANGED_MODE) {

    logWebView2(
        QString("CoInitializeEx returned RPC_E_CHANGED_MODE (%1), continuing")
            .arg(hrHex(coResult)));
  }

  initializeWebView2();
#else
  Q_UNUSED(parent)
#endif
}

WebView2Widget::~WebView2Widget() {
#ifdef CAPSCRIPT_WEBVIEW2
  if (m_webView) {
    m_webView->Release();
    m_webView = nullptr;
  }
  if (m_controller) {
    m_controller->Release();
    m_controller = nullptr;
  }
  if (m_environment) {
    m_environment->Release();
    m_environment = nullptr;
  }
  if (m_loaderModule) {
    FreeLibrary(static_cast<HMODULE>(m_loaderModule));
    m_loaderModule = nullptr;
  }
  if (m_comInitialized) {
    CoUninitialize();
  }
#endif
}

void WebView2Widget::loadUrl(const QString &url) {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_webView) {
    m_pendingUrl = url;
    m_pendingHtml.clear();
    return;
  }

  syncControllerVisibility();

  const std::wstring wideUrl = toWide(url);
  m_webView->Navigate(wideUrl.c_str());
#else
  Q_UNUSED(url)
#endif
}

void WebView2Widget::loadHtml(const QString &html) {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_webView) {
    m_pendingHtml = html;
    m_pendingUrl.clear();
    return;
  }

  syncControllerVisibility();

  const std::wstring wideHtml = toWide(html);
  m_webView->NavigateToString(wideHtml.c_str());

  QTimer::singleShot(0, this, [this]() {
    if (m_controller && m_shouldBeVisible && isVisible()) {
      syncControllerVisibility();
    }
  });
#else
  Q_UNUSED(html)
#endif
}

void WebView2Widget::executeJavaScript(const QString &js) {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_webView)
    return;
  const std::wstring wideJs = toWide(js);
  m_webView->ExecuteScript(wideJs.c_str(), nullptr);
#else
  Q_UNUSED(js)
#endif
}

void WebView2Widget::stop() {
#ifdef CAPSCRIPT_WEBVIEW2
  loadUrl(QStringLiteral("about:blank"));
#endif
}

void WebView2Widget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateControllerBounds();
}

void WebView2Widget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
#ifdef CAPSCRIPT_WEBVIEW2
  m_shouldBeVisible = true;
  syncControllerVisibility();

  QTimer::singleShot(0, this, [this]() {
    if (m_controller && m_shouldBeVisible && isVisible()) {
      syncControllerVisibility();
    }
  });
#endif
  updateControllerBounds();
}

void WebView2Widget::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
#ifdef CAPSCRIPT_WEBVIEW2
  m_shouldBeVisible = false;
  syncControllerVisibility();
#endif
}

void WebView2Widget::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
#ifdef CAPSCRIPT_WEBVIEW2

  if (event->type() == QEvent::WindowStateChange) {

    QTimer::singleShot(100, this, [this]() {
      if (m_shouldBeVisible && isVisible()) {
        syncControllerVisibility();
        updateControllerBounds();
      }
    });
  }
#endif
}

void WebView2Widget::initializeWebView2() {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_comReady) {
    m_initFailed = true;
    logWebView2("Aborting WebView2 init because COM is unavailable");
    return;
  }

  const QString appDir = QCoreApplication::applicationDirPath();
  const std::wstring loaderPath =
      toWide(QDir(appDir).filePath("WebView2Loader.dll"));

  HMODULE loaderModule = LoadLibraryW(loaderPath.c_str());
  if (!loaderModule) {
    loaderModule = LoadLibraryW(L"WebView2Loader.dll");
  }
  if (!loaderModule) {
    m_initFailed = true;
    logWebView2(QString("Failed to load WebView2Loader.dll (GetLastError=%1)")
                    .arg(GetLastError()));
    return;
  }

  m_loaderModule = loaderModule;

  auto *createEnvironment = reinterpret_cast<CreateEnvironmentFn>(
      GetProcAddress(loaderModule, "CreateCoreWebView2EnvironmentWithOptions"));
  if (!createEnvironment) {
    m_initFailed = true;
    logWebView2(QString("CreateCoreWebView2EnvironmentWithOptions symbol not "
                        "found (GetLastError=%1)")
                    .arg(GetLastError()));
    return;
  }

  const QString dataRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dataRoot);
  const std::wstring userDataFolder =
      toWide(QDir(dataRoot).filePath("webview2"));

  const HRESULT hr = createEnvironment(nullptr, userDataFolder.c_str(), nullptr,
                                       new EnvironmentCompletedHandler(this));

  if (FAILED(hr)) {
    m_initFailed = true;
    logWebView2(QString("CreateCoreWebView2EnvironmentWithOptions failed: %1")
                    .arg(hrHex(hr)));
    return;
  }

  logWebView2(
      QString("Requested WebView2 environment creation. userDataFolder=%1")
          .arg(QDir(dataRoot).filePath("webview2")));
#endif
}

void WebView2Widget::updateControllerBounds() {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_controller)
    return;

  RECT bounds;
  GetClientRect(reinterpret_cast<HWND>(winId()), &bounds);
  m_controller->put_Bounds(bounds);
#endif
}

void WebView2Widget::syncControllerVisibility() {
#ifdef CAPSCRIPT_WEBVIEW2
  if (!m_controller)
    return;

  if (m_shouldBeVisible && isVisible()) {
    m_controller->put_IsVisible(TRUE);
    updateControllerBounds();
    return;
  }

  m_controller->put_IsVisible(FALSE);

  RECT zeroBounds = {0, 0, 0, 0};
  m_controller->put_Bounds(zeroBounds);
#endif
}

void WebView2Widget::onEnvironmentCreated(
    long result, ICoreWebView2Environment *environment) {
#ifdef CAPSCRIPT_WEBVIEW2
  if (FAILED(static_cast<HRESULT>(result)) || !environment) {
    m_initFailed = true;
    logWebView2(QString("onEnvironmentCreated failed: %1")
                    .arg(hrHex(static_cast<HRESULT>(result))));
    return;
  }

  environment->AddRef();
  m_environment = environment;

  const HWND parentHwnd = reinterpret_cast<HWND>(winId());
  const HRESULT hr = environment->CreateCoreWebView2Controller(
      parentHwnd, new ControllerCompletedHandler(this));
  if (FAILED(hr)) {
    m_initFailed = true;
    logWebView2(
        QString("CreateCoreWebView2Controller failed: %1").arg(hrHex(hr)));
    return;
  }

  logWebView2("Environment created successfully");
#else
  Q_UNUSED(environment)
#endif
}

void WebView2Widget::onControllerCreated(long result,
                                         ICoreWebView2Controller *controller) {
#ifdef CAPSCRIPT_WEBVIEW2
  if (FAILED(static_cast<HRESULT>(result)) || !controller) {
    m_initFailed = true;
    logWebView2(QString("onControllerCreated failed: %1")
                    .arg(hrHex(static_cast<HRESULT>(result))));
    return;
  }

  controller->AddRef();
  m_controller = controller;

  syncControllerVisibility();

  ICoreWebView2 *webView = nullptr;
  if (SUCCEEDED(controller->get_CoreWebView2(&webView)) && webView) {
    m_webView = webView;

    m_webView->AddWebResourceRequestedFilter(
        L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    EventRegistrationToken token;
    m_webView->add_WebResourceRequested(new WebResourceRequestedHandler(),
                                        &token);

    syncControllerVisibility();

    if (!m_pendingUrl.isEmpty()) {
      const QString pending = m_pendingUrl;
      m_pendingUrl.clear();
      loadUrl(pending);
    } else if (!m_pendingHtml.isEmpty()) {
      const QString pending = m_pendingHtml;
      m_pendingHtml.clear();
      loadHtml(pending);
    }

    logWebView2("WebView2 controller + core webview created successfully");
    emit webViewReady();
  } else {
    m_initFailed = true;
    logWebView2("get_CoreWebView2 returned null/failed");
  }
#else
  Q_UNUSED(controller)
#endif
}

bool WebView2Widget::nativeEvent(const QByteArray &eventType, void *message,
                                 qintptr *result) {
#ifdef CAPSCRIPT_WEBVIEW2
  Q_UNUSED(eventType)
  MSG *msg = static_cast<MSG *>(message);

  if (msg->message == WM_MOUSEACTIVATE) {
    *result = MA_NOACTIVATE;
    return true;
  }
#else
  Q_UNUSED(eventType)
  Q_UNUSED(message)
  Q_UNUSED(result)
#endif
  return QWidget::nativeEvent(eventType, message, result);
}

}
