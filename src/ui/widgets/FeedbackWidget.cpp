#include "FeedbackWidget.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace CapScript {

HeartWidget::HeartWidget(int index, QWidget *parent)
    : QWidget(parent), m_index(index) {
  setFixedSize(30, 30);
  setCursor(Qt::PointingHandCursor);
}

void HeartWidget::setFilled(bool filled) {
  if (m_filled != filled) {
    m_filled = filled;
    update();
  }
}

void HeartWidget::enterEvent(QEnterEvent *event) {
  emit hovered(m_index);
  QWidget::enterEvent(event);
}

void HeartWidget::leaveEvent(QEvent *event) {
  emit unhovered();
  QWidget::leaveEvent(event);
}

void HeartWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit clicked(m_index);
  }
  QWidget::mouseReleaseEvent(event);
}

void HeartWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  QIcon icon(m_filled ? ":/icons/heart-pink.svg" : ":/icons/heart.svg");
  int size = qMin(width(), height());
  icon.paint(&p, (width() - size) / 2, (height() - size) / 2, size, size);
}

FeedbackPanel::FeedbackPanel(QWidget *parent) : QWidget(parent) {

  setObjectName("feedbackPanel");
  setFixedSize(350, 320);

  if (parent)
    parent->installEventFilter(this);

  m_networkManager = new QNetworkAccessManager(this);
  connect(m_networkManager, &QNetworkAccessManager::finished, this,
          &FeedbackPanel::onNetworkReply);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(15, 15, 15, 15);
  mainLayout->setSpacing(8);

  auto *topLayout = new QHBoxLayout;
  m_titleLabel = new QLabel("Feedback", this);
  m_titleLabel->setStyleSheet(
      "font-weight: bold; font-size: 11pt; color: #fff;");

  m_closeBtn = new QPushButton(this);
  m_closeBtn->setIcon(QIcon(":/icons/close.svg"));
  m_closeBtn->setIconSize(QSize(10, 10));
  m_closeBtn->setFixedSize(20, 20);

  m_closeBtn->setStyleSheet(R"(
        QPushButton {
            border: none;
            background: transparent;
            border-radius: 10px;
            padding: 0px;
            min-width: 20px; max-width: 20px;
            min-height: 20px; max-height: 20px;
        }
        QPushButton:hover {
            background: rgba(120,120,120,0.35);
        }
    )");
  connect(m_closeBtn, &QPushButton::clicked, this, &FeedbackPanel::hidePanel);

  topLayout->addWidget(m_titleLabel);
  topLayout->addStretch();
  topLayout->addWidget(m_closeBtn);
  mainLayout->addLayout(topLayout);

  auto *heartsLayout = new QHBoxLayout;
  heartsLayout->setSpacing(5);
  heartsLayout->addStretch();
  for (int i = 0; i < 5; ++i) {
    auto *heart = new HeartWidget(i, this);
    connect(heart, &HeartWidget::hovered, this, &FeedbackPanel::onHeartHovered);
    connect(heart, &HeartWidget::unhovered, this,
            &FeedbackPanel::onHeartUnhovered);
    connect(heart, &HeartWidget::clicked, this, &FeedbackPanel::onHeartClicked);
    m_hearts.append(heart);
    heartsLayout->addWidget(heart);
  }
  heartsLayout->addStretch();
  mainLayout->addLayout(heartsLayout);

  m_emailInput = new QLineEdit(this);
  m_emailInput->setPlaceholderText("Email (optional, for follow-up)...");
  m_emailInput->setMaxLength(254);
  m_emailInput->setStyleSheet(
      "QLineEdit { background: #1e1e1e; border: 1px solid #333; border-radius: "
      "5px; padding: 5px; color: #fff; }");
  mainLayout->addWidget(m_emailInput);

  m_feedbackText = new QTextEdit(this);
  m_feedbackText->setPlaceholderText("Anything to fix? add? please tell me <3");
  m_feedbackText->setStyleSheet(
      "QTextEdit { background: #1e1e1e; border: 1px solid #333; border-radius: "
      "5px; padding: 5px; color: #fff; }");
  mainLayout->addWidget(m_feedbackText);

  auto *bottomLayout = new QHBoxLayout;
  m_statusLabel = new QLabel(this);
  m_statusLabel->setStyleSheet("color: #ccc; font-size: 8pt;");

  m_sendBtn = new QPushButton("Send", this);
  m_sendBtn->setStyleSheet(
      "QPushButton { background: #e91e63; color: white; border: none; "
      "border-radius: 5px; padding: 6px 12px; font-weight: bold; } "
      "QPushButton:hover { background: #d81b60; } QPushButton:pressed { "
      "background: #c2185b; }");
  connect(m_sendBtn, &QPushButton::clicked, this, &FeedbackPanel::onSend);

  bottomLayout->addWidget(m_statusLabel);
  bottomLayout->addStretch();
  bottomLayout->addWidget(m_sendBtn);
  mainLayout->addLayout(bottomLayout);

  m_selectedIndex = -1;
  m_hoveredIndex = -1;
  updateHearts();

  hide();
}

bool FeedbackPanel::isValidEmail(const QString &email) {
  QRegularExpression regex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
  return regex.match(email).hasMatch();
}

void FeedbackPanel::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void FeedbackPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(QColor("#181818"));
  p.setPen(QPen(QColor("#555555"), 1.5));
  p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
}

void FeedbackPanel::onHeartHovered(int index) {
  m_hoveredIndex = index;
  updateHearts();
}

void FeedbackPanel::onHeartUnhovered() {
  m_hoveredIndex = -1;
  updateHearts();
}

void FeedbackPanel::onHeartClicked(int index) {
  m_selectedIndex = index;
  updateHearts();
}

void FeedbackPanel::updateHearts() {
  int maxFilled = m_hoveredIndex != -1 ? m_hoveredIndex : m_selectedIndex;
  for (int i = 0; i < 5; ++i) {
    m_hearts[i]->setFilled(i <= maxFilled);
  }
}

void FeedbackPanel::togglePanel() {
  if (m_isOpen)
    hidePanel();
  else
    showPanel();
}

void FeedbackPanel::showPanel() {
  if (m_isOpen)
    return;
  m_isOpen = true;
  reanchor();
  show();
  raise();
}

void FeedbackPanel::hidePanel() {
  if (!m_isOpen)
    return;
  m_isOpen = false;
  hide();
}

void FeedbackPanel::animatePanel(bool) {}

void FeedbackPanel::reanchor() {
  if (!parentWidget())
    return;
  move(10, parentWidget()->height() - height() - 10);
}

void FeedbackPanel::moveEvent(QMoveEvent *event) {
  QWidget::moveEvent(event);

  if (m_isOpen && parentWidget()) {
    int expectedX = 10;
    int expectedY = parentWidget()->height() - height() - 10;
    if (pos().x() != expectedX || pos().y() != expectedY) {
      move(expectedX, expectedY);
    }
  }
}

bool FeedbackPanel::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize && m_isOpen)
    reanchor();
  return QWidget::eventFilter(watched, event);
}


void FeedbackPanel::onSend() {
  QString text = m_feedbackText->toPlainText().trimmed();
  QString email = m_emailInput->text().trimmed();

  if (text.isEmpty() && m_selectedIndex == -1) {
    m_statusLabel->setStyleSheet("color: #ff9800;");
    m_statusLabel->setText("Please add text or rating.");
    return;
  }

  if (text.length() > 1000) {
    m_statusLabel->setStyleSheet("color: #ff9800;");
    m_statusLabel->setText("Feedback too long (max 1000).");
    return;
  }

  if (!email.isEmpty() && !isValidEmail(email)) {
    m_statusLabel->setStyleSheet("color: #ff9800;");
    m_statusLabel->setText("Please enter a valid email address.");
    return;
  }

  m_statusLabel->setStyleSheet("color: #ccc;");
  m_statusLabel->setText("Sending...");
  m_sendBtn->setEnabled(false);

  QNetworkRequest request(
      QUrl("https://capscript-feedback-worker.devbit-server.workers.dev"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["rating"] = m_selectedIndex + 1;
  json["email"] = email; 
  json["text"] = text.isEmpty() ? "" : text;

  m_pendingReply =
      m_networkManager->post(request, QJsonDocument(json).toJson());

  QTimer::singleShot(30000, this, [this]() {
    if (m_pendingReply) {
      m_pendingReply->abort();
    }
  });
}

void FeedbackPanel::onNetworkReply(QNetworkReply *reply) {
  m_pendingReply = nullptr;
  m_sendBtn->setEnabled(true);

  if (reply->error() == QNetworkReply::OperationCanceledError) {
    m_statusLabel->setStyleSheet("color: #f44336;");
    m_statusLabel->setText("Request timed out.");
    reply->deleteLater();
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 200 && httpStatus < 300) {
      m_statusLabel->setStyleSheet("color: #4caf50;");
      m_statusLabel->setText("Feedback sent thanks!");
      m_feedbackText->clear();
      m_selectedIndex = -1;
      updateHearts();
      QTimer::singleShot(2500, this, &FeedbackPanel::hidePanel);
    } else {
      m_statusLabel->setStyleSheet("color: #f44336;");
      m_statusLabel->setText(QString("Server error (%1).").arg(httpStatus));
    }
  } else {

    m_statusLabel->setStyleSheet("color: #f44336;");

    QString errorDetail = reply->errorString();
    if (errorDetail.length() > 60)
      errorDetail = errorDetail.left(57) + "...";
    m_statusLabel->setText(errorDetail);
  }
  reply->deleteLater();
}

} // namespace CapScript
