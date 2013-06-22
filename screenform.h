#ifndef SCREENFORM_H
#define SCREENFORM_H

#include <QtWidgets/QWidget>
#include <QtNetwork/QTcpSocket>
#include <QTime>
#include <QPainter>
#include <QBuffer>
#include <QLabel>
#include "QStreamDecoder.h"

#define FPS_AVERAGE_SAMPLES 50

enum TouchEventType {
	TET_UP,
	TET_DOWN,
	TET_MOVE
};

enum InputEventType {
	IET_KEYBOARD,
	IET_TOUCH
};

namespace Ui {
	class ScreenForm;
}

class MainWindow;

class ShrinkableQLabel : public QLabel
{
	Q_OBJECT;

public:
	ShrinkableQLabel(QWidget* parent = 0);
	void paintEvent(QPaintEvent *aEvent);
	void setPixmap(const QPixmap& aPicture);
	void setHighQuality(bool high);
	QSize getRenderSize();

protected:
	void _displayImage();

	QPixmap mSource;
	QPixmap mCurrent;
	bool mHighQuality;
};

class ScreenForm : public QWidget
{
	Q_OBJECT

public:
	explicit ScreenForm(MainWindow* win, QWidget *parent = 0);
	~ScreenForm();

	void connectTo(const QString& host);

	void timerEvent(QTimerEvent *evt);
	void closeEvent(QCloseEvent *evt);
	void keyPressEvent(QKeyEvent *evt);
	void keyReleaseEvent(QKeyEvent *evt);
	void mousePressEvent(QMouseEvent *evt);
	void mouseReleaseEvent(QMouseEvent *evt);
	void mouseMoveEvent(QMouseEvent *evt);

	quint8 bytesToUInt8(const QByteArray& bytes, int offset);
	quint16 bytesToUInt16(const QByteArray& bytes, int offset);
	quint32 bytesToUInt32(const QByteArray& bytes, int offset);
	QByteArray numberToBytes(unsigned int value, int size);

	void setQuality(bool high);
	void setShowFps(bool show);

	void sendKeyboardInput(bool down, uint32_t keyCode);
	void sendTouchInput(TouchEventType type, uint8_t finger, uint16_t x, uint16_t y);

	QPoint getScreenSpacePoint(int x, int y);

#ifdef __APPLE__
    bool nativeEvent(const QByteArray& eventType, void* message, long* result);
#endif

private slots:
	void processPendingDatagrams();
	void onSocketStateChanged();

private:
	Ui::ScreenForm *ui;
	MainWindow* mParentWindow;

	bool mIsMouseDown;

	QTcpSocket mTcpSocket;
	int mTotalFrameReceived;
	int mRotationAngle;
	QTime mFrameTimer;
	unsigned long mTimeLastFrame;
	bool mShowFps;
	QString mHost;
	bool mHighQuality;
	bool mIsConnecting;
	QList<QPixmap> mLastPixmap;
	int mOrientationOffset;
	bool mCtrlDown;
	QPoint mOriginalSize;

	QByteArray mBytesBuffer;

	QStreamDecoder mDecoder;
	bool mFuckingStop;
};

#endif // SCREENFORM_H
