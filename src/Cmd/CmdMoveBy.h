/******************************************************************************************************
 * (C) 2014 markummitchell@github.com. This file is part of Engauge Digitizer, which is released      *
 * under GNU General Public License version 2 (GPLv2) or (at your option) any later version. See file *
 * LICENSE or go to gnu.org/licenses for details. Distribution requires prior written permission.     *
 ******************************************************************************************************/

#ifndef CMD_MOVE_BY_H
#define CMD_MOVE_BY_H

#include "CmdPointChangeBase.h"
#include "PointIdentifiers.h"
#include <QPointF>
#include <QStringList>

class QXmlStreamReader;

/// Command for moving all selected Points by a specified translation.
class CmdMoveBy : public CmdPointChangeBase
{
public:
  /// Constructor for normal creation
  CmdMoveBy(MainWindow &mainWindow,
            Document &document,
            const QPointF &deltaScreen,
            const QString &moveText,
            const QStringList &selectedPointIdentifiers);

  /// Constructor for parsing error report file xml
  CmdMoveBy(MainWindow &mainWindow,
            Document &document,
            const QString &cmdDescription,
            QXmlStreamReader &reader);

  virtual ~CmdMoveBy();

  virtual void cmdRedo ();
  virtual void cmdUndo ();
  virtual void saveXml (QXmlStreamWriter &writer) const;

private:
  CmdMoveBy();

  void moveBy (const QPointF &deltaScreen);

  QPointF m_deltaScreen;
  PointIdentifiers m_movedPoints;
};

#endif // CMD_MOVE_BY_H
