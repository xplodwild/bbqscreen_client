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
	~ShrinkableQLabel() {};
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

	QImage mSource;
	bool mHighQuality;
};

#endif
