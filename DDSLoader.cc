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
#include "DDSLoader.h"

#include <sstream>

#include <BESConstraintFuncs.h>
#include <BESContainerStorage.h>
#include <BESContainerStorageList.h>
#include <BESDapNames.h>
#include <BESDapResponse.h>
#include <BESDataDDSResponse.h>
#include <BESDataHandlerInterface.h>
#include <BESDDSResponse.h>
#include <BESDebug.h>
#include <BESInternalError.h>
#include <BESResponseHandler.h>
#include <BESResponseNames.h>
#include <BESRequestHandlerList.h>
#include <BESServiceRegistry.h>
#include <BESTextInfo.h>
#include <BESUtil.h>
#include <BESVersionInfo.h>
//#include <mime_util.h>
#include "NCMLDebug.h"
#include "NCMLUtil.h"


using namespace std;
using namespace agg_util;


// Rep Init

/* static */
long DDSLoader::_gensymID = 0L;

// Impl

DDSLoader::DDSLoader(BESDataHandlerInterface& dhi)
: _dhi(dhi)
, _hijacked(false)
, _filename("")
, _store(0)
, _containerSymbol("")
, _origAction("")
, _origActionName("")
, _origContainer(0)
, _origResponse(0)
{
}

// WE ONLY COPY THE DHI!  I got forced to impl this.
DDSLoader::DDSLoader(const DDSLoader& proto)
: _dhi(proto._dhi)
, _hijacked(false)
, _filename("")
, _store(0)
, _containerSymbol("")
, _origAction("")
, _origActionName("")
, _origContainer(0)
, _origResponse(0)
{
}

DDSLoader&
DDSLoader::operator=(const DDSLoader& rhs)
{
  if (&rhs == this)
    {
      return *this;
    }

  // First cleanup any state
  ensureClean();

  // Now copy the dhi only, since
  // we assume we'll be using this fresh.
  // Normally we don't want to copy these
  // but I got forced into it.
  _dhi.make_copy(rhs._dhi);
  return *this;
}


DDSLoader::~DDSLoader()
{
  ensureClean();
}

auto_ptr<BESDapResponse>
DDSLoader::load(const string& location, ResponseType type)
{
  // We need to make the proper response object as well, since in this call the dhi is coming in with the
  // response object for the original ncml request.
  std::auto_ptr<BESDapResponse> response = makeResponseForType(type);
  loadInto(location, type, response.get());
  return response; // relinquish
}

void
DDSLoader::loadInto(const std::string& location, ResponseType type, BESDapResponse* pResponse)
{
  VALID_PTR(pResponse);
  VALID_PTR(_dhi.response_handler);

  // Just be sure we're cleaned up before doing anything, in case the caller calls load again after exception
  // and before dtor.
  ensureClean();

  _filename = location;

  // Remember current state of dhi before we touch it -- _hijacked is now true!!
  snapshotDHI();

  // Add a new symbol to the storage list and return container for it.
  // We will remove this new container on the way out.
  BESContainer* container = addNewContainerToStorage();

  // Take over the dhi
  _dhi.container = container;
  _dhi.response_handler->set_response_object(pResponse);

  // Choose the proper request type...
  _dhi.action = getActionForType(type);
  _dhi.action_name = getActionNameForType(type);

  // TODO mpj do we need to do these calls?
  BESDEBUG( "ncml", "about to set dap version to: "
        << pResponse->get_dap_client_protocol() << endl);
  BESDEBUG( "ncml", "about to set xml:base to: "
                           << pResponse->get_request_xml_base() << endl);

  // Figure out which underlying type of response it is to get the DDS (or DataDDS via DDS super).
  DDS* pDDS = ncml_module::NCMLUtil::getDDSFromEitherResponse(pResponse);
  if (!pDDS)
    {
      THROW_NCML_INTERNAL_ERROR("DDSLoader::load expected BESDDSResponse or BESDataDDSResponse but got neither!");
    }
  pDDS->set_request_xml_base( pResponse->get_request_xml_base() );

#if 0
  // I think this should be removed. pDDSResponse was likely changed to pResponse
  // and pDDS is the same object. The BES will set these for us.
  // I took these out since they seem to have changed and I am not sure what the right thing to do is...
  pDDS->set_dap_major( pDDSResponse->get_dds()->get_dap_major() );
  pDDS->set_dap_minor( pDDSResponse->get_dds()->get_dap_major() );
#endif

  // DO IT!
  BESRequestHandlerList::TheList()->execute_current( _dhi ) ;

  // Put back the dhi state we hijacked
  restoreDHI();

  // Get rid of the container we added.
  removeContainerFromStorage();

  _filename = "";

  // We should be clean here too.
  ensureClean();
}


void
DDSLoader::cleanup() throw()
{
  ensureClean();
}

BESContainer*
DDSLoader::addNewContainerToStorage()
{
  // Make sure we can find the storage
  BESContainerStorageList *store_list = BESContainerStorageList::TheList() ;
  VALID_PTR(store_list);
  BESContainerStorage* store = store_list->find_persistence( "catalog" ) ;
  if( !store )
    {
      throw BESInternalError( "couldn't find the catalog storage",
          __FILE__, __LINE__ ) ;
    }

  // Make a new symbol from the ncml filename
  // NCML_ASSERT_MSG(_dhi.container, "DDSLoader::addNewContainerToStorage(): null container!");
  // string newSymbol = _dhi.container->get_symbolic_name() + "_location_" + _filename;
  string newSymbol = getNextContainerName() + "__" + _filename;

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
DDSLoader::removeContainerFromStorage() throw()
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
      catch (BESError& besErr)
      {
        BESDEBUG("ncml",
            "WARNING: tried to remove symbol " << _containerSymbol <<
            " from singleton but unexpectedly it was not there." << endl);
      }
      _containerSymbol = "";
      _store = 0;
    }
}

void
DDSLoader::snapshotDHI()
{
  VALID_PTR(_dhi.response_handler);

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
  VALID_PTR(_dhi.response_handler);

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
DDSLoader::ensureClean() throw()
{
  // If we're still hijacked here, there was an exception in load, so clean
  // up if needed.
  if (_hijacked)
    {
      restoreDHI();
    }

   // Make sure we've removed the new symbol from the container list as well.
   removeContainerFromStorage();
}

/* static */
std::string
DDSLoader::getNextContainerName()
{
  static const string _sPrefix = "__DDSLoader_Container_ID_";
  _gensymID++;
  std::ostringstream oss;
  oss << _sPrefix << (_gensymID);
  return oss.str();
}


std::auto_ptr<BESDapResponse>
DDSLoader::makeResponseForType(ResponseType type)
{
  if (type == eRT_RequestDDX)
    {
      return auto_ptr<BESDapResponse>(new BESDDSResponse( new DDS( new BaseTypeFactory(), "virtual" )  )) ;
    }
  else if (type == eRT_RequestDataDDS)
    {
      return auto_ptr<BESDapResponse>(new BESDataDDSResponse( new DataDDS( new BaseTypeFactory(), "virtual" )  )) ;
    }
  else
    {
      THROW_NCML_INTERNAL_ERROR("DDSLoader::makeResponseForType() got unknown type!");
    }
  return auto_ptr<BESDapResponse>(0);
}

std::string
DDSLoader::getActionForType(ResponseType type)
{
  if (type == eRT_RequestDDX)
      {
        return DDS_RESPONSE;
      }
    else if (type == eRT_RequestDataDDS)
      {
        return DATA_RESPONSE;
      }

    THROW_NCML_INTERNAL_ERROR("DDSLoader::getActionForType(): unknown type!");
}

std::string
DDSLoader::getActionNameForType(ResponseType type)
{
  if (type == eRT_RequestDDX)
    {
      return DDX_RESPONSE_STR;
    }
  else if (type == eRT_RequestDataDDS)
    {
      return DATA_RESPONSE_STR;
    }

  THROW_NCML_INTERNAL_ERROR("DDSLoader::getActionNameForType(): unknown type!");
}

bool
DDSLoader::checkResponseIsValidType(ResponseType type, BESDapResponse* pResponse)
{
  if (type == eRT_RequestDDX)
    {
      return dynamic_cast<BESDDSResponse*>(pResponse);
    }
  else if (type == eRT_RequestDataDDS)
    {
      return dynamic_cast<BESDataDDSResponse*>(pResponse);
    }
  else
    {
      return false;
    }
}



