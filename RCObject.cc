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
#include "NCMLDebug.h"
#include <sstream>
#include <vector>

namespace agg_util
{
  RCObject::RCObject(RCObjectPool* pool/*=0*/)
  : RCObjectInterface()
  , _count(0)
  , _pool(pool)
  {
    if (_pool)
      {
        _pool->add(this);
      }
  }

  RCObject::RCObject(const RCObject& proto)
  : RCObjectInterface()
  , _count(0) // new objects have no count, forget what the proto has!
  , _pool(proto._pool)
  {
    if (_pool)
      {
        _pool->add(this);
      }
  }

 RCObject::~RCObject()
 {
   // just to let us know its invalid
   _count = -1;
 }

 int
 RCObject::ref() const
 {
   return ++_count;
 }

 int
 RCObject::unref() const throw()
 {
   int tmp = --_count;
   if (_count == 0)
     {
       if (_pool)
         {
           BESDEBUG("ncml:memory", "Releasing back to pool: Object ref count hit 0.  " << printRCObject() <<
             " with toString() == " << toString() << endl);
           _pool->release(const_cast<RCObject*>(this));
         }
       else
         {
           BESDEBUG("ncml:memory", "Calling delete: Object ref count hit 0.  " << printRCObject() <<
             " with toString() == " << toString() << endl);
           delete this;
         }
     }
   return tmp;
 }

 int
 RCObject::getRefCount() const
 {
   return _count;
 }

 void
 RCObject::removeFromPool() const
 {
   if (_pool)
     {
       // remove will not delete it
       // and will clear _pool
       _pool->remove(const_cast<RCObject*>(this));
       NCML_ASSERT(!_pool);
     }
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


 /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //////////////////////////// RCObjectPool

 RCObjectPool::RCObjectPool()
 : _liveObjects()
 {
 }

 RCObjectPool::~RCObjectPool()
 {
   deleteAllObjects();
 }

 bool
 RCObjectPool::contains(RCObject* pObj) const
 {
   RCObjectSet::const_iterator foundIt = _liveObjects.find(pObj);
   return (foundIt != _liveObjects.end());
 }

 void
 RCObjectPool::add(RCObject* pObj)
 {
   if (contains(pObj))
     {
       throw string("Internal Pool Error: Object added twice!");
     }
   _liveObjects.insert(pObj);
   pObj->_pool = this;
 }

 void
 RCObjectPool::release(RCObject* pObj, bool shouldDelete/*=true*/)
 {
   if (contains(pObj))
     {
       _liveObjects.erase(pObj);
       pObj->_pool = 0;

       if (shouldDelete)
         {
           // Delete it for now...  If we decide to subclass and implement a real pool,
           // we'll want to push this onto a vector for reuse.
           BESDEBUG("ncml:memory", "RCObjectPool::release(): Calling delete on released object=" <<
               pObj->printRCObject() <<
               endl);
           delete pObj;
         }
       else
         {
           BESDEBUG("ncml:memory", "RCObjectPool::release(): Removing object, but not deleting it: "
               << pObj->printRCObject()
               << endl);
         }
     }
   else
     {
       BESDEBUG("ncml:memory", "ERROR: RCObjectPool::release() called on object not in pool!!  Ignoring!" << endl);
     }
 }

 void
 RCObjectPool::deleteAllObjects()
 {
   BESDEBUG("ncml:memory", "RCObjectPool::deleteAllObjects() started...." << endl);
   RCObjectSet::iterator endIt = _liveObjects.end();
   RCObjectSet::iterator it = _liveObjects.begin();
   for (; it != endIt; ++it)
     {
       RCObject* pObj = *it;
       BESDEBUG("ncml:memory", "Calling delete on RCObject=" << pObj->printRCObject() << endl);
       delete pObj;
     }
   _liveObjects.clear();
   BESDEBUG("ncml:memory", "RCObjectPool::deleteAllObjects() complete!" << endl);
 }


} // namespace agg_util
