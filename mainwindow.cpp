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