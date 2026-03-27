#include "SupportPopup.h"
#include "../../core/UrlLauncher.h"
#include <QEvent>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QSvgRenderer>

namespace CapScript {

SupportPopup::SupportPopup(QWidget *parent) : QWidget(parent) {

  setAttribute(
      Qt::WA_NoSystemBackground);

  setFixedSize(305, 158);

  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(60, 60);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  {
    QFile svgFile(":/icons/cute2.svg");
    bool rendered = false;
    if (svgFile.open(QIODevice::ReadOnly)) {
      QByteArray svgData = svgFile.readAll();

      QByteArray innerTag =
          "<svg xmlns=\"http://www.w3.org/2000/svg\" enable-background";
      int start = svgData.indexOf(innerTag);
      if (start != -1) {
        svgData = svgData.mid(start);

        int last = svgData.lastIndexOf("</svg>");
        if (last != -1) {
          svgData = svgData.left(last);
          int inner = svgData.lastIndexOf("</svg>");
          if (inner != -1)
            svgData = svgData.left(inner + 6);
        }
      }
      QSvgRenderer renderer(svgData);
      if (renderer.isValid()) {

        const qreal dpr = devicePixelRatioF();
        QPixmap px(static_cast<int>(60 * dpr), static_cast<int>(60 * dpr));
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&p, QRectF(0, 0, 60 * dpr, 60 * dpr));
        p.end();
        px.setDevicePixelRatio(dpr);
        m_iconLabel->setPixmap(px);
        rendered = true;
      }
    }
    if (!rendered) {
      m_iconLabel->setText("💕");
      m_iconLabel->setStyleSheet("font-size: 26px;");
    }
  }

  m_closeBtn = new QPushButton("✕", this);
  m_closeBtn->setFixedSize(20, 20);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setStyleSheet(R"(
        QPushButton {
            border: none;
            border-radius: 10px;
            background: transparent;
            color: rgba(255,255,255,0.55);
            font-size: 11px;
            font-weight: bold;
            padding: 0px;
        }
        QPushButton:hover {
            background: rgba(0,0,0,0.22);
            color: rgba(255,255,255,0.28);
        }
    )");
  connect(m_closeBtn, &QPushButton::clicked, this, &SupportPopup::hidePopup);

  m_titleLabel = new QLabel("Love CapScript? \xf0\x9f\x92\x95", this);
  m_titleLabel->setStyleSheet(
      "font-weight: bold; font-size: 13px; color: #ffffff;");
  m_titleLabel->setWordWrap(true);

  m_descLabel =
      new QLabel("Help keep the magic going for you and others \xe2\x80\x94 "
                 "your support means everything to me! \xe2\x98\x95",
                 this);
  m_descLabel->setWordWrap(true);
  m_descLabel->setStyleSheet("color: rgba(255,255,255,0.85); font-size: 9pt;");

  m_supportBtn = new QPushButton("", this);
  m_supportBtn->setIconSize(QSize(16, 16));
  m_supportBtn->setCursor(Qt::PointingHandCursor);
  m_supportBtn->setText("Buy Me a Coffee ^_^");
  m_supportBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(0,0,0,0.28);
            color: #ffffff;
            border: 1px solid rgba(255,255,255,0.75);
            border-radius: 10px;
            padding: 5px 16px;
            font-weight: bold;
            font-size: 10pt;
        }
        QPushButton:hover {
            background-color: rgba(0,0,0,0.42);
            border-color: rgba(255,255,255,0.9);
        }
        QPushButton:pressed {
            background-color: rgba(0,0,0,0.55);
        }
    )");
  connect(m_supportBtn, &QPushButton::clicked, this, [this]() {
    openExternalUrl(QUrl("https://ko-fi.com/serptail"));
    hidePopup();
  });

  auto *topRow = new QHBoxLayout;
  topRow->setContentsMargins(0, 0, 0, 0);
  topRow->setSpacing(8);
  topRow->addWidget(m_iconLabel, 0, Qt::AlignVCenter);

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(3);
  textCol->addWidget(m_titleLabel);
  textCol->addWidget(m_descLabel);
  topRow->addLayout(textCol, 1);
  topRow->addWidget(m_closeBtn, 0, Qt::AlignTop);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(14, 10, 12, 12);
  mainLayout->setSpacing(8);
  mainLayout->addLayout(topRow);
  mainLayout->addWidget(m_supportBtn, 0, Qt::AlignCenter);

  m_opacityEffect = new QGraphicsOpacityEffect(this);
  setGraphicsEffect(m_opacityEffect);
  m_opacityEffect->setOpacity(0.0);

  m_opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
  m_opacityAnimation->setDuration(300);

  if (parentWidget()) {
    parentWidget()->installEventFilter(this);
  }

  hide();
}

void SupportPopup::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QPainterPath path;
  path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 14, 14);

  painter.fillPath(path, QColor("#FF92D3"));

  painter.setPen(QPen(QColor("#E052B3"), 1.5));
  painter.drawPath(path);
}

void SupportPopup::showPopup() {
  if (!parentWidget())
    return;

  int x = parentWidget()->width() - width() - 24;
  if (x < 24)
    x = 24;
  int y = parentWidget()->height() - height() - 24;
  move(x, y);

  show();
  raise();
  m_opacityAnimation->stop();
  m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
  m_opacityAnimation->setEndValue(1.0);
  m_opacityAnimation->start();
}

void SupportPopup::hidePopup() {
  m_opacityAnimation->stop();
  m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
  m_opacityAnimation->setEndValue(0.0);
  connect(m_opacityAnimation, &QPropertyAnimation::finished, this,
          &QWidget::hide, Qt::UniqueConnection);
  m_opacityAnimation->start();
}

bool SupportPopup::eventFilter(QObject *obj, QEvent *event) {

  if (obj == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
    int x = parentWidget()->width() - width() - 24;
    if (x < 24)
      x = 24;
    int y = parentWidget()->height() - height() - 24;
    move(x, y);
  }
  return QWidget::eventFilter(obj, event);
}

}
