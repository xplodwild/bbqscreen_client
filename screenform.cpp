#include "stdafx.h"
#include "screenform.h"
#include "ui_screenform.h"
#include "mainwindow.h"
#include <QByteArray>
#include <QPainter>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFile>
#include <QtNetwork/QHostAddress>
#if defined(__APPLE__)
 #include <Carbon/Carbon.h>
#endif

#define INPUT_PROTOCOL_VERSION 1

//#define PROFILING

//----------------------------------------------------
//----------------------------------------------------
ShrinkableQLabel::ShrinkableQLabel(QWidget* parent /* = 0 */) : QLabel(parent),
	mHighQuality(false)
{

}
//----------------------------------------------------
void ShrinkableQLabel::setHighQuality(bool high)
{
	mHighQuality = high;
}
//---------------------------------------------------
void ShrinkableQLabel::paintEvent(QPaintEvent *aEvent)
{
	QLabel::paintEvent(aEvent);
	_displayImage();
}
//----------------------------------------------------
void ShrinkableQLabel::setPixmap(const QPixmap& aPicture)
{
	mSource = mCurrent = aPicture;
	repaint();
}
//----------------------------------------------------
void ShrinkableQLabel::_displayImage()
{
	if (mSource.isNull()) //no image was set, don't draw anything
		return;

	float cw = width(), ch = height();
	float pw = mCurrent.width(), ph = mCurrent.height();

	if (pw > cw && ph > ch && pw/cw > ph/ch || //both width and high are bigger, ratio at high is bigger or
		pw > cw && ph <= ch || //only the width is bigger or
		pw < cw && ph < ch && cw/pw < ch/ph //both width and height is smaller, ratio at width is smaller
		)
		mCurrent = mSource.scaledToWidth(cw, mHighQuality ? Qt::SmoothTransformation : Qt::TransformationMode::FastTransformation);
	else if (pw > cw && ph > ch && pw/cw <= ph/ch || //both width and high are bigger, ratio at width is bigger or
		ph > ch && pw <= cw || //only the height is bigger or
		pw < cw && ph < ch && cw/pw > ch/ph //both width and height is smaller, ratio at height is smaller
		)
		mCurrent = mSource.scaledToHeight(ch, mHighQuality ? Qt::SmoothTransformation : Qt::TransformationMode::FastTransformation);

	int x = (cw - mCurrent.width())/2, y = (ch - mCurrent.height())/2;

	QPainter paint(this);
	paint.drawPixmap(x, y, mCurrent);
}
//----------------------------------------------------
QSize ShrinkableQLabel::getRenderSize()
{
	return mCurrent.size();
}
//----------------------------------------------------
//----------------------------------------------------
ScreenForm::ScreenForm(MainWindow* win, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ScreenForm),
	mTotalFrameReceived(0),
	mRotationAngle(0),
	mParentWindow(win),
	mOrientationOffset(0),
	mShowFps(false),
	mFuckingStop(false),
	mCtrlDown(false),
	mIsMouseDown(false)
{
	ui->setupUi(this);

	// Start TCP client
	connect(&mTcpSocket, SIGNAL(readyRead()),
		this, SLOT(processPendingDatagrams()));

	connect(&mTcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged()));

	mFrameTimer.start();

	// The client runs at a refresh rate of 35 fps whereas the server is locked at 25 fps.
	// This is to ensure both smoothness and responsiveness
	startTimer(1000/35, Qt::PreciseTimer);
}
//----------------------------------------------------
ScreenForm::~ScreenForm()
{
	//mTcpSocket.close();

	if (ui)
		delete ui;

	ui = nullptr;
}
//----------------------------------------------------
void ScreenForm::connectTo(const QString &host)
{
	mHost = host;
	this->setWindowTitle("BBQScreen - " + host);

	qDebug() << "Connecting to " << host;
	mIsConnecting = true;

	mTcpSocket.connectToHost(QHostAddress(mHost), 9876);

	QTime connectionTimeout;
	connectionTimeout.start();
	int attempt = 1;

	while (mTcpSocket.state() != QAbstractSocket::ConnectedState && attempt < 4)
	{
		if (!ui)
			return;

		ui->lblFps->setText("Connecting to '" + mHost + "'... (Attempt " + QString::number(attempt) + "/3)");
		qApp->processEvents();


		if (connectionTimeout.elapsed() > 1000)
		{
			mTcpSocket.disconnectFromHost();
			mTcpSocket.waitForDisconnected(1000);
			mTcpSocket.connectToHost(QHostAddress(mHost), 9876);
			connectionTimeout.restart();
			attempt++;
		}
	}

	if (mTcpSocket.state() != QAbstractSocket::ConnectedState)
	{
		QMessageBox::critical(this, "Could not connect", "Unable to connect to " + host + " after 3 attempts. Please check the device IP, and make sure your screen is unlocked.\nError message: " + mTcpSocket.errorString());
		close();
	}

	mIsConnecting = false;
	mFrameTimer.start();
	mTimeLastFrame = mFrameTimer.elapsed();

	if (!mShowFps)
	{
		ui->lblFps->setText("");
		ui->lblFps->setVisible(false);
	}
}
//----------------------------------------------------
void ScreenForm::setQuality(bool high)
{
	mHighQuality = high;
	ui->lblDisplay->setHighQuality(high);
}
//----------------------------------------------------
void ScreenForm::setShowFps(bool show)
{
	mShowFps = show;
}
//----------------------------------------------------
void ScreenForm::processPendingDatagrams()
{
	if (!isVisible() || !ui || mFuckingStop)
		return;

	int currentSocket = 0;
    const int headerSize = 6;

	while (mTcpSocket.bytesAvailable() > 0)
	{
		if (!isVisible() || !ui || mFuckingStop)
			return;
#ifdef PROFILING
		int timeSinceLastFrame = mFrameTimer.elapsed();
		qDebug() << "Got datagram after " << timeSinceLastFrame << " ms";
		mFrameTimer.restart();
#endif

		//qDebug() << "Has packet of " << mTcpSocket.bytesAvailable() << " bytes";
		// Read the first pending packet
		mBytesBuffer.append(mTcpSocket.readAll());

		int remainSize = mBytesBuffer.size();

		while (mBytesBuffer.size() > 6)
		{
			// Read header
			quint8 headerSize = 6, protVersion, orientation;
			quint32 frameSize;

			protVersion = bytesToUInt8(mBytesBuffer, 0);
			orientation = bytesToUInt8(mBytesBuffer, 1);
			frameSize = bytesToUInt32(mBytesBuffer, 2);

            //qDebug() << "Frame is " << frameSize << " bytes (prot " << protVersion << " ori " << orientation << ")";

			if (frameSize > mBytesBuffer.size()-headerSize)
			{
				//qDebug() << "Will revisit later (" << (mBytesBuffer.size()-headerSize) << "/" << frameSize << ")";
				break;
			}

			mBytesBuffer = mBytesBuffer.remove(0,headerSize);

            if (mDecoder.decodeFrame((unsigned char*)mBytesBuffer.data(), frameSize))
			{
				QImage img = mDecoder.getLastFrame();
				mRotationAngle = orientation * (-90) + mOrientationOffset;

				mOriginalSize.setX(img.width());
				mOriginalSize.setY(img.height());

				if (mRotationAngle != 0)
				{
					QTransform t;
					t.rotate(mRotationAngle);
					img = img.transformed(t, mHighQuality ? Qt::SmoothTransformation : Qt::FastTransformation);
				}

				mLastPixmap.append(QPixmap::fromImage(img));

				mTotalFrameReceived++;
				mTimeLastFrame = mFrameTimer.elapsed();

#ifdef PROFILING
				timeSinceLastFrame = mFrameTimer.elapsed();
				qDebug() << "Decoded in " << timeSinceLastFrame << " ms";
#endif
				if (!isVisible() || !ui || mFuckingStop)
					return;

				if (mShowFps)
					ui->lblFps->setText(QString::number((double)(mTotalFrameReceived/(mFrameTimer.elapsed()/1000.0))) + " fps");
				else
					ui->lblFps->setText("");
			}
			else
			{
				qDebug() << "Error decoding frame";
			}

			mBytesBuffer = mBytesBuffer.remove(0, frameSize);
		}
	}
}
//----------------------------------------------------
void ScreenForm::onSocketStateChanged()
{
	if (mIsConnecting || !isVisible() || !ui || mFuckingStop)
		return;

	if (mTcpSocket.state() == QAbstractSocket::UnconnectedState)
	{
		ui->lblFps->setText("Lost connection with host device. Reconnecting...");
		mIsConnecting = true;

		QTime connectionTimeout;
		connectionTimeout.start();
		int attempt = 1;

		while (mTcpSocket.state() != QAbstractSocket::ConnectedState)
		{
			if (!isVisible() || !ui)
				return;

			ui->lblFps->setText("Connecting... (Attempt " + QString::number(attempt) + "...)");
			qApp->processEvents();


			if (connectionTimeout.elapsed() > 1000)
			{
				mTcpSocket.disconnectFromHost();
				mTcpSocket.waitForDisconnected(1000);
				mTcpSocket.connectToHost(QHostAddress(mHost), 9876);
				connectionTimeout.restart();
				attempt++;
			}
		}

		mIsConnecting = false;
	}
}
//----------------------------------------------------
void ScreenForm::timerEvent(QTimerEvent *evt)
{
	if (mFuckingStop)
		return;

	if (mLastPixmap.size() == 0)
		return;

	while (mLastPixmap.size() >= 3)
		mLastPixmap.pop_front();

	// Display next frame
	ui->lblDisplay->clear();
	ui->lblDisplay->setPixmap(mLastPixmap.front());
	mLastPixmap.pop_front();
}
//----------------------------------------------------
#if defined(__APPLE__)
bool ScreenForm::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    EventRef* event = reinterpret_cast<EventRef*>(message);
    uint32_t keyCode;

    switch (GetEventClass(*event)) {
    case kEventRawKeyDown:
        qDebug() << "key down mac";
        GetEventParameter(*event, kEventParamKeyCode, typeUInt32, NULL, sizeof(keyCode), NULL, &keyCode);
        qDebug() << keyCode;
        break;

    case kEventRawKeyUp:
        qDebug() << "key up mac";
        GetEventParameter(*event, kEventParamKeyCode, typeUInt32, NULL, sizeof(keyCode), NULL, &keyCode);
        qDebug() << keyCode;
        break;

    default:
        qDebug() << "event: " << GetEventClass(*event);
        break;
    }

    return QWidget::nativeEvent(eventType, message, result);
}
#endif
//----------------------------------------------------
void ScreenForm::keyReleaseEvent(QKeyEvent *evt)
{
	switch (evt->key())
	{
	case Qt::Key_Control:
		mCtrlDown = false;
		break;

	default:
		if (!mCtrlDown)
		{
			// Route the key to the server
#if defined(_WIN32) || defined(_WIN64)
			sendKeyboardInput(false, evt->nativeScanCode());
#elif defined(__APPLE__)
            sendKeyboardInput(false, evt->key());
#else
#error "Unsupported keyboard platform"
#endif
		}
		break;
	}
}
//----------------------------------------------------
void ScreenForm::keyPressEvent(QKeyEvent *evt)
{
	switch (evt->key())
	{
	case Qt::Key_Control:
		mCtrlDown = true;
		break;

	default:
		if (!mCtrlDown)
		{
			// Route the key to the server
#if defined(_WIN32) || defined(_WIN64)
            sendKeyboardInput(false, evt->nativeScanCode());
#elif defined(__APPLE__)
            sendKeyboardInput(false, evt->key());
#else
#error "Unsupported keyboard platform"
#endif
		}
		break;
	}

	if (mCtrlDown)
	{
		switch (evt->key())
		{
		case Qt::Key_F:
			if (windowState() & Qt::WindowFullScreen)
				setWindowState(windowState() & ~Qt::WindowFullScreen);
			else
				setWindowState(windowState() ^ Qt::WindowFullScreen);
			break;

		case Qt::Key_O:
			mOrientationOffset -= 90;
			if (mOrientationOffset == -360)
				mOrientationOffset = 0;
			break;

		case Qt::Key_P:
			mOrientationOffset += 90;
			if (mOrientationOffset == 360)
				mOrientationOffset = 0;
			break;
		}
	}

}
//----------------------------------------------------
void ScreenForm::mousePressEvent(QMouseEvent *evt)
{
	if (evt->button() == Qt::LeftButton)
	{
		mIsMouseDown = true;
		float posX = evt->x();
		float posY = evt->y();
		QSize imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - (width() / 2.0f - imgSz.width() / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());
		
		QPoint pos = getScreenSpacePoint(posX, posY);
		sendTouchInput(TET_DOWN, 0, pos.x(), pos.y());
	}
}
//----------------------------------------------------
void ScreenForm::mouseReleaseEvent(QMouseEvent *evt)
{
	if (evt->button() == Qt::LeftButton)
	{
		mIsMouseDown = false;
		float posX = evt->x();
		float posY = evt->y();
		QSize imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - (width() / 2.0f - imgSz.width() / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());

		QPoint pos = getScreenSpacePoint(posX, posY);
		sendTouchInput(TET_UP, 0, pos.x(), pos.y());
	}
}
//----------------------------------------------------
void ScreenForm::mouseMoveEvent(QMouseEvent *evt)
{
	if (mIsMouseDown)
	{
		float posX = evt->x();
		float posY = evt->y();
		QSize imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - (width() / 2.0f - imgSz.width() / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());

		QPoint pos = getScreenSpacePoint(posX, posY);
		sendTouchInput(TET_MOVE, 0, pos.x(), pos.y());
	}
}
//----------------------------------------------------
quint8 ScreenForm::bytesToUInt8(const QByteArray& bytes, int offset)
{
	return ((unsigned char) (bytes.at(offset)));
}
//----------------------------------------------------
quint16 ScreenForm::bytesToUInt16(const QByteArray& bytes, int offset)
{
	quint16 value = 0;
	for (int i = 0; i < 2; i++) {
		value += ((unsigned char) bytes.at(offset + i)) << (8 * (1-i));
	}
	return value;
}
//----------------------------------------------------
quint32 ScreenForm::bytesToUInt32(const QByteArray& bytes, int offset)
{
	quint32 value = 0;
	for (int i = 0; i < 4; i++) {
		value += ((unsigned char) bytes.at(offset + i)) << ((3 - i) * 8);
	}
	return value;
}
//----------------------------------------------------
void ScreenForm::closeEvent(QCloseEvent *evt)
{
	mParentWindow->show();
	QWidget::closeEvent(evt);
	mParentWindow->notifyScreenClose(this);
	mFuckingStop = true;
}
//----------------------------------------------------
void ScreenForm::sendKeyboardInput(bool down, uint32_t keyCode)
{
	if (mTcpSocket.state() != QAbstractSocket::ConnectedState) return;

	QByteArray packet;
	packet.append((char)IET_KEYBOARD);
	packet.append(down ? '\x01' : '\x00');
	packet.append(numberToBytes(keyCode, 4));

    qDebug() << "Keyboard pressed: " << keyCode;

	mTcpSocket.write(packet);
	mTcpSocket.flush();
}
//----------------------------------------------------
void ScreenForm::sendTouchInput(TouchEventType type, uint8_t finger, uint16_t x, uint16_t y)
{
	if (mTcpSocket.state() != QAbstractSocket::ConnectedState) return;

	QByteArray packet;
	packet.append((char)IET_TOUCH);
	packet.append((char)type);
	packet.append(finger);
	packet.append(numberToBytes(x, 2));
	packet.append(numberToBytes(y, 2));

	mTcpSocket.write(packet);
	mTcpSocket.flush();
}
//----------------------------------------------------
QByteArray ScreenForm::numberToBytes(unsigned int value, int size)
{
	QByteArray output;

	for (int i = 0; i < size; i++)
	{
		// value >> 2-1-0
		output.push_back((value >> ((size - 1 - i) * 8)) & 0xFF);
	}

	return output;
}
//----------------------------------------------------
QPoint ScreenForm::getScreenSpacePoint(int x, int y)
{
	QPoint output;
	output.setX(x * mOriginalSize.x() / ui->lblDisplay->width());
	output.setY(y * mOriginalSize.y() / ui->lblDisplay->height());

	return output;
}
//----------------------------------------------------
