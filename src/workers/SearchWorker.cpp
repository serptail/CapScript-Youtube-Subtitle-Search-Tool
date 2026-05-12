#include "SearchWorker.h"
#include "core/PythonBridge.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace CapScript {

enum class LogCat {
    Video,
    Info,
    Network,
    Stats,
    Subs,
    Hit,
    Warning,
    Error,
    Success,
};

static QString logLine(LogCat cat, const QString &text, bool bold = false)
{
    struct CatInfo { const char *label; const char *fg; };
    static const CatInfo kCats[] = {
        { "VIDEO", "#ffb74d" },
        { "INFO",  "#78909c" },
        { "NET",   "#4dd0e1" },
        { "STAT",  "#ba68c8" },
        { "SUBS",  "#4db6ac" },
        { "HIT",   "#fff176" },
        { "WARN",  "#ffa726" },
        { "ERR",   "#ef5350" },
        { "OK",    "#66bb6a" },
    };
    const auto &c = kCats[static_cast<int>(cat)];

    static const char kParaStyle[] =
        "margin:0; padding:1px 0;"
        "font-family:'Consolas','Courier New',monospace;"
        "font-size:9pt; line-height:1.45;";

    const QString tagCell = QStringLiteral(
        "<td width='52' style='"
        "color:%1; font-weight:700; white-space:nowrap; padding:0 6px 0 0;"
        "vertical-align:top'>%2</td>"
    ).arg(QLatin1String(c.fg), QLatin1String(c.label));

    const QString msgStyle = bold
        ? QStringLiteral("color:%1; font-weight:700").arg(QLatin1String(c.fg))
        : QStringLiteral("color:%1").arg(QLatin1String(c.fg));

    const QString msgCell = QStringLiteral(
        "<td style='%1; vertical-align:top'>%2</td>"
    ).arg(msgStyle, text.toHtmlEscaped());

    return QStringLiteral(
        "<table cellspacing='0' cellpadding='0' width='100%%' style='%1'>"
        "<tr>%2%3</tr></table>"
    ).arg(QLatin1String(kParaStyle), tagCell, msgCell);
}

static LogCat stripPythonTag(QString &msg)
{
    struct Entry { const char *tag; LogCat cat; };
    static const Entry kMap[] = {
        { "[VIDEO]", LogCat::Video   },
        { "[SUBS]",  LogCat::Subs    },
        { "[HIT]",   LogCat::Hit     },
        { "[NET]",   LogCat::Network },
        { "[STAT]",  LogCat::Stats   },
        { "[WARN]",  LogCat::Warning },
        { "[ERR]",   LogCat::Error   },
        { "[INFO]",  LogCat::Info    },
    };
    for (const auto &e : kMap) {
        if (msg.startsWith(QLatin1String(e.tag))) {
            msg = msg.mid(static_cast<int>(qstrlen(e.tag))).trimmed();
            return e.cat;
        }
    }
    return LogCat::Info;
}

static bool isBold(LogCat cat)
{
    return cat == LogCat::Video
        || cat == LogCat::Hit
        || cat == LogCat::Warning
        || cat == LogCat::Error
        || cat == LogCat::Success;
}

static bool isSuccessMessage(const QString &msg)
{
    return msg.startsWith(QLatin1String("Done"), Qt::CaseInsensitive)
        || msg.contains(QLatin1String("Results saved"), Qt::CaseInsensitive);
}

SearchWorker::SearchWorker(const QJsonObject &params, QObject *parent)
    : QObject(parent), m_params(params) {}

void SearchWorker::stop()
{
    m_running.store(false);
    emit logOutput(logLine(LogCat::Warning,
                           QStringLiteral("Stop requested — finishing current video…"),
                           true));
}

void SearchWorker::run()
{
    const auto    startTime = QDateTime::currentDateTime();
    const QString keyword   = m_params.value(QStringLiteral("keyword")).toString();

    emit logOutput(logLine(LogCat::Info,
                           QStringLiteral("Search started — \"%1\"").arg(keyword),
                           true));

    auto progressCb = [this](int pct, const QString &rawMsg) -> bool {
        if (!m_running.load())
            return false;

        emit progressUpdate(pct);

        QString msg = rawMsg.trimmed();
        if (msg.isEmpty())
            return true;

        LogCat  cat = stripPythonTag(msg);

        if (cat == LogCat::Stats && isSuccessMessage(msg))
            cat = LogCat::Success;

        emit logOutput(logLine(cat, msg, isBold(cat)));
        return true;
    };

    const QString resultJson =
        PythonBridge::instance().searchTranscripts(m_params, progressCb);

    if (resultJson.trimmed().isEmpty()) {
        const QString message = m_running.load()
            ? QStringLiteral("Python engine returned an empty response")
            : QStringLiteral("Search cancelled");

        emit logOutput(logLine(
            m_running.load() ? LogCat::Error : LogCat::Warning,
            message,
            true));

        if (m_running.load())
            emit error(message);

        emit progressUpdate(100);
        emit finished(0, {});
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(resultJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString message = QStringLiteral("Invalid Python response JSON: %1")
            .arg(parseError.errorString());
        emit logOutput(logLine(LogCat::Error, message, true));
        emit error(message);
        emit progressUpdate(100);
        emit finished(0, {});
        return;
    }

    QJsonObject obj = doc.object();
    const QString status = obj.value(QStringLiteral("status")).toString(
        QStringLiteral("ok"));
    const QString engineError = obj.value(QStringLiteral("error")).toString();
    const int matchCount = obj.value(QStringLiteral("match_count")).toInt(0);

    QStringList results;
    const QJsonArray arr = obj.value(QStringLiteral("results")).toArray();
    for (const auto &v : arr)
        results.append(v.toString());

    if (status.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0) {
        const QString message = engineError.isEmpty()
            ? QStringLiteral("Python search failed")
            : engineError;
        emit logOutput(logLine(LogCat::Error, message, true));
        emit error(message);
        emit progressUpdate(100);
        emit finished(matchCount, results);
        return;
    }

    const double elapsed =
        startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;

    emit logOutput(logLine(LogCat::Stats,
                           QStringLiteral("Completed in %1 s").arg(elapsed, 0, 'f', 2),
                           true));

    if (status.compare(QStringLiteral("cancelled"), Qt::CaseInsensitive) == 0
        || !m_running.load()) {
        emit logOutput(logLine(
            LogCat::Warning,
            QStringLiteral("Search cancelled by user"),
            true));

        if (matchCount > 0) {
            emit logOutput(logLine(
                LogCat::Success,
                QStringLiteral("%1 partial match%2 captured before cancellation")
                    .arg(matchCount)
                    .arg(matchCount == 1 ? QString{} : QStringLiteral("es")),
                true));
        }

        emit progressUpdate(100);
        emit finished(matchCount, results);
        return;
    }

    if (matchCount > 0) {
        emit logOutput(logLine(
            LogCat::Success,
            QStringLiteral("%1 match%2 found across %3 video%4")
                .arg(matchCount)
                .arg(matchCount == 1 ? QString{} : QStringLiteral("es"))
                .arg(results.size())
                .arg(results.size() == 1 ? QString{} : QStringLiteral("s")),
            true));
    } else {
        emit logOutput(logLine(LogCat::Warning,
                               QStringLiteral("No matches found"),
                               true));
    }

    emit progressUpdate(100);
    emit finished(matchCount, results);
}

}