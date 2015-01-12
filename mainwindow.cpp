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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "screenform.h"
#include <QDesktopServices>
#include <QUrl>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#define CLIENT_VERSION "2.3.0"

#if defined(PLAT_WINDOWS)
#define ADB_PATH "prebuilts/adb.exe"
#define UPDATE_URL "http://screen.bbqdroid.org/downloads/VERSION_WINDOWS"
#elif defined(PLAT_LINUX)
#define ADB_PATH "prebuilts/adb"
#define UPDATE_URL "http://screen.bbqdroid.org/downloads/VERSION_LINUX"
#elif defined(PLAT_APPLE)
#define ADB_PATH "prebuilts/adb"
#define UPDATE_URL "http://screen.bbqdroid.org/downloads/VERSION_OSX"
#endif


//----------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MainWindow), mADBProcess(NULL), mDebugWidget(nullptr), mServiceShouldRun(false),
	mCrashCount(0)
{
	ui->setupUi(this);

	// Setup UDP discovery socket
	mAnnouncer = new QUdpSocket(this);
	mAnnouncer->bind(QHostAddress::Any, 9876);
	connect(mAnnouncer, SIGNAL(readyRead()), this, SLOT(onDiscoveryReadyRead()));

	// Connect UI slots
	connect(ui->listDevices, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onSelectDevice(QListWidgetItem*)));
	connect(ui->listDevices, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onDoubleClickDevice(QListWidgetItem*)));
	connect(ui->btnBootstrapUSB, SIGNAL(clicked()), this, SLOT(onClickBootstrapUSB()));
	connect(ui->btnConnectUSB, SIGNAL(clicked()), this, SLOT(onClickConnectUSB()));
	connect(ui->btnConnect, SIGNAL(clicked()), this, SLOT(onClickConnect()));
	connect(ui->btnWebsite, SIGNAL(clicked()), this, SLOT(onClickWebsite()));
	connect(ui->btnDebugLog, SIGNAL(clicked()), this, SLOT(onClickShowDebugLog()));
	connect(ui->cbQuality, SIGNAL(currentIndexChanged(int)), this, SLOT(onQualityChanged(int)));
	connect(ui->spinBitrate, SIGNAL(valueChanged(int)), this, SLOT(onBitrateChanged(int)));

	// Check if we have an update available
	QNetworkAccessManager* netAM = new QNetworkAccessManager(this);
	QNetworkRequest request(QUrl(UPDATE_URL));

	QNetworkReply* reply = netAM->get(request);
	connect(reply, SIGNAL(finished()), this, SLOT(onUpdateChecked()));

	ui->lblClientVersion->setText("Client version " CLIENT_VERSION);

	// Start timeout timer
	startTimer(500);
}
//----------------------------------------------------
MainWindow::~MainWindow()
{
	delete ui;
}
//----------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* evt)
{
	Q_UNUSED(evt);
	if (mADBProcess)
	{
		disconnect(mADBProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onADBProcessFinishes()));
		mADBProcess->terminate();
		delete mADBProcess;
	}
}
//----------------------------------------------------
void MainWindow::timerEvent(QTimerEvent* evt)
{
	Q_UNUSED(evt);

	// See if we have devices that disappeared. We make them timeout after 3 seconds.
	for (auto it = mDevices.begin(); it != mDevices.end(); ++it)
	{
		if ((*it)->lastPing.elapsed() > 3000)
		{
			ui->listDevices->takeItem(mDevices.indexOf(*it));
			delete (*it);
			it = mDevices.erase(it);
			break;
		}
	}
}
//----------------------------------------------------
void MainWindow::onQualityChanged(int index)
{
	if (mADBProcess)
	{
		mCrashCount = 0;
		// Restart the app
		mADBProcess->terminate();
		mADBProcess->kill();
	}
}
//----------------------------------------------------
void MainWindow::onBitrateChanged(int value)
{
	if (mADBProcess)
	{
		mCrashCount = 0;
		// Restart the app
		mADBProcess->terminate();
		mADBProcess->kill();
	}
}
//----------------------------------------------------
void MainWindow::onClickConnect()
{
	// Check that the IP entered is valid
	QString ip = ui->ebIP->text();
	QHostAddress address(ip);
	if (QAbstractSocket::IPv4Protocol != address.protocol())
	{
		QMessageBox::critical(this, "Invalid IP", "The IP address you entered is invalid");
		return;
	}

	// The IP is valid, connect to there
	ScreenForm* screen = new ScreenForm(this);
	screen->setAttribute(Qt::WA_DeleteOnClose);
	screen->setQuality(ui->cbHighQuality->isChecked());
	screen->setShowFps(ui->cbShowFps->isChecked());
	screen->show();
	screen->connectTo(ui->ebIP->text());

	// Hide this dialog
	hide();
}
//----------------------------------------------------
void MainWindow::onSelectDevice(QListWidgetItem* item)
{
	Q_UNUSED(item);
	
	int index = ui->listDevices->currentRow();
	if (index >= 0)
	{
		ui->ebIP->setText(mDevices.at(index)->address);
	}
}
//----------------------------------------------------
void MainWindow::onDoubleClickDevice(QListWidgetItem* item)
{
	onSelectDevice(item);
	onClickConnect();
}
//----------------------------------------------------
void MainWindow::onDiscoveryReadyRead()
{
	QByteArray datagram;
	QHostAddress sender;
	quint16 senderPort;

	while (mAnnouncer->hasPendingDatagrams())
	{
		if (datagram.size() != mAnnouncer->pendingDatagramSize())	
			datagram.resize(mAnnouncer->pendingDatagramSize());
		
		// Read pending UDP datagram
		mAnnouncer->readDatagram(datagram.data(), datagram.size(),
			&sender, &senderPort);

		// Format of announcer packet:
		// 0 : Protocol version
		// 1 : Device name size
		// 2+: Device name

		unsigned char protocolVersion = datagram.at(0),
			deviceNameSize = datagram.at(1);

		QString deviceName = QByteArray(datagram.data()+2, deviceNameSize);
		QString remoteIp = sender.toString();

		// Make sure we don't already know this device
		bool exists = false;
		for (auto it = mDevices.begin(); it != mDevices.end(); ++it)
		{
			if ((*it)->name == deviceName && (*it)->address == remoteIp)
			{
				(*it)->lastPing.restart();
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			// XXX: Protocol v3 indicates that audio can't be streamed, and v4
			// indicates that we can stream audio. However, the user can choose
			// to turn off audio even on v4. Maybe in the future we could indicate
			// that.
			Device* device = new Device;
			device->name = deviceName;
			device->address = remoteIp;
			device->lastPing.start();
			
			ui->listDevices->addItem(QString("%1 - (%2)").arg(deviceName, remoteIp));
			mDevices.push_back(device);
		}
	}
}
//----------------------------------------------------
void MainWindow::onClickWebsite()
{
	QDesktopServices::openUrl(QUrl("http://screen.bbqdroid.org/"));
}
//----------------------------------------------------
void MainWindow::onClickShowDebugLog()
{
	if (mDebugWidget != nullptr) {
		delete mDebugWidget;
	}
	mDebugWidget = new QListWidget();
	mDebugWidget->addItems(mADBLog);

	for (auto it = mADBErrorLog.begin(); it != mADBErrorLog.end(); ++it) {
		QListWidgetItem* item = new QListWidgetItem(*it);
		item->setTextColor(QColor(255, 0, 0));
		mDebugWidget->addItem(item);
	}

	mDebugWidget->show();
}
//----------------------------------------------------
QProcess* MainWindow::runAdb(const QStringList& params)
{
	QProcess* process = new QProcess(this);
#ifndef PLAT_APPLE
	process->start(ADB_PATH, params);
#else
	process->start(QDir(QCoreApplication::applicationDirPath()).absolutePath() + "/" + ADB_PATH, params);
#endif	

	return process;
}
//----------------------------------------------------
void MainWindow::onClickBootstrapUSB()
{
	if (!mServiceShouldRun)
	{
		mCrashCount = 0;
		mServiceShouldRun = true;
		startUsbService();
	}
	else
	{
		mServiceShouldRun = false;
		if (mADBProcess) 
		{
			mADBProcess->terminate();
			mADBProcess->kill();
		}
	}
}
//----------------------------------------------------
void MainWindow::onClickConnectUSB()
{
	// Forward TCP port to localhost and connect to it
	QStringList args;
	args << "forward";
	args << "tcp:9876";
	args << "tcp:9876";

	runAdb(args);

	ui->ebIP->setText("127.0.0.1");
	onClickConnect();
}
//----------------------------------------------------
void MainWindow::onADBProcessFinishes()
{
	if (mServiceShouldRun)
	{
		mCrashCount++;

		if (mCrashCount > 20) 
		{
			QMessageBox::critical(this, "Crash!", "It appears that the streaming process has crashed over 20 times. Please check the debug log window and send a screenshot to the support.");
			mServiceShouldRun = false;
		}
		// If the process crashed, reboot it
		startUsbService();
	}
	else
	{
		// Normal stop
		ui->btnBootstrapUSB->setText("Start USB service");
		ui->btnConnectUSB->setEnabled(false);
	}
}
//----------------------------------------------------
void MainWindow::onADBProcessReadyRead()
{
	QByteArray stdOut = mADBProcess->readAllStandardOutput();
	QString stdOutLine = QString(stdOut).trimmed();

	if (!stdOutLine.isEmpty())
	{
		mADBLog.push_back(stdOutLine);

		if (mDebugWidget != nullptr)
		{
			mDebugWidget->addItem(stdOutLine);
		}
	}
}
//----------------------------------------------------
void MainWindow::onADBErrorReadyRead()
{
	QByteArray stdErr = mADBProcess->readAllStandardError();
	QString stdErrLine = QString(stdErr).trimmed();

	if (stdErrLine.contains("device not found"))
	{
		mServiceShouldRun = false;
		QMessageBox::critical(this, "Device not found or unplugged", "Cannot find an Android device connected via ADB. Make sure USB Debugging is enabled on your device, and that the ADB drivers are installed. Follow the guide on our website for more information.");
	}

	if (!stdErrLine.isEmpty())
	{
		mADBErrorLog.push_back(stdErrLine);

		if (mDebugWidget != nullptr)
		{
			QListWidgetItem* item = new QListWidgetItem(stdErrLine);
			item->setTextColor(QColor(255, 0, 0));
			mDebugWidget->addItem(item);
		}
	}
}
//----------------------------------------------------
void MainWindow::onUpdateChecked()
{
	QNetworkReply* reply = (QNetworkReply*) QObject::sender();
	QString version = reply->readAll().trimmed();
	if (version != CLIENT_VERSION) 
	{
		if (QMessageBox::information(this, "A new version is available",
			"A new version of the client (" + version + ") is available!\nClick OK to visit http://screen.bbqdroid.org/",
			QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok)
		{
			QDesktopServices::openUrl(QUrl("http://screen.bbqdroid.org/"));
		}
	}
}
//----------------------------------------------------
void MainWindow::startUsbService()
{
	ui->btnBootstrapUSB->setEnabled(false);
	ui->btnBootstrapUSB->setText("Starting...");
	qApp->processEvents();

	if (!mADBProcess)
	{
		mADBProcess = new QProcess(this);
		connect(mADBProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onADBProcessFinishes()));
		connect(mADBProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(onADBProcessReadyRead()));
		connect(mADBProcess, SIGNAL(readyReadStandardError()), this, SLOT(onADBErrorReadyRead()));
	}

	// Copy binary to workaround some security restrictions on Lollipop and Knox
	QStringList args;
	args << "shell";
	args << "cp";
	args << "/data/data/org.bbqdroid.bbqscreen/files/bbqscreen";
	args << "/data/local/tmp/bbqscreen";
	QProcess* copyProc = runAdb(args);
	delete copyProc;

	args.clear();
	args << "shell";
	args << "chmod";
	args << "755";
	args << "/data/local/tmp/bbqscreen";
	copyProc = runAdb(args);
	delete copyProc;

	args.clear();
	args << "shell";
	args << "/data/local/tmp/bbqscreen";
	args << "-s";
	args << "50";
	switch (ui->cbQuality->currentIndex())
	{
	case 0:
		args << "-1080";
		break;
	case 1:
		args << "-720";
		break;
	case 2:
		args << "-540";
		break;
	case 3:
		args << "-360";
		break;
	}
	args << "-q";
	args << QString::number(ui->spinBitrate->value());
	args << "-i";

#ifndef PLAT_APPLE
	mADBProcess->start(ADB_PATH, args);
#else
	mADBProcess->start(QDir(QCoreApplication::applicationDirPath()).absolutePath() + "/" + ADB_PATH, args);
#endif

	ui->btnConnectUSB->setEnabled(true);
	ui->btnBootstrapUSB->setEnabled(true);
	ui->btnBootstrapUSB->setText("Stop USB service");
}
//----------------------------------------------------
