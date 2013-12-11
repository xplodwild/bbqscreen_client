#ifndef SHRINKABLEQLABEL_H
#define SHRINKABLEQLABEL_H

#include <QPixmap>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QGraphicsView>

class ShrinkableQLabel : public QGraphicsView
{
	Q_OBJECT;

public:
	ShrinkableQLabel(QWidget* parent = 0);
	void paintEvent(QPaintEvent *aEvent);
	void setImage(const QImage& aPicture);
	void setHighQuality(bool high);
	QSizeF getRenderSize();

	void mousePressEvent(QMouseEvent *event) { event->ignore(); }
	void mouseReleaseEvent(QMouseEvent *event) { event->ignore(); }
	void mouseMoveEvent(QMouseEvent *event) { event->ignore(); }

protected:
	void _displayImage();
	QGraphicsScene* mScene;
	QGraphicsPixmapItem* mPixmapItem;

	bool mLastImagePainted;
	QImage mSource;
	bool mHighQuality;
};

#endif