///////////////////////////////////////////////////////////////////////////////
// This file is part of the "NcML Module" project, a BES module designed
// to allow NcML files to be used to be used as a wrapper to add
// AIS to existing datasets of any format.
//
// Copyright (c) 2009 OPeNDAP, Inc.
// Author: Michael Johnson  <m.johnson@opendap.org>
//
// For more information, please also see the main website: http://opendap.org/
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////
#include "RCObject.h"
#include "BESDebug.h"
#include <sstream>

namespace ncml_module
{
  RCObject::RCObject()
  : _count(0)
  {
  }

 RCObject::~RCObject()
 {
   // just to let us know its invalid
   _count = -1;
 }

 int
 RCObject::ref()
 {
   return ++_count;
 }

 int
 RCObject::unref() throw()
 {
   int tmp = --_count;
   if (_count == 0)
     {
       BESDEBUG("ncml", "Object being deleted since its count hit 0.  " << printRCObject() <<
           " with toString() == " << toString() << endl);
       delete this;
     }
   return tmp;
 }

 int
 RCObject::getRefCount() const
 {
   return _count;
 }

 string
 RCObject::toString() const
 {
   return printRCObject();
 }

 string
 RCObject::printRCObject() const
 {
   std::ostringstream oss;
   oss << "RCObject@(" << reinterpret_cast<const void*>(this) << ") _count=" << _count;
   return oss.str();
 }

} // namespace ncml_module
