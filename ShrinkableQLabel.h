#ifndef SHRINKABLEQLABEL_H
#define SHRINKABLEQLABEL_H

#include <QPixmap>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QLabel>

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

#endif