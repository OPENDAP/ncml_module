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

class BESDataHandlerInterface;
class BESContainerStorage;
class BESDDSResponse;

/**
  @brief A helper class for loading the DDX of a local datafile.

  The dhi handed in at creation will be hijacked to do the loading.
  We store all the dhi state we mutate when we hijack in the instance
  so that we can put it all back correctly if there's an exception
  while getting the requested DDX.

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

  // Remember the store so we can pull the location out in restoreDHI on exception as well.
  BESContainerStorage* _store;

  // DHI state we hijack, for putting back on exception.
  string _containerSymbol;
  string _origAction;
  string _origActionName;
  BESContainer* _origContainer;
  BESResponseObject* _origResponse;

  // Used to generate unique symbols each time we load an object...
  static unsigned int _sNextID;

private:
  DDSLoader(const DDSLoader&); // disallow
  DDSLoader& operator=(const DDSLoader&); // disallow

public:
  /**
   * the dhi to be used for the purposes of loading from this class.
   * We mutate it temporarily in createNewDDXForLocation, but put
   * the state back on return from the call.
   */
  DDSLoader(BESDataHandlerInterface &dhi);

  virtual ~DDSLoader();

  /**
   * @brief Load and return a new DDX structure for the local dataset referred to by location.
   * Ownership of the response object passes to the caller to destroy when done.
   *
   * @param \c location  the local dataset to load the DDX for.
   *
   * @return a newly allocated DDX for the location.
   *
   * @exception if the underlying location cannot be loaded.
   */
  BESDDSResponse* createNewDDXForLocation(const string& location);


private:
  /**
   * Helper that does the actual work of loading the location into ddsRespOut.
   *
   * Invariant: The _dhi is mutated during this call, but is required to be put back as it was at the start of the
   * call when we return, even via exception.
   */
   void loadDDXForLocation(BESDDSResponse* ddsRespOut, const string& location);

   /**
    *  Remember the state of the _dhi we plan to change.
    *  Also remember the persistent store and the symbol we added
    *  the location under so we can remove those as well.
    */
   void snapshotDHI(const string& newContainerSymbol, BESContainerStorage* store);

   /**
    * Put back the _dhi state we hijacked.
    * This must be called if an exception occurs by any function in here.
    */
   void restoreDHI();

   static unsigned int sGetNewID() { return _sNextID++; }
}; // class DDSLoader
} // namespace ncml_module

#endif /* DDSLOADER_H_ */
