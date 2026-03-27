#include "SearchWorker.h"
#include "core/PythonBridge.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>


namespace CapScript {

static const char *kLogStyle =
    "margin:0; padding:2px 0; font-family:'Consolas','Courier New',monospace; "
    "font-size:9pt;";

enum class LogCat { Info, Network, Stats, Warning, Error, Success };

static QString logLine(LogCat cat, const QString &text, bool bold = false) {

  struct CatInfo {
    const char *label;
    const char *badge;
    const char *fg;
  };
  static const CatInfo cats[] = {
       {"[INFO]", "#2196f3", "#b0bec5"},
       {"[NET] ", "#00bcd4", "#80deea"},
       {"[STAT]", "#ab47bc", "#ce93d8"},
       {"[WARN]", "#ff9800", "#ffcc80"},
       {"[ERR] ", "#f44336", "#ef9a9a"},
       {"[ OK ]", "#4caf50", "#a5d6a7"},
  };
  const auto &c = cats[static_cast<int>(cat)];

  QString html = QStringLiteral("<p style='%1'>"
                                "<span style='color:%2; font-weight:900; "
                                "font-size:9pt; font-family:monospace;'>"
                                "%3</span> "
                                "&nbsp;")
                     .arg(kLogStyle, c.fg, c.label);

  if (bold)
    html += QStringLiteral("<b style='color:%1'>%2</b>")
                .arg(c.fg, text.toHtmlEscaped());
  else
    html += QStringLiteral("<span style='color:%1'>%2</span>")
                .arg(c.fg, text.toHtmlEscaped());

  html += QStringLiteral("</p>");
  return html;
}

static LogCat classifyMessage(const QString &msg) {

  if (msg.contains(QStringLiteral("Error"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("Failed"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("Could not"), Qt::CaseInsensitive))
    return LogCat::Error;

  if (msg.contains(QStringLiteral("No transcript"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("skipped"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("No videos found"), Qt::CaseInsensitive))
    return LogCat::Warning;

  if (msg.contains(QStringLiteral("Fetch"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("Resolv"), Qt::CaseInsensitive) ||
      msg.contains(QStringLiteral("Download"), Qt::CaseInsensitive))
    return LogCat::Network;

  static QRegularExpression rxCount(
      QStringLiteral("Found \\d|Parsed \\d|\\d+ match"));
  if (rxCount.match(msg).hasMatch())
    return LogCat::Stats;

  if (msg.startsWith(QStringLiteral("Done"), Qt::CaseInsensitive))
    return LogCat::Success;

  return LogCat::Info;
}

SearchWorker::SearchWorker(const QJsonObject &params, QObject *parent)
    : QObject(parent), m_params(params) {}

void SearchWorker::stop() {
  m_running.store(false);
  emit logOutput(logLine(LogCat::Warning, "STOP REQUEST RECEIVED", true));
}

void SearchWorker::run() {
  const auto startTime = QDateTime::currentDateTime();
  const QString keyword = m_params.value("keyword").toString();

  emit logOutput(logLine(
      LogCat::Info,
      QStringLiteral("Search started — keyword: \"%1\"").arg(keyword), true));

  auto progressCb = [this](int pct, const QString &msg) -> bool {
    if (!m_running.load())
      return false;
    emit progressUpdate(pct);
    LogCat cat = classifyMessage(msg);
    bool bold = (cat == LogCat::Error || cat == LogCat::Warning ||
                 cat == LogCat::Success);
    emit logOutput(logLine(cat, msg, bold));
    return true;
  };

  QString resultJson =
      PythonBridge::instance().searchTranscripts(m_params, progressCb);

  QJsonDocument doc = QJsonDocument::fromJson(resultJson.toUtf8());
  QJsonObject obj = doc.object();

  int matchCount = obj.value("match_count").toInt(0);
  QStringList results;
  const QJsonArray arr = obj.value("results").toArray();
  for (const auto &v : arr) {
    results.append(v.toString());
  }

  const double elapsed =
      startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;

  emit logOutput(logLine(
      LogCat::Stats, QStringLiteral("Completed in %1s").arg(elapsed, 0, 'f', 2),
      true));

  if (matchCount > 0) {
    emit logOutput(logLine(LogCat::Success,
                           QStringLiteral("%1 match%2 found")
                               .arg(matchCount)
                               .arg(matchCount == 1 ? "" : "es"),
                           true));
  } else {
    emit logOutput(logLine(LogCat::Warning, "No matches found", true));
  }

  emit progressUpdate(100);
  emit finished(matchCount, results);
}

}
