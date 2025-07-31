#include "stdafx.h"
#include "QDialogHint.h"
#include "MainWindow.h"

QDialogHint::QDialogHint(const QString text, const int time, QWidget *parent)
	: QDialog(parent),
	show_time(time),
	show_text(text)
{
	setWindowTitle(tr("ÌבÊ¾"));
	this->setAttribute(Qt::WA_DeleteOnClose);
	initData();
	initUi();
	setFocus();
}

QDialogHint::~QDialogHint()
{
}

void QDialogHint::create(const QString text, const int time, QWidget* parent, bool staysOnTop, bool bfocusOutClose)
{
	QDialogHint* dlg = new QDialogHint(text, time, parent);
	dlg->setFocusOutClose(bfocusOutClose);
	if (staysOnTop)
		dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);
	dlg->show();
}

void QDialogHint::setText(const QString text)
{
	show_text = text;
	show_label->setText(show_text);
	this->setFixedWidth(getTextWidth(show_label->font(), show_text) + 48);
}

void QDialogHint::setFocusOutClose(bool bfocusOutClose)
{
	m_bFocusOutClose = bfocusOutClose;
}

void QDialogHint::initData()
{
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	//this->setFixedSize(108,30);
	show_label = new QLabel(this);
	show_label->setStyleSheet("QLabel{background-color:rgba(0,0,0,153);color:#F2F4F8;font-size: 12px;font-weight:400;border-radius:2px;padding-top:8px;padding-bottom:8px}");
	show_label->setAlignment(Qt::AlignCenter);
	show_label->setText(show_text);
	this->setFixedWidth(getTextWidth(show_label->font(), show_text) + 48);
}

void QDialogHint::initUi()
{
	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->setMargin(0);
	mainLayout->addWidget(show_label);

	this->setLayout(mainLayout);
}

void QDialogHint::showEvent(QShowEvent* e) {
	QRect parentRect(parentWidget()->mapToGlobal(QPoint(0, 0)),
		parentWidget()->size());
	move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(),
		parentRect).topLeft());

	QTimer::singleShot(show_time, this, [&]
	{
		close();
	});

	QDialog::showEvent(e);
}

void QDialogHint::closeEvent(QCloseEvent* e)
{
}

void QDialogHint::focusOutEvent(QFocusEvent* e) {
	QDialog::focusOutEvent(e);
	if (m_bFocusOutClose)
		close();
}
