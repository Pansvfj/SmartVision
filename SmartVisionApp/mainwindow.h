#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QString>
#include <QPixmap>
#include <QTextToSpeech>
#include <QThread>
#include <QPushButton>

class ModelRunner;
class TranslateTask;
class ModelWork;
class YoloDetector;
class YoloWork;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

	static MainWindow* getMainWindow();

signals:
	void signalStartModelWork(const QString& filePath);
	void signalStartGeneralWork(const QString& filePath);
	void signalStartFishYoloWork(const QString& filePath);

public slots:
	void onModelResultReceived(bool success, const GetModelResultType& result);
	void onYoloResultReceived(bool success, const QPair<QImage, QStringList>& result);
	void onTranslationFinished(const QString& result);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void playText(const QString& text);

private:
	QLabel* m_lbImage = nullptr;
	QLabel* m_lbYoloImage = nullptr;
    QString m_filePath;
	QPixmap m_pixmap;
	QPixmap m_YoloPixmap;
    TranslateTask* m_translator = nullptr;
	QTextToSpeech* m_textToSpeech = nullptr;
	QThread* m_thread = nullptr;
	QTextBrowser* m_txtBrowserYolo = nullptr;
	QTextBrowser* m_txtBrowserFish = nullptr;
	QTextBrowser* m_txtBrowser = nullptr;
	QPushButton* m_pbYolo = nullptr;
	QPushButton* m_pbYoloFish = nullptr;

	ModelRunner* m_model = nullptr;
	ModelWork* m_modelWork = nullptr;
	QThread m_modelThread;

	YoloDetector* m_generalYoloDetector = nullptr;
	YoloWork* m_generalYoloWork = nullptr;
	QThread m_generalYoloThread;

	YoloDetector* m_fishYoloDetector = nullptr;
	YoloWork* m_fishYoloWork = nullptr;
	QThread m_fishYoloThread;
};
