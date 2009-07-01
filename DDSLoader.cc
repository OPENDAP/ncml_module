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
#include "config.h"

#include "cgi_util.h"
#include <BESResponseHandler.h>
#include <BESResponseNames.h>
#include <BESVersionInfo.h>
#include <BESTextInfo.h>
#include "BESDDSResponse.h"
#include <BESConstraintFuncs.h>
#include <BESServiceRegistry.h>
#include <BESUtil.h>
#include <BESInternalError.h>
#include <BESContainerStorageList.h>
#include <BESContainerStorage.h>
#include <BESRequestHandlerList.h>
#include <BESDebug.h>

#include "DDSLoader.h"

using namespace std;
using namespace ncml_module;

DDSLoader::DDSLoader(BESDataHandlerInterface& dhi)
: _dhi(dhi)
, _hijacked(false)
, _filename("")
, _ddxResponse(0)
, _store(0)
, _containerSymbol("")
, _origAction("")
, _origActionName("")
, _origContainer(0)
, _origResponse(0)
{
}

DDSLoader::~DDSLoader()
{
  ensureClean();
}

// TODO make sure that on exceptions being thrown we clean up the dhi on the way out
// or create the proper response
BESDDSResponse*
DDSLoader::load(const string& location)
{
  // Just be sure we're cleaned up before doing anything, in case the caller calls load again after exception
  // and before dtor.
  ensureClean();

  _filename = location;

  // Remember current state of dhi before we touch it -- _hijacked is now true!!
  snapshotDHI();

  // We need to make the proper response object as well, since the dhi is coming in with the
  // response object for the original ncml request.
  _ddxResponse = new BESDDSResponse( new DDS( NULL, "virtual" )  ) ;

  // Add a new symbol to the storage list and return container for it.
  // We will remove this new container on the way out.
  BESContainer* container = addNewContainerToStorage();

  // Take over the dhi
  _dhi.container = container;
  _dhi.response_handler->set_response_object(_ddxResponse);
  _dhi.action = DDS_RESPONSE;
  _dhi.action_name = DDX_RESPONSE_STR;

  // TODO mpj do we need to do these calls?
  BESDEBUG( "ncml", "about to set dap version to: "
      << _ddxResponse->get_dap_client_protocol() << endl);
  BESDEBUG( "ncml", "about to set xml:base to: "
                         << _ddxResponse->get_request_xml_base() << endl);
  _ddxResponse->get_dds()->set_client_dap_version( _ddxResponse->get_dap_client_protocol() ) ;
  _ddxResponse->get_dds()->set_request_xml_base( _ddxResponse->get_request_xml_base() );

  // DO IT!
  BESRequestHandlerList::TheList()->execute_current( _dhi ) ;

  // Put back the dhi state we hijacked
  restoreDHI();

  // Get rid of the container we added.
  removeContainerFromStorage();

  _filename = "";

  // Get ready to return the dynamically created DDX response, passing ownership to caller by nulling ours.
  BESDDSResponse* ret = _ddxResponse;
  _ddxResponse = 0;

  // We should be clean here too.
  ensureClean();

  return ret;
}

void
DDSLoader::cleanup()
{
  ensureClean();
}


BESContainer*
DDSLoader::addNewContainerToStorage()
{
  // Make sure we can find the storage
  BESContainerStorageList *store_list = BESContainerStorageList::TheList() ;
  BESContainerStorage* store = store_list->find_persistence( "catalog" ) ;
  if( !store )
    {
      throw BESInternalError( "couldn't find the catalog storage",
          __FILE__, __LINE__ ) ;
    }

  // Make a new symbol from the ncml filename
  string newSymbol = _dhi.container->get_symbolic_name() + "_location_" + _filename;

  // this will throw an exception if the location isn't found in the
  // catalog. Might want to catch this. Wish the add returned the
  // container object created. Might want to change it.
  store->add_container( newSymbol, _filename, "" ) ;

  // If we were successful, note the store location and symbol we added  for removal later.
  _store = store;
  _containerSymbol = newSymbol;

  // Now look up the symbol we added
  BESContainer *container = store->look_for( _containerSymbol ) ;
  if( !container )
    {
      throw BESInternalError( "couldn't find the container we just added:" + newSymbol,
          __FILE__, __LINE__ ) ;
    }

  return container;
}

void
DDSLoader::removeContainerFromStorage()
{
  // If we have non-null _store, we added the container symbol,
  // so get rid of it
  if (_store)
    {
      try
      {
        // This should go through, but if there's an exception, we could unwind through the dtor,
        // so make sure we don't.
        _store->del_container( _containerSymbol );
      }
      catch (...)
      {
        BESDEBUG("ncml", "WARNING: tried to remove symbol " << _containerSymbol << " from singleton but unexpectedly it was not there." << endl);
      }
      _containerSymbol = "";
      _store = 0;
    }
}

void
DDSLoader::snapshotDHI()
{
  // Store off the container for the original ncml file call and replace with the new one
  _origContainer = _dhi.container ;
  _origAction = _dhi.action;
  _origActionName = _dhi.action_name;
  _origResponse = _dhi.response_handler->get_response_object();

   _hijacked = true;
}

void
DDSLoader::restoreDHI()
{
  // Make sure we have state before we go mucking
  if (!_hijacked)
    {
      return;
    }

  // Restore saved state
  _dhi.action = _origAction;
  _dhi.action_name = _origActionName;
  _dhi.response_handler->set_response_object(_origResponse);
  _dhi.container = _origContainer;

  // clear our copy of saved state
  _origAction = "";
  _origActionName = "";
  _origResponse = 0;
  _origContainer = 0;
  _filename = "";

  _hijacked = false;
}

void
DDSLoader::ensureClean()
{
  // If we're still hijacked here, there was an exception in load, so clean
  // up if needed.
  if (_hijacked)
    {
      restoreDHI();
    }

  // If we still have _ddxResponse, we also failed to load() properly and
  // relinquish ownership.  Clean it up as we stack unwind!
  if (_ddxResponse)
     {
       delete _ddxResponse;
       _ddxResponse = 0;
     }

   // Make sure we've removed the new symbol from the container list as well.
   removeContainerFromStorage();
}
