#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>


namespace Ui {
	class MainWindow;
}

class ScreenForm;

class MainWindow : public QDialog
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();
	void notifyScreenClose(ScreenForm* form);

	void timerEvent(QTimerEvent* evt);

	private slots:
		void onClickConnect();
		void onClickWebsite();

private:
	Ui::MainWindow *ui;
	ScreenForm* mFormToDelete;
	int mKillTimerId;

};

#endif // MAINWINDOW_H
