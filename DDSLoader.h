//////////////////////////////////////////////////////////////////////////////
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
#ifndef DDSLOADER_H_
#define DDSLOADER_H_

#include <string>

class BESDataHandlerInterface;
class BESContainerStorage;
class BESDDSResponse;


/**
  @brief Helper class for temporarily hijacking an existing dhi to load a DDX response
  for one particular file.

  The dhi will hijacked at the start of the load() call and restored before the
  DDX is returned.

  For exception safety, if this object's dtor is called while the dhi is hijacked
  or the response hasn't been relinquished, it will restore the dhi and clean memory, so the
  caller can guarantee resources will be in their original (ctor time) state if this object
  is destroyed via an exception stack unwind, etc.

  TODO see if there's another way to load these files without hijacking an existing dhi or
  if we want to refer to remote BES's rather than the local.  If we do this,
  this class will become an interface class with the various concrete subclasses for
  doing local vs. remote loads, etc.

  @author mjohnson <m.johnson@opendap.org>
 */
namespace ncml_module
{

class DDSLoader
{
private:

  // The dhi to use for the loading, passed in on creation.
  // Rep Invariant: the dhi state is the same on call exits as it was on call entry.
  BESDataHandlerInterface& _dhi;

  // whether we have actually hijacked the dhi, so restore knows.
  bool _hijacked;

  // The file we are laoding if we're hijacked
  std::string _filename;

  // While we're loading, this is non-null.  We pass it to caller on successful load.
  // Otherwise, it's null.
  BESDDSResponse* _ddxResponse;

  // Remember the store so we can pull the location out in restoreDHI on exception as well.
  BESContainerStorage* _store;

  // DHI state we hijack, for putting back on exception.
  std::string _containerSymbol;
  std::string _origAction;
  std::string _origActionName;
  BESContainer* _origContainer;
  BESResponseObject* _origResponse;

private:
  DDSLoader(const DDSLoader&); // disallow
  DDSLoader& operator=(const DDSLoader&); // disallow

public:
  /**
   * Create a loader that will hijack dhi on a load call, then restore it's state.
   *
   * @param dhi DHI to hijack during load, needs to be a valid object for the scope of this object's life.
   */
  DDSLoader(BESDataHandlerInterface &dhi);

  /**
   * Restores the state of the dhi to what it was when object if it is still hijacked (i.e. exception on load())
   * Destroys the BESDDSResponse if non-null, which is also the case on failed load() call.
   */
  virtual ~DDSLoader();

  /**
   * @brief Load and return a new DDX structure for the local dataset referred to by location.
   *
   * Ownership of the response object passes to the caller.
   *
   * On exception, the dhi will be restored when this is destructed, or the user
   * call directly call cleanup() to ensure this if they catch the exception and need the
   * dhi restored immediately.
   *
   * @return a newly allocated DDS (DDX) for the location, ownership passed to caller.
   *
   * @exception if the underlying location cannot be loaded.
   */
  BESDDSResponse* load(const std::string& location);

  /**
   * Ensures that the resources and dhi are all in the state they were at construction time.
   * Probably not needed by users, but in case they want to catch an exception
   * and then retry or something
   */
  void cleanup();

private:

   /**
    *  Remember the current state of the _dhi we plan to change.
    *  @see restoreDHI()
    */
   void snapshotDHI();

   /**
    * if (_hijacked), put back all the _dhi state we hijacked. Inverse of snapshotDHI().
    * At end of call, (_hijacked == false)
    */
   void restoreDHI();

   /**
    * Add a new symbol to the BESContainerStorageList singleton.
    */
   BESContainer* addNewContainerToStorage();

   /** Remove the symbol we added in addNewContainerToStorage if it's there. */
   void removeContainerFromStorage();

   /** Make sure we clean up anything we've touched.
    * On exit, everything should be in the same state as construction.
    */
   void ensureClean();

  }; // class DDSLoader
} // namespace ncml_module

#endif /* DDSLOADER_H_ */
