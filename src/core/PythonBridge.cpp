#include "PythonBridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace CapScript {

namespace {

PyObject *toPyString(const QString &value) {
  return PyUnicode_FromString(value.toUtf8().constData());
}

PyObject *toPyOptionalString(const QString &value) {
  if (value.isEmpty()) {
    Py_INCREF(Py_None);
    return Py_None;
  }
  return toPyString(value);
}

} // namespace

PythonBridge &PythonBridge::instance() {
  static PythonBridge s_instance;
  return s_instance;
}

PythonBridge::PythonBridge() = default;
PythonBridge::~PythonBridge() { shutdown(); }

bool PythonBridge::init(const QString &pythonScriptsPath) {
  if (m_initialised)
    return true;

  qDebug() << "[PythonBridge] Initialising Python interpreter...";
  qDebug() << "[PythonBridge] Scripts path:" << pythonScriptsPath;

  const std::string pathUtf8 = pythonScriptsPath.toStdString();

  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  QString appDir = QCoreApplication::applicationDirPath();
  QString pythonHome;
  QDir appDirObj(appDir);

  auto hasBundledRuntime = [](const QString &dirPath) -> bool {
    const QDir d(dirPath);
    if (!d.exists())
      return false;

    return QFileInfo::exists(d.filePath("python311.zip")) ||
           QFileInfo::exists(d.filePath("python312.zip")) ||
           QFileInfo::exists(d.filePath("python313.zip")) ||
           QFileInfo::exists(d.filePath("python314.zip")) ||
           QFileInfo::exists(d.filePath("python311.dll")) ||
           QFileInfo::exists(d.filePath("python312.dll")) ||
           QFileInfo::exists(d.filePath("python313.dll")) ||
           QFileInfo::exists(d.filePath("python314.dll")) ||
           !d.entryList(QStringList() << "*.pyd", QDir::Files).isEmpty() ||
           QDir(d.filePath("Lib")).exists();
  };

  auto firstValid = [&](const QStringList &candidates) -> QString {
    for (const QString &c : candidates) {
      const QString abs = QDir(c).absolutePath();
      if (hasBundledRuntime(abs))
        return abs;
    }
    return {};
  };

  pythonHome = firstValid(QStringList{
      pythonScriptsPath, appDirObj.filePath("python"), appDir,
      appDirObj.filePath("../python"), appDirObj.filePath("../../python")});

  qDebug() << "[PythonBridge] Python home (bundled):" << pythonHome;

  if (pythonHome.isEmpty() || !hasBundledRuntime(pythonHome)) {
    qWarning() << "[PythonBridge] Bundled Python runtime not found."
               << "Resolved home:" << pythonHome
               << "Scripts path:" << pythonScriptsPath;
  }

  if (!pythonHome.isEmpty() && hasBundledRuntime(pythonHome)) {

    auto appendSearchPath = [&](const QString &p) -> bool {
      std::wstring w = QDir::toNativeSeparators(p).toStdWString();
      const PyStatus st =
          PyWideStringList_Append(&config.module_search_paths, w.c_str());
      if (PyStatus_Exception(st)) {
        qCritical() << "[PythonBridge] Failed to append module path:" << p;
        return false;
      }
      return true;
    };

    QString stdlibZip;
    for (const QString &zipName :
         QStringList{"python314.zip", "python313.zip", "python312.zip", "python311.zip"}) {
      const QString candidate = QDir(pythonHome).filePath(zipName);
      if (QFileInfo::exists(candidate)) {
        stdlibZip = candidate;
        break;
      }
    }

    if (!stdlibZip.isEmpty() && !appendSearchPath(stdlibZip))
      return false;
    if (!appendSearchPath(QDir(pythonHome).filePath("Lib")))
      return false;
    if (!appendSearchPath(QDir(pythonHome).filePath("Lib/site-packages")))
      return false;
    if (!appendSearchPath(pythonScriptsPath))
      return false;

    config.module_search_paths_set = 1;
    config.use_environment = 0;
  }

  config.user_site_directory = 0;

  PyStatus status = Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);

  if (PyStatus_Exception(status)) {
    qCritical() << "[PythonBridge] Py_InitializeFromConfig() failed! msg:"
                << (status.err_msg ? status.err_msg : "(none)");
    return false;
  }

  {
    PyObject *sysPath = PySys_GetObject("path");
    PyObject *pyPath = PyUnicode_FromString(pathUtf8.c_str());
    if (sysPath && pyPath)
      PyList_Insert(sysPath, 0, pyPath);
    Py_XDECREF(pyPath);
  }

  m_module = PyImport_ImportModule("capscript_engine");
  if (!m_module) {
    qCritical() << "[PythonBridge] Failed to import capscript_engine:";
    PyErr_Print();
    Py_Finalize();
    return false;
  }

  m_mainThreadState = PyEval_SaveThread();
  m_initialised = true;
  qDebug() << "[PythonBridge] Python interpreter ready.";
  return true;
}

void PythonBridge::shutdown() {
  if (!m_initialised)
    return;
  qDebug() << "[PythonBridge] Shutting down Python interpreter...";

  PyEval_RestoreThread(m_mainThreadState);

  Py_XDECREF(m_module);
  m_module = nullptr;

  Py_Finalize();
  m_initialised = false;
  qDebug() << "[PythonBridge] Python interpreter shut down.";
}

PyObject *PythonBridge::callFunction(const char *funcName, PyObject *args) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_module)
    return nullptr;

  PyObject *func = PyObject_GetAttrString(m_module, funcName);
  if (!func || !PyCallable_Check(func)) {
    qWarning() << "[PythonBridge] Cannot find callable:" << funcName;
    Py_XDECREF(func);
    if (PyErr_Occurred())
      PyErr_Print();
    return nullptr;
  }

  PyObject *result = PyObject_CallObject(func, args);
  Py_DECREF(func);

  if (!result) {
    QString errorMsg = QStringLiteral("Python call failed: %1")
                           .arg(QString::fromUtf8(funcName));
    if (PyErr_Occurred()) {
      PyObject *ptype = nullptr;
      PyObject *pvalue = nullptr;
      PyObject *ptraceback = nullptr;
      PyErr_Fetch(&ptype, &pvalue, &ptraceback);
      PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
      if (pvalue) {
        PyObject *strObj = PyObject_Str(pvalue);
        if (strObj) {
          errorMsg += QStringLiteral(" - %1").arg(pyStringToQString(strObj));
          Py_DECREF(strObj);
        }
      }
      PyErr_Restore(ptype, pvalue, ptraceback);
    }

    qWarning() << "[PythonBridge]" << errorMsg;
    emit pythonError(errorMsg);
    PyErr_Print();
  }
  return result;
}

QString PythonBridge::pyStringToQString(PyObject *obj) {
  if (!obj)
    return {};
  if (PyUnicode_Check(obj)) {
    const char *utf8 = PyUnicode_AsUTF8(obj);
    return utf8 ? QString::fromUtf8(utf8) : QString{};
  }
  return {};
}

bool PythonBridge::validateApiKey(const QString &key) {
  GILLock gil;
  PyObject *args =
      PyTuple_Pack(1, PyUnicode_FromString(key.toUtf8().constData()));
  PyObject *result = callFunction("validate_api_key", args);
  Py_XDECREF(args);
  bool ok = result && PyObject_IsTrue(result);
  Py_XDECREF(result);
  return ok;
}

bool PythonBridge::saveApiKey(const QString &key) {
  GILLock gil;
  PyObject *args =
      PyTuple_Pack(1, PyUnicode_FromString(key.toUtf8().constData()));
  PyObject *result = callFunction("save_api_key", args);
  Py_XDECREF(args);
  bool ok = result && PyObject_IsTrue(result);
  Py_XDECREF(result);
  return ok;
}

QString PythonBridge::loadApiKey() {
  GILLock gil;
  PyObject *result = callFunction("load_api_key", nullptr);
  QString key = pyStringToQString(result);
  Py_XDECREF(result);
  return key;
}

bool PythonBridge::saveProxySettings(const QString &type,
                                     const QString &username,
                                     const QString &password,
                                     const QString &url) {
  GILLock gil;
  PyObject *args =
      PyTuple_Pack(4, PyUnicode_FromString(type.toUtf8().constData()),
                   PyUnicode_FromString(username.toUtf8().constData()),
                   PyUnicode_FromString(password.toUtf8().constData()),
                   PyUnicode_FromString(url.toUtf8().constData()));
  PyObject *result = callFunction("save_proxy_settings", args);
  Py_XDECREF(args);
  bool ok = result && PyObject_IsTrue(result);
  Py_XDECREF(result);
  return ok;
}

QString PythonBridge::loadProxySettings() {
  GILLock gil;
  PyObject *result = callFunction("load_proxy_settings", nullptr);
  QString json = pyStringToQString(result);
  Py_XDECREF(result);
  return json;
}

struct ProgressCallbackData {

  std::function<bool(int, const QString &)> fn;
};

static void destroyProgressCallbackData(PyObject *capsule) {
  auto *data = static_cast<ProgressCallbackData *>(
      PyCapsule_GetPointer(capsule, "progress_cb_data"));
  delete data;
}

static PyObject *progressTrampoline(PyObject *self, PyObject *args) {
  (void)self;
  int percent = 0;
  const char *msg = nullptr;
  if (!PyArg_ParseTuple(args, "is", &percent, &msg)) {
    Py_RETURN_NONE;
  }

  auto *data = static_cast<ProgressCallbackData *>(
      PyCapsule_GetPointer(self, "progress_cb_data"));
  if (data && data->fn) {
    bool shouldContinue = data->fn(percent, QString::fromUtf8(msg));
    if (!shouldContinue) {

      PyErr_SetString(PyExc_KeyboardInterrupt, "Search cancelled by user");
      return nullptr;
    }
  }
  Py_RETURN_NONE;
}

static PyMethodDef progressMethodDef = {"progress_callback", progressTrampoline,
                                        METH_VARARGS,
                                        "C++ progress callback trampoline"};

QString PythonBridge::searchTranscripts(
    const QJsonObject &params,
    std::function<bool(int, const QString &)> progressCb) {
  GILLock gil;

  QJsonDocument doc(params);
  QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);
  PyObject *pyJson = PyUnicode_FromString(jsonBytes.constData());

  PyObject *pyCallback = nullptr;

  if (progressCb) {

    auto *cbData = new ProgressCallbackData{progressCb};
    PyObject *capsule =
        PyCapsule_New(cbData, "progress_cb_data", destroyProgressCallbackData);

    pyCallback = PyCFunction_New(&progressMethodDef, capsule);
    Py_DECREF(capsule);

    if (!pyCallback) {
      qWarning() << "[PythonBridge] Failed to create progress callback.";
      PyErr_Print();
      pyCallback = Py_None;
      Py_INCREF(Py_None);
    }
  } else {
    pyCallback = Py_None;
    Py_INCREF(Py_None);
  }

  PyObject *args = PyTuple_Pack(2, pyJson, pyCallback);
  PyObject *result = callFunction("search_transcripts", args);

  Py_XDECREF(args);
  Py_XDECREF(pyJson);
  Py_XDECREF(pyCallback);

  QString resultStr = pyStringToQString(result);
  Py_XDECREF(result);
  return resultStr;
}

QString PythonBridge::resolveChannelId(const QString &apiKey,
                                       const QString &channelInput,
                                       const QString &cookiesFile,
                                       const QString &cookiesFromBrowser,
                                       const QString &proxyType,
                                       const QString &proxyUsername,
                                       const QString &proxyPassword,
                                       const QString &proxyUrl) {
  GILLock gil;
  PyObject *args = PyTuple_New(8);
  PyTuple_SET_ITEM(args, 0, toPyString(apiKey));
  PyTuple_SET_ITEM(args, 1, toPyString(channelInput));
  PyTuple_SET_ITEM(args, 2, toPyOptionalString(cookiesFile));
  PyTuple_SET_ITEM(args, 3, toPyOptionalString(cookiesFromBrowser));
  PyTuple_SET_ITEM(args, 4, toPyOptionalString(proxyType));
  PyTuple_SET_ITEM(args, 5, toPyOptionalString(proxyUsername));
  PyTuple_SET_ITEM(args, 6, toPyOptionalString(proxyPassword));
  PyTuple_SET_ITEM(args, 7, toPyOptionalString(proxyUrl));
  PyObject *result = callFunction("resolve_channel_id", args);
  Py_XDECREF(args);

  QString resolved = pyStringToQString(result);
  Py_XDECREF(result);
  return resolved;
}

QJsonArray PythonBridge::getChannelVideos(const QString &apiKey,
                                          const QString &channelId,
                                          const QString &lang, int maxResults,
                                          const QString &cookiesFile,
                                          const QString &cookiesFromBrowser,
                                          const QString &proxyType,
                                          const QString &proxyUsername,
                                          const QString &proxyPassword,
                                          const QString &proxyUrl) {
  GILLock gil;
  PyObject *args = PyTuple_New(10);
  PyTuple_SET_ITEM(args, 0, toPyString(apiKey));
  PyTuple_SET_ITEM(args, 1, toPyString(channelId));
  PyTuple_SET_ITEM(args, 2, toPyString(lang));
  PyTuple_SET_ITEM(args, 3, PyLong_FromLong(maxResults));
  PyTuple_SET_ITEM(args, 4, toPyOptionalString(cookiesFile));
  PyTuple_SET_ITEM(args, 5, toPyOptionalString(cookiesFromBrowser));
  PyTuple_SET_ITEM(args, 6, toPyOptionalString(proxyType));
  PyTuple_SET_ITEM(args, 7, toPyOptionalString(proxyUsername));
  PyTuple_SET_ITEM(args, 8, toPyOptionalString(proxyPassword));
  PyTuple_SET_ITEM(args, 9, toPyOptionalString(proxyUrl));
  PyObject *result = callFunction("get_channel_videos", args);
  Py_XDECREF(args);

  QJsonArray arr;
  if (result && PyList_Check(result)) {
    Py_ssize_t size = PyList_Size(result);
    for (Py_ssize_t i = 0; i < size; ++i) {
      PyObject *item = PyList_GetItem(result, i);
      arr.append(QString::fromUtf8(PyUnicode_AsUTF8(item)));
    }
  }
  Py_XDECREF(result);
  return arr;
}

QJsonArray PythonBridge::parseVideoIds(const QString &input) {
  GILLock gil;
  PyObject *args =
      PyTuple_Pack(1, PyUnicode_FromString(input.toUtf8().constData()));
  PyObject *result = callFunction("parse_video_ids", args);
  Py_XDECREF(args);

  QJsonArray arr;
  if (result && PyList_Check(result)) {
    Py_ssize_t size = PyList_Size(result);
    for (Py_ssize_t i = 0; i < size; ++i) {
      PyObject *item = PyList_GetItem(result, i);
      arr.append(QString::fromUtf8(PyUnicode_AsUTF8(item)));
    }
  }
  Py_XDECREF(result);
  return arr;
}

QJsonObject PythonBridge::getVideoDetails(const QString &apiKey,
                                          const QString &videoId) {
  GILLock gil;
  PyObject *args =
      PyTuple_Pack(2, PyUnicode_FromString(apiKey.toUtf8().constData()),
                   PyUnicode_FromString(videoId.toUtf8().constData()));
  PyObject *result = callFunction("get_video_details", args);
  Py_XDECREF(args);

  QJsonObject obj;
  if (result && PyDict_Check(result)) {

    PyObject *jsonMod = PyImport_ImportModule("json");
    if (jsonMod) {
      PyObject *dumps = PyObject_GetAttrString(jsonMod, "dumps");
      PyObject *dumpArgs = PyTuple_Pack(1, result);
      PyObject *jsonStr = PyObject_CallObject(dumps, dumpArgs);
      if (jsonStr) {
        QString qs = pyStringToQString(jsonStr);
        obj = QJsonDocument::fromJson(qs.toUtf8()).object();
      }
      Py_XDECREF(jsonStr);
      Py_XDECREF(dumpArgs);
      Py_XDECREF(dumps);
      Py_XDECREF(jsonMod);
    }
  }
  Py_XDECREF(result);
  return obj;
}

QJsonArray PythonBridge::fetchVideosByChannelDate(const QString &apiKey,
                                                  const QString &channelId,
                                                  const QString &startIso,
                                                  const QString &endIso,
                                                  const QString &cookiesFile,
                                                  const QString &cookiesFromBrowser,
                                                  const QString &proxyType,
                                                  const QString &proxyUsername,
                                                  const QString &proxyPassword,
                                                  const QString &proxyUrl) {
  GILLock gil;
  PyObject *args = PyTuple_New(10);
  PyTuple_SET_ITEM(args, 0, toPyString(apiKey));
  PyTuple_SET_ITEM(args, 1, toPyString(channelId));
  PyTuple_SET_ITEM(args, 2, toPyString(startIso));
  PyTuple_SET_ITEM(args, 3, toPyString(endIso));
  PyTuple_SET_ITEM(args, 4, toPyOptionalString(cookiesFile));
  PyTuple_SET_ITEM(args, 5, toPyOptionalString(cookiesFromBrowser));
  PyTuple_SET_ITEM(args, 6, toPyOptionalString(proxyType));
  PyTuple_SET_ITEM(args, 7, toPyOptionalString(proxyUsername));
  PyTuple_SET_ITEM(args, 8, toPyOptionalString(proxyPassword));
  PyTuple_SET_ITEM(args, 9, toPyOptionalString(proxyUrl));
  PyObject *result = callFunction("fetch_videos_by_channel_date", args);
  Py_XDECREF(args);

  QJsonArray arr;
  if (result && PyList_Check(result)) {
    PyObject *jsonMod = PyImport_ImportModule("json");
    if (jsonMod) {
      PyObject *dumps = PyObject_GetAttrString(jsonMod, "dumps");
      PyObject *dumpArgs = PyTuple_Pack(1, result);
      PyObject *jsonStr = PyObject_CallObject(dumps, dumpArgs);
      if (jsonStr) {
        arr = QJsonDocument::fromJson(pyStringToQString(jsonStr).toUtf8())
                  .array();
      }
      Py_XDECREF(jsonStr);
      Py_XDECREF(dumpArgs);
      Py_XDECREF(dumps);
      Py_XDECREF(jsonMod);
    }
  }
  Py_XDECREF(result);
  return arr;
}

QJsonArray PythonBridge::searchVideosByKeyword(const QString &apiKey,
                                               const QString &channelId,
                                               const QString &keyword,
                                               const QString &startIso,
                                               const QString &endIso,
                                               const QString &cookiesFile,
                                               const QString &cookiesFromBrowser,
                                               const QString &proxyType,
                                               const QString &proxyUsername,
                                               const QString &proxyPassword,
                                               const QString &proxyUrl) {
  GILLock gil;
  PyObject *args = PyTuple_New(11);
  PyTuple_SET_ITEM(args, 0, toPyString(apiKey));
  PyTuple_SET_ITEM(args, 1, toPyString(channelId));
  PyTuple_SET_ITEM(args, 2, toPyString(keyword));
  PyTuple_SET_ITEM(args, 3, toPyOptionalString(startIso));
  PyTuple_SET_ITEM(args, 4, toPyOptionalString(endIso));
  PyTuple_SET_ITEM(args, 5, toPyOptionalString(cookiesFile));
  PyTuple_SET_ITEM(args, 6, toPyOptionalString(cookiesFromBrowser));
  PyTuple_SET_ITEM(args, 7, toPyOptionalString(proxyType));
  PyTuple_SET_ITEM(args, 8, toPyOptionalString(proxyUsername));
  PyTuple_SET_ITEM(args, 9, toPyOptionalString(proxyPassword));
  PyTuple_SET_ITEM(args, 10, toPyOptionalString(proxyUrl));
  PyObject *result = callFunction("search_videos_by_keyword", args);
  Py_XDECREF(args);

  QJsonArray arr;
  if (result && PyList_Check(result)) {
    PyObject *jsonMod = PyImport_ImportModule("json");
    if (jsonMod) {
      PyObject *dumps = PyObject_GetAttrString(jsonMod, "dumps");
      PyObject *dumpArgs = PyTuple_Pack(1, result);
      PyObject *jsonStr = PyObject_CallObject(dumps, dumpArgs);
      if (jsonStr) {
        arr = QJsonDocument::fromJson(pyStringToQString(jsonStr).toUtf8())
                  .array();
      }
      Py_XDECREF(jsonStr);
      Py_XDECREF(dumpArgs);
      Py_XDECREF(dumps);
      Py_XDECREF(jsonMod);
    }
  }
  Py_XDECREF(result);
  return arr;
}

QJsonObject PythonBridge::getVideoDetailsBatch(const QString &apiKey,
                                               const QStringList &videoIds) {
  GILLock gil;
  PyObject *pyList = PyList_New(videoIds.size());
  for (int i = 0; i < videoIds.size(); ++i) {
    PyList_SetItem(pyList, i,
                   PyUnicode_FromString(videoIds[i].toUtf8().constData()));
  }
  PyObject *args = PyTuple_Pack(
      2, PyUnicode_FromString(apiKey.toUtf8().constData()), pyList);
  PyObject *result = callFunction("get_video_details_batch", args);
  Py_XDECREF(args);

  QJsonObject obj;
  if (result && PyDict_Check(result)) {
    PyObject *jsonMod = PyImport_ImportModule("json");
    if (jsonMod) {
      PyObject *dumps = PyObject_GetAttrString(jsonMod, "dumps");
      PyObject *dumpArgs = PyTuple_Pack(1, result);
      PyObject *jsonStr = PyObject_CallObject(dumps, dumpArgs);
      if (jsonStr) {
        obj = QJsonDocument::fromJson(pyStringToQString(jsonStr).toUtf8())
                  .object();
      }
      Py_XDECREF(jsonStr);
      Py_XDECREF(dumpArgs);
      Py_XDECREF(dumps);
      Py_XDECREF(jsonMod);
    }
  }
  Py_XDECREF(result);
  return obj;
}

}
