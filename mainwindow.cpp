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

//----------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	//setWindowFlags( Qt::CustomizeWindowHint );
	this->setFixedSize(this->width(),this->height());


	mAnnouncer = new QUdpSocket(this);
	mAnnouncer->bind(QHostAddress::Any, 9876);
	connect(mAnnouncer, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

	connect(ui->listDevices, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onSelectDevice(QListWidgetItem*)));

	connect(ui->btnConnect, SIGNAL(clicked()), this, SLOT(onClickConnect()));
	connect(ui->btnWebsite, SIGNAL(clicked()), this, SLOT(onClickWebsite()));
}
//----------------------------------------------------
MainWindow::~MainWindow()
{
	delete ui;
}
//----------------------------------------------------
void MainWindow::onClickConnect()
{
	ScreenForm* screen = new ScreenForm(this);
	screen->setQuality(ui->cbHighQuality->isChecked());
	screen->setShowFps(ui->cbShowFps->isChecked());
	screen->show();
	screen->connectTo(ui->ebIP->text());
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
void MainWindow::onReadyRead()
{
	while (mAnnouncer->hasPendingDatagrams())
	{
		QByteArray datagram;
		datagram.resize(mAnnouncer->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		mAnnouncer->readDatagram(datagram.data(), datagram.size(),
			&sender, &senderPort);

		// Format of announcer packet:
		// 0 : Protocol version
		// 1 : Device name size
		// 2+: Device name

		unsigned char protocolVersion = datagram.at(0),
			deviceNameSize = datagram.at(1);
		QString deviceName = QString(datagram.data()+2);

		bool exists = false;
		for (auto it = mDevices.begin(); it != mDevices.end(); ++it)
		{
			if ((*it).first == deviceName && (*it).second == sender.toString())
			{
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			ui->listDevices->addItem(deviceName + " - (" + sender.toString() + ")");
			mDevices.push_back(QPair<QString, QString>(deviceName, sender.toString()));
		}
	}
}
//----------------------------------------------------
void MainWindow::onClickWebsite()
{
	QDesktopServices::openUrl(QUrl("http://screen.bbqdroid.org/"));
}
//----------------------------------------------------
void MainWindow::notifyScreenClose(ScreenForm* form)
{
	mFormToDelete = form;
	mKillTimerId = startTimer(700);
}
//----------------------------------------------------
void MainWindow::timerEvent(QTimerEvent* evt)
{
	killTimer(mKillTimerId);
	delete mFormToDelete;
	mFormToDelete = nullptr;

	show();
}
//----------------------------------------------------