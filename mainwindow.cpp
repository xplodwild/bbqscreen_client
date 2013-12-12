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

#if defined(PLAT_WINDOWS)
#define ADB_PATH "prebuilts/adb.exe"
#elif defined(PLAT_LINUX) || defined(PLAT_APPLE)
#define ADB_PATH "prebuilts/adb"
#endif

//----------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MainWindow), mADBProcess(NULL)
{
	ui->setupUi(this);

	// Setup UDP discovery socket
	mAnnouncer = new QUdpSocket(this);
	mAnnouncer->bind(QHostAddress::Any, 9876);
	connect(mAnnouncer, SIGNAL(readyRead()), this, SLOT(onDiscoveryReadyRead()));

	// Connect UI slots
	connect(ui->listDevices, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onSelectDevice(QListWidgetItem*)));
	connect(ui->btnBootstrapUSB, SIGNAL(clicked()), this, SLOT(onClickBootstrapUSB()));
	connect(ui->btnConnect, SIGNAL(clicked()), this, SLOT(onClickConnect()));
	connect(ui->btnWebsite, SIGNAL(clicked()), this, SLOT(onClickWebsite()));
}
//----------------------------------------------------
MainWindow::~MainWindow()
{
	if (mADBProcess)
	{
		mADBProcess->kill();
		delete mADBProcess;
	}
	delete ui;
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
		ui->ebIP->setText(mDevices.at(index).second);
	}
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
			if ((*it).first == deviceName && (*it).second == remoteIp)
			{
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
			ui->listDevices->addItem(QString("%1 - (%2)").arg(deviceName, remoteIp));
			mDevices.push_back(QPair<QString, QString>(deviceName, remoteIp));
		}
	}
}
//----------------------------------------------------
void MainWindow::onClickWebsite()
{
	QDesktopServices::openUrl(QUrl("http://screen.bbqdroid.org/"));
}
//----------------------------------------------------
void MainWindow::onClickBootstrapUSB()
{
	if (!mADBProcess)
	{
		mADBProcess = new QProcess(this);
		connect(mADBProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onADBProcessFinishes()));
	}
	
	QStringList args;
	args << "shell";
	args << "/data/data/org.bbqdroid.bbqscreen/files/bbqscreen";
	args << "-s 50";
	args << "-720";

	mADBProcess->start("prebuilts/adb.exe", args);
}
//----------------------------------------------------
void MainWindow::onADBProcessFinishes()
{
	// If the process crashed, reboot it
	onClickBootstrapUSB();
}
//----------------------------------------------------