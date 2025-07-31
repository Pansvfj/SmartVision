#pragma once

#include <QDialog>
#include <QLabel>

class QDialogHint : public QDialog
{
	Q_OBJECT

public:
	~QDialogHint();

	static void create(const QString text, const int time = 2000, QWidget* parent = nullptr, bool staysOnTop/*ÊÇ·ñ´°¿ÚÖÃ¶¥*/ = false, bool bfocusOutClose = true);
	void setText(const QString text);
	void setFocusOutClose(bool bfocusOutClose);
protected:
	void focusOutEvent(QFocusEvent* e) Q_DECL_OVERRIDE;
	void showEvent(QShowEvent* e) Q_DECL_OVERRIDE;
	void closeEvent(QCloseEvent* e);
private:
	QDialogHint(const QString text, const int time = 2000, QWidget* parent = nullptr);

	void initData();
	void initUi();

	QLabel* show_label;
	QString show_text;

	int show_time = 2000;

	bool m_bFocusOutClose = false;
};
