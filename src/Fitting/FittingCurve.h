/******************************************************************************************************
 * (C) 2016 markummitchell@github.com. This file is part of Engauge Digitizer, which is released      *
 * under GNU General Public License version 2 (GPLv2) or (at your option) any later version. See file *
 * LICENSE or go to gnu.org/licenses for details. Distribution requires prior written permission.     *
 ******************************************************************************************************/

#ifndef FITTING_CURVE_H
#define FITTING_CURVE_H

#include "GraphicsLinesForCurve.h"

/// Repurpose the GraphicsLinesForCurve class to show the curve fit line
class FittingCurve : public GraphicsLinesForCurve
{
public:
  /// Single constructor
  FittingCurve ();
  virtual ~FittingCurve ();

private:

};

#endif // FITTING_CURVE_H
