#include "Checker.h"
#include "EngaugeAssert.h"
#include "EnumsToQt.h"
#include "Logger.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <qmath.h>
#include <QPen>
#include <QTextStream>
#include "Transformation.h"

const QString DUMMY_CURVENAME ("dummy");
const int Z_VALUE_IN_FRONT = 100;
const int NO_SIDE = -1;

// To emphasize that the axis lines are still there, we make these checker somewhat transparent
const double CHECKER_OPACITY = 0.6;

// One-pixel wide line (produced by setting width=0) is too small. 5 is big enough to be always noticeable,
// but such a thick line obscures the axes points. To keep the axes points visible, we remove portions of 
// the line nearer to an axes point than the point radius.
const int CHECKER_POINTS_WIDTH = 5;

Checker::Checker(QGraphicsScene &scene) :
  m_scene (scene)
{
  for (int i = 0; i < MAX_LINES_PER_SIDE; i++) {
    m_sideLeft [i] = 0;
    m_sideTop [i] = 0;
    m_sideRight [i] = 0;
    m_sideBottom [i] = 0;
  }
}

void Checker::bindItemToScene(QGraphicsItem *item)
{
  LOG4CPP_DEBUG_S ((*mainCat)) << "Checker:bindItemToScene";

  item->setOpacity (CHECKER_OPACITY);
  item->setZValue (Z_VALUE_IN_FRONT);
  item->setToolTip (QObject::tr ("Axes checker. If this does not align with the axes, then the axes points should be checked"));

  m_scene.addItem (item);
}

void Checker::createSide (int pointRadius,
                          const QList<Point> &points,
                          const DocumentModelCoords &modelCoords,
                          const QPointF &pointFromGraph,
                          const QPointF &pointToGraph,
                          const Transformation &transformation,
                          QGraphicsItem *items [MAX_LINES_PER_SIDE])
{
  QPointF pointFromGraphCart = transformation.cartesianFromCartesianOrPolar (modelCoords,
                                                                             pointFromGraph);
  QPointF pointToGraphCart = transformation.cartesianFromCartesianOrPolar (modelCoords,
                                                                           pointToGraph);

  // Convert graph coordinates to screen coordinates
  QPointF pointFromScreen, pointToScreen;
  transformation.transformInverse (pointFromGraphCart,
                                   pointFromScreen);
  transformation.transformInverse (pointToGraphCart,
                                   pointToScreen);

  // Build a list of points where the circle around each point intercepts the infinite line through
  // pointFromScreen and pointToScreen
  QList<double> sInterceptPoints;
  sInterceptPoints << 0; // Start at limit
  sInterceptPoints << 1; // Stop at limit
  QList<Point>::const_iterator itr;
  for (itr = points.begin (); itr != points.end (); itr++) {
    const Point &point = *itr;
    interceptPointCircleWithLine (pointRadius,
                                  sInterceptPoints,
                                  point,
                                  pointFromScreen,
                                  pointToScreen);
  }

  qSort (sInterceptPoints);

  // Loop through sorted s values, ignoring those outside the range 0 to 1. Draw line for (s(i-1),s(i))
  // if the midpoint is not near any point
  int itemCount = 0;
  double sLast = 0;
  for (int i = 0; i < sInterceptPoints.count(); i++) {
    double s = sInterceptPoints.at (i);
    if (i > 0) {
      if (0 < s && sLast < 1) {

        double sMidpoint = (s + sLast) / 2.0;

        QPointF posStart    = (1.0 - sLast    ) * pointFromScreen + sLast     * pointToScreen;
        QPointF posMidpoint = (1.0 - sMidpoint) * pointFromScreen + sMidpoint * pointToScreen;
        QPointF posEnd      = (1.0 - s        ) * pointFromScreen + s         * pointToScreen;

        if (minScreenDistanceFromPoints (posMidpoint, points) > pointRadius) {
          QGraphicsItem *item = new QGraphicsLineItem (QLineF (posStart,
                                                               posEnd));
          items [itemCount++] = item;
          bindItemToScene (item);
        }
      }
    }
    sLast = s;
  }
}

void Checker::deleteSide (QGraphicsItem *items [MAX_LINES_PER_SIDE])
{
  for (int i = 0; i < MAX_LINES_PER_SIDE; i++) {
    QGraphicsItem *item = items [i];
    if (item != 0) {
      delete item;
    }

    items [i] = 0;
  }
}

void Checker::interceptPointCircleWithLine (int pointRadius,
                                            QList<double> &sInterceptPoints,
                                            const Point &point,
                                            const QPointF &pointFromScreen,
                                            const QPointF &pointToScreen)
{
  double distanceFromTo = qSqrt ((pointToScreen.x() - pointFromScreen.x()) * (pointToScreen.x() - pointFromScreen.x()) +
                                 (pointToScreen.y() - pointFromScreen.y()) * (pointToScreen.y() - pointFromScreen.y()));

  // Compensate for slop in drawing of lines by making radius a tiny bit bigger
  double radiusTweaked = pointRadius + 1;

  // Intersect:
  // 1) (y-y0)/(y1-y0) = (x-x0)/(x1-x0), but converted from two point form to slope intercept form for convenience
  // 2) (x-xc)^2+(y-yc)^2=r^2

  double dx = pointToScreen.x() - pointFromScreen.x();
  double dy = pointToScreen.y() - pointFromScreen.y();

  // Handle more-horizontal and more-vertical lines separately to prevent divide by zero issues
  if (dx < dy) {

    // x = slope * y + intercept
    double slope = dx / dy;
    double intercept = pointToScreen.x() - slope * pointToScreen.y();

    // Perpendicular line that goes through specified point
    double slopePerp = -1.0 / slope;
    double interceptPerp = point.posScreen().x() - slopePerp * point.posScreen().y();

    // Intersection point of both lines comes from subtracting both x=slope*y+intercept and x=slopePerp*y+interceptPerp
    double yIntercept = (interceptPerp - intercept) / (slope - slopePerp);
    double xIntercept = slope * yIntercept + intercept;

    // Distance from point to line
    double separation = qSqrt ((xIntercept - point.posScreen().x()) * (xIntercept - point.posScreen().x()) +
                               (yIntercept - point.posScreen().y()) * (yIntercept - point.posScreen().y()));
    if (separation < radiusTweaked) {

      // s at intercept (=distanceFromIntercept/distanceFromTo) is not needed, but distance is needed
      double distanceFromIntercept = qSqrt ((xIntercept - pointFromScreen.x()) * (xIntercept - pointFromScreen.x()) +
                                            (yIntercept - pointFromScreen.y()) * (yIntercept - pointFromScreen.y()));

      // Find both intersection points at +/-offsetFromIntercept
      double offsetFromIntercept = qSqrt (radiusTweaked * radiusTweaked - separation * separation);
      double sMinus = (distanceFromIntercept - offsetFromIntercept) / distanceFromTo;
      double sPlus  = (distanceFromIntercept + offsetFromIntercept) / distanceFromTo;

      sInterceptPoints.push_back (sMinus);
      sInterceptPoints.push_back (sPlus);
    }

  } else {

    // y = slope * y + intercept
    double slope = dy / dx;
    double intercept = pointToScreen.y() - slope * pointToScreen.x();

    // Perpendicular line that goes through specified point
    double slopePerp = -1.0 / slope;
    double interceptPerp = point.posScreen().y() - slopePerp * point.posScreen().x();

    // Intersection point of both lines comes from subtracting both y=slope*x+intercept and y=slopePerp*x+interceptPerp
    double xIntercept = (interceptPerp - intercept) / (slope - slopePerp);
    double yIntercept = slope * xIntercept + intercept;

    // Distance from point to line
    double separation = qSqrt ((xIntercept - point.posScreen().x()) * (xIntercept - point.posScreen().x()) +
                               (yIntercept - point.posScreen().y()) * (yIntercept - point.posScreen().y()));
    if (separation < radiusTweaked) {

      // s at intercept (=distanceFromIntercept/distanceFromTo) is not needed, but distance is needed
      double distanceFromIntercept = qSqrt ((xIntercept - pointFromScreen.x()) * (xIntercept - pointFromScreen.x()) +
                                            (yIntercept - pointFromScreen.y()) * (yIntercept - pointFromScreen.y()));

      // Find both intersection points at +/-offsetFromIntercept
      double offsetFromIntercept = qSqrt (radiusTweaked * radiusTweaked - separation * separation);
      double sMinus = (distanceFromIntercept - offsetFromIntercept) / distanceFromTo;
      double sPlus  = (distanceFromIntercept + offsetFromIntercept) / distanceFromTo;

      sInterceptPoints.push_back (sMinus);
      sInterceptPoints.push_back (sPlus);
    }
  }
}

double Checker::minScreenDistanceFromPoints (const QPointF &posScreen,
                                             const QList<Point> &points)
{
  double minDistance = 0;
  for (int i = 0; i < points.count (); i++) {
    const Point &pointCenter = points.at (i);

    double dx = posScreen.x() - pointCenter.posScreen().x();
    double dy = posScreen.y() - pointCenter.posScreen().y();

    double distance = qSqrt (dx * dx + dy * dy);
    if (i == 0 || distance < minDistance) {
      minDistance = distance;
    }
  }

  return minDistance;
}

void Checker::prepareForDisplay (const QPolygonF &polygon,
                                 int pointRadius,
                                 const DocumentModelAxesChecker &modelAxesChecker,
                                 const DocumentModelCoords &modelCoords)
{
  LOG4CPP_INFO_S ((*mainCat)) << "Checker::prepareForDisplay";

  ENGAUGE_ASSERT (polygon.count () == NUM_AXES_POINTS);

  // Convert pixel coordinates in QPointF to screen and graph coordinates in Point using
  // identity transformation, so this routine can call the general case routine
  QList<Point> points;
  QPolygonF::const_iterator itr;
  for (itr = polygon.begin (); itr != polygon.end (); itr++) {

    QPointF pF = *itr;
    Point p (DUMMY_CURVENAME,
             pF,
             pF);
    points.push_back (p);
  }

  // Screen and graph coordinates are treated as the same, so identity transform is used
  Transformation transformIdentity;
  transformIdentity.identity();
  prepareForDisplay (points,
                     pointRadius,
                     modelAxesChecker,
                     modelCoords,
                     transformIdentity);
}

void Checker::prepareForDisplay (const QList<Point> &points,
                                 int pointRadius,
                                 const DocumentModelAxesChecker &modelAxesChecker,
                                 const DocumentModelCoords &modelCoords,
                                 const Transformation &transformation)
{
  LOG4CPP_INFO_S ((*mainCat)) << "Checker::prepareForDisplay";

  ENGAUGE_ASSERT (points.count () == NUM_AXES_POINTS);

  // Remove previous lines
  deleteSide (m_sideLeft);
  deleteSide (m_sideTop);
  deleteSide (m_sideRight);
  deleteSide (m_sideBottom);

  // Get the min and max of x and y
  double xMin, xMax, yMin, yMax;
  int i;
  for (i = 0; i < 3; i++) {
    if (i == 0) {
      xMin = points.at(i).posGraph().x();
      xMax = points.at(i).posGraph().x();
      yMin = points.at(i).posGraph().y();
      yMax = points.at(i).posGraph().y();
    }
    xMin = qMin (xMin, points.at(i).posGraph().x());
    xMax = qMax (xMax, points.at(i).posGraph().x());
    yMin = qMin (yMin, points.at(i).posGraph().y());
    yMax = qMax (yMax, points.at(i).posGraph().y());
  }

  // Draw the bounding box as four sides
  createSide (pointRadius, points, modelCoords, QPointF (xMin, yMin), QPointF (xMin, yMax), transformation, m_sideLeft);
  createSide (pointRadius, points, modelCoords, QPointF (xMin, yMax), QPointF (xMax, yMax), transformation, m_sideTop);
  createSide (pointRadius, points, modelCoords, QPointF (xMax, yMax), QPointF (xMax, yMin), transformation, m_sideRight);
  createSide (pointRadius, points, modelCoords, QPointF (xMax, yMin), QPointF (xMin, yMin), transformation, m_sideBottom);

  updateModelAxesChecker (modelAxesChecker);
}

void Checker::setLineColor (QGraphicsItem *items [MAX_LINES_PER_SIDE], const QPen &pen)
{
  for (int i = 0; i < MAX_LINES_PER_SIDE; i++) {
    QGraphicsItem *item = items [i];
    if (item != 0) {

      // Downcast since QGraphicsItem does not have a pen
      QGraphicsLineItem *itemLine = dynamic_cast<QGraphicsLineItem*> (item);
      QGraphicsEllipseItem *itemEllipse = dynamic_cast<QGraphicsEllipseItem*> (item);
      if (itemLine == 0) {
        itemEllipse->setPen (pen);
      } else {
        itemLine->setPen (pen);
      }
    }
  }
}

void Checker::setVisible (bool visible)
{
  setVisibleSide (m_sideLeft, visible);
  setVisibleSide (m_sideTop, visible);
  setVisibleSide (m_sideRight, visible);
  setVisibleSide (m_sideBottom, visible);
}

void Checker::setVisibleSide (QGraphicsItem *items [MAX_LINES_PER_SIDE],
                              bool visible)
{
  for (int i = 0; i < MAX_LINES_PER_SIDE; i++) {
    QGraphicsItem *item = items [i];
    if (item != 0) {
      item->setVisible (visible);
    }
  }
}

void Checker::updateModelAxesChecker (const DocumentModelAxesChecker &modelAxesChecker)
{
  QColor color = ColorPaletteToQColor (modelAxesChecker.lineColor());
  QPen pen (QBrush (color), CHECKER_POINTS_WIDTH);

  setLineColor (m_sideLeft, pen);
  setLineColor (m_sideTop, pen);
  setLineColor (m_sideRight, pen);
  setLineColor (m_sideBottom, pen);
}