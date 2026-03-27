#pragma once

#ifndef CAPSCRIPT_PYTHONBRIDGE_H
#define CAPSCRIPT_PYTHONBRIDGE_H

#ifdef slots
#undef slots
#endif
#include <Python.h>

#ifndef slots
#define slots Q_SLOTS
#endif

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>


#include <mutex>
#include <string>

namespace CapScript {

class GILLock {
public:
  GILLock() {
    if (Py_IsInitialized()) {
      m_state = PyGILState_Ensure();
      m_acquired = true;
    }
  }
  ~GILLock() {
    if (m_acquired) {
      PyGILState_Release(m_state);
    }
  }
  GILLock(const GILLock &) = delete;
  GILLock &operator=(const GILLock &) = delete;

private:
  PyGILState_STATE m_state{};
  bool m_acquired = false;
};

class PythonBridge : public QObject {
  Q_OBJECT

public:
  static PythonBridge &instance();

  bool init(const QString &pythonScriptsPath);
  void shutdown();
  bool isInitialised() const { return m_initialised; }

  bool validateApiKey(const QString &key);
  bool saveApiKey(const QString &key);
  QString loadApiKey();

  bool saveProxySettings(const QString &type, const QString &username,
                         const QString &password, const QString &url);
  QString loadProxySettings();

  QString searchTranscripts(
      const QJsonObject &params,
      std::function<bool(int, const QString &)> progressCb = nullptr);

  QJsonArray getChannelVideos(const QString &apiKey, const QString &channelId,
                              const QString &lang, int maxResults);
  QJsonArray parseVideoIds(const QString &input);
  QJsonObject getVideoDetails(const QString &apiKey, const QString &videoId);

  QString resolveChannelId(const QString &apiKey, const QString &channelInput);

  QJsonArray fetchVideosByChannelDate(const QString &apiKey,
                                      const QString &channelId,
                                      const QString &startIso,
                                      const QString &endIso);
  QJsonArray searchVideosByKeyword(const QString &apiKey,
                                   const QString &channelId,
                                   const QString &keyword,
                                   const QString &startIso = {},
                                   const QString &endIso = {});
  QJsonObject getVideoDetailsBatch(const QString &apiKey,
                                   const QStringList &videoIds);

signals:
  void pythonError(const QString &message);

private:
  PythonBridge();
  ~PythonBridge();
  PythonBridge(const PythonBridge &) = delete;
  PythonBridge &operator=(const PythonBridge &) = delete;

  PyObject *callFunction(const char *funcName, PyObject *args);

  QString pyStringToQString(PyObject *obj);

  bool m_initialised = false;
  PyObject *m_module = nullptr;
  std::mutex m_mutex;
  PyThreadState *m_mainThreadState = nullptr;
};

}

#endif
