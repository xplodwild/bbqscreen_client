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

//----------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	//setWindowFlags( Qt::CustomizeWindowHint );
	this->setFixedSize(this->width(),this->height());

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