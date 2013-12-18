/**
 * Copyright (C) 2013 Guillaume Lesniak
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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
#include <QtGui/QPixmap>

#ifdef PLAT_APPLE
 #include <Carbon/Carbon.h>
#endif

#define INPUT_PROTOCOL_VERSION 1

//#define PROFILING

//----------------------------------------------------
ScreenForm::ScreenForm(MainWindow* win, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ScreenForm),
	mTotalFrameReceived(0),
	mRotationAngle(0),
	mParentWindow(win),
	mOrientationOffset(0),
	mShowFps(false),
	mStopped(false),
	mCtrlDown(false),
	mIsMouseDown(false),
	mDecoder(false),
	mAudioDecoder(true)
{
	ui->setupUi(this);

	// Run decoder in separate threads
	mDecoder.moveToThread(&mVideoDecoderThread);
	mAudioDecoder.moveToThread(&mAudioDecoderThread);
	connect(&mVideoDecoderThread, SIGNAL(started()), &mDecoder, SLOT(process()));
	connect(&mAudioDecoderThread, SIGNAL(started()), &mDecoder, SLOT(process()));

	connect(&mDecoder, SIGNAL(decodeFinished(bool, bool)), this, SLOT(onDecodeFinished(bool, bool)));
	connect(&mAudioDecoder, SIGNAL(decodeFinished(bool, bool)), this, SLOT(onDecodeFinished(bool, bool)));

	// Start TCP client
	connect(&mTcpSocket, SIGNAL(readyRead()),
		this, SLOT(processPendingDatagrams()));

	connect(&mTcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged()));

	mFrameTimer.start();

	// On protocol v3, the client runs at a refresh rate of 35 fps whereas
	// the server is locked at 25 fps. On protocol v4, the stream is at 60fps
	// This is to ensure both smoothness and responsiveness
	startTimer(1, Qt::PreciseTimer);
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

	QTime connectionTimeout;
	connectionTimeout.start();
	mConnectionAttempts = 0;
	mConnectionTimerId = startTimer(1000);
	attemptConnection();
}
//----------------------------------------------------
void ScreenForm::attemptConnection()
{
	mIsConnecting = true;
	if (mTcpSocket.state() != QAbstractSocket::UnconnectedState)
	{
		mTcpSocket.disconnectFromHost();
		mTcpSocket.waitForDisconnected(1000);
	}

	mConnectionAttempts++;
	mTcpSocket.connectToHost(QHostAddress(mHost), 9876);
	ui->lblFps->setText("Connecting to '" + mHost + "'... (Attempt " + QString::number(mConnectionAttempts) + "/3)");
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
	if (!isVisible() || !ui || mStopped)
		return;
	
	int currentSocket = 0;
	const int headerSize = 6;

	while (mTcpSocket.bytesAvailable() > 0)
	{
		if (!isVisible() || !ui || mStopped)
			return;

#ifdef PROFILING
		int timeSinceLastFrame = mFrameTimer.elapsed();
		qDebug() << "Got datagram after " << timeSinceLastFrame << " ms";
		mFrameTimer.restart();
#endif

		//qDebug() << "Has packet of " << mTcpSocket.bytesAvailable() << " bytes";

		// Read the first pending packet
		mGlobalBytesBuffer.append(mTcpSocket.readAll());

		int remainSize = mGlobalBytesBuffer.size();
		while (mGlobalBytesBuffer.size() > 10)
		{
			// Read header
			quint8 headerSize, protVersion;
			quint32 frameSize, audioFrameSize = 0;

			protVersion = bytesToUInt8(mGlobalBytesBuffer, 0);

			if (protVersion == 3) // BBQScreen 2.1.2 - Legacy method, no audio
				headerSize = 6;
			else if (protVersion == 4) // BBQScreen 2.2.0 - With audio
				headerSize = 10;
			else
				qWarning() << "WARN: Unknown protVersion " << protVersion;

			mRemoteOrientation = (int) bytesToUInt8(mGlobalBytesBuffer, 1);
			frameSize = bytesToUInt32(mGlobalBytesBuffer, 2);

			if (protVersion == 4)
			{
				audioFrameSize = bytesToUInt32(mGlobalBytesBuffer, 6);
				//qDebug() << "Protocol V4, video size=" << frameSize << " audio size=" << audioFrameSize;
			}

			mVideoFrameSize = frameSize;
			mAudioFrameSize = audioFrameSize;
			if (frameSize+audioFrameSize+headerSize > mGlobalBytesBuffer.size())
			{
				//qDebug() << "-- Will revisit later (" << (mGlobalBytesBuffer.size()-headerSize) << "/" << (frameSize+audioFrameSize) << ")";
				break;
			}

			mGlobalBytesBuffer = mGlobalBytesBuffer.remove(0, headerSize);

			// Decode the video frame (if any)
			if (frameSize > 0)
			{
				unsigned char* buff = new unsigned char[frameSize];
				memcpy(buff, mGlobalBytesBuffer.data(), frameSize);
				mDecoder.decodeFrame(buff, frameSize, mLastImageDisplayed);
				mDecoder.process();
				//mVideoDecoderThread.start();
				mGlobalBytesBuffer = mGlobalBytesBuffer.remove(0, frameSize);
			}
			
			// If protocol version 4, device the audio frame (if any)
			if (audioFrameSize > 0)
			{
				unsigned char* buff = new unsigned char[audioFrameSize];
				memcpy(buff, mGlobalBytesBuffer.data(), audioFrameSize);
				mAudioDecoder.decodeFrame(buff, audioFrameSize);
				mAudioDecoder.process();
				//mAudioDecoderThread.start();
				mGlobalBytesBuffer = mGlobalBytesBuffer.remove(0, audioFrameSize);
			}
		}
	}
}
//----------------------------------------------------
void ScreenForm::onDecodeFinished(bool result, bool isAudio)
{
	if (!isAudio && result)
	{
		if (mLastImageDisplayed)
		{
			QImage img = mDecoder.getLastFrame();
			mRotationAngle = mRemoteOrientation * (-90) + mOrientationOffset;

			mOriginalSize.setX(img.width());
			mOriginalSize.setY(img.height());

			if (mRotationAngle != 0)
			{
				QTransform t;
				t.rotate(mRotationAngle);
				img = img.transformed(t, mHighQuality ? Qt::SmoothTransformation : Qt::FastTransformation);
			}

			mLastImage = img;
			mLastImageDisplayed = false;
			ui->lblDisplay->setImage(mLastImage);

			mTotalFrameReceived++;
			
#ifdef PROFILING
			timeSinceLastFrame = mFrameTimer.elapsed();
			qDebug() << "Decoded in " << timeSinceLastFrame << " ms";
#endif
			if (!isVisible() || !ui || mStopped)
				return;

			if (mShowFps)
				ui->lblFps->setText(QString::number((double)(mTotalFrameReceived/(mFrameTimer.elapsed()/1000.0))) + " fps");
			else
				ui->lblFps->setText("");
		}
	}

	if (isAudio)
		mAudioDecoderThread.quit();
	else
		mVideoDecoderThread.quit();
}
//----------------------------------------------------
void ScreenForm::onSocketStateChanged()
{
	if (mStopped || !ui)
	{
		// Don't do anything if we stopped or closed the window
		return;
	}

	if (mTcpSocket.state() == QAbstractSocket::ConnectedState)
	{
		ui->lblFps->setText("Connected, loading first frame...");
		mIsConnecting = false;
		qApp->processEvents();
	}
	else if (mTcpSocket.state() == QAbstractSocket::UnconnectedState)
	{
		ui->lblFps->setText("Lost connection with host device. Reconnecting...");
		ui->lblFps->setVisible(true);
		mIsConnecting = true;

		connectTo(mHost);
	}
}
//----------------------------------------------------
void ScreenForm::timerEvent(QTimerEvent *evt)
{
	if (mStopped)
		return;

	if (evt->timerId() == mConnectionTimerId)
	{
		if (mTcpSocket.state() != QAbstractSocket::ConnectedState
			&& mTcpSocket.state() != QAbstractSocket::ConnectingState)
		{
			if (mConnectionAttempts < 3)
			{
				// Unable to connect, try again
				attemptConnection();
			}
			else
			{
				// Tried too much times, abort
				QMessageBox::critical(this, "Could not connect", "Unable to connect to " + mHost + " after 3 attempts. Please check the device IP, and make sure your screen is unlocked.\nError message: " + mTcpSocket.errorString());
				killTimer(mConnectionTimerId);
				mConnectionTimerId = -1;
				close();
				return;
			}
		}
		else if (mTcpSocket.state() == QAbstractSocket::ConnectedState)
		{
			// Connected
			mIsConnecting = false;
			mFrameTimer.start();

			if (!mShowFps)
			{
				ui->lblFps->setText("");
				ui->lblFps->setVisible(false);
			}

			mTimeSinceLastTouchEvent.restart();
			killTimer(mConnectionTimerId);
			mConnectionTimerId = -1;
		}
	}
	else
	{
		// Frame updater timer
		if (!mLastImageDisplayed)
		{
			// Display next frame
			ui->lblDisplay->setImage(mLastImage);
			mLastImageDisplayed = true;
		}

		if (mTimeSinceLastTouchEvent.elapsed() > 16 && mTouchEventPacket.size() > 0)
		{
			mTcpSocket.write(mTouchEventPacket);
			mTcpSocket.flush();
			mTouchEventPacket.clear();

			mTimeSinceLastTouchEvent.restart();
		}
	}
}
//----------------------------------------------------
#if defined(PLAT_APPLE)
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
#if defined(PLAT_WINDOWS) || defined(PLAT_LINUX)
			sendKeyboardInput(false, evt->nativeScanCode());
#elif defined(PLAT_APPLE)
			sendKeyboardInput(false, evt->key());
#else
#error "Unsupported keyboard platform"
#endif
		}
		break;
	}

	evt->accept();
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
#if defined(PLAT_WINDOWS) || defined(PLAT_LINUX)
			sendKeyboardInput(true, evt->nativeScanCode());
#elif defined(PLAT_APPLE)
			char inputChar = evt->text().at(0).toLatin1();
			qDebug() << "Input char is " << inputChar;
			sendKeyboardInput(true, evt->key());
#endif
		}
		break;
	}

	// Handle Ctrl-... shortcuts
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

	evt->accept();
}
//----------------------------------------------------
void ScreenForm::mousePressEvent(QMouseEvent *evt)
{
	if (evt->button() == Qt::LeftButton || evt->button() == Qt::RightButton)
	{
		mIsMouseDown = true;
		float posX = evt->x();
		float posY = evt->y();
		QSizeF imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - ((width() - imgSz.width()) / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());
		
		QPoint pos = getScreenSpacePoint(posX, posY);

		if (pos.x() < 0) pos.setX(0);
		if (pos.y() < 0) pos.setY(0);

		if (evt->button() == Qt::LeftButton) {
			sendTouchInput(TET_DOWN, 0, pos.x(), pos.y());
		} else {
			sendTouchInput(TET_DOWN, 1, pos.x() + 30, pos.y() + 30);
		}

		evt->accept();
	}
}
//----------------------------------------------------
void ScreenForm::mouseReleaseEvent(QMouseEvent *evt)
{
	if (evt->button() == Qt::LeftButton || evt->button() == Qt::RightButton)
	{
		mIsMouseDown = false;
		float posX = evt->x();
		float posY = evt->y();
		QSizeF imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - (width() / 2.0f - imgSz.width() / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());

		QPoint pos = getScreenSpacePoint(posX, posY);

		if (evt->button() == Qt::LeftButton) {
			sendTouchInput(TET_UP, 0, pos.x(), pos.y());
		} else {
			sendTouchInput(TET_UP, 1, pos.x(), pos.y());
		}

		evt->accept();
	}
}
//----------------------------------------------------
void ScreenForm::mouseMoveEvent(QMouseEvent *evt)
{
	if (mIsMouseDown)
	{
		float posX = evt->x();
		float posY = evt->y();
		QSizeF imgSz = ui->lblDisplay->getRenderSize();

		posX = posX - (width() / 2.0f - imgSz.width() / 2.0f);
		posX = posX * ((float) width() / (float) imgSz.width());

		QPoint pos = getScreenSpacePoint(posX, posY);
		sendTouchInput(TET_MOVE, 0, pos.x(), pos.y());

		evt->accept();
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
	mStopped = true;
}
//----------------------------------------------------
void ScreenForm::sendKeyboardInput(bool down, unsigned int keyCode)
{
	if (mTcpSocket.state() != QAbstractSocket::ConnectedState) return;

	QByteArray packet;
	packet.append((char)IET_KEYBOARD);
	packet.append(down ? '\x01' : '\x00');
	packet.append(numberToBytes(keyCode, 4));

	qDebug() << "Keyboard pressed: " << keyCode;

	mTcpSocket.write(packet);
	mTcpSocket.flush();
	qApp->processEvents();
}
//----------------------------------------------------
void ScreenForm::sendTouchInput(TouchEventType type, unsigned char finger, unsigned short x, unsigned short y)
{
	if (mTcpSocket.state() != QAbstractSocket::ConnectedState) return;

	QByteArray packet;
	packet.append((char)IET_TOUCH);
	packet.append((char)type);
	packet.append(finger);
	packet.append(numberToBytes(x, 2));
	packet.append(numberToBytes(y, 2));

	mTouchEventPacket = packet;
	// Will be sent in timerEvent
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
