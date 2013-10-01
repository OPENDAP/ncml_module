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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////

#include "config.h"

#include <iostream>

#include <BESCatalogDirectory.h>
#include <BESCatalogList.h>
#include <BESContainerStorageList.h>
#include <BESContainerStorageCatalog.h>
#include <BESDapService.h>
#include <BESDebug.h>
#include <BESRequestHandlerList.h>
#include <BESResponseHandlerList.h>
#include <BESResponseNames.h>
#include <BESXMLCommand.h>
#include <BESContainerStorageList.h>
#include <TheBESKeys.h>
#include <BESInternalError.h>

#include "NCMLModule.h"
#include "NCMLCacheAggXMLCommand.h"
#include "NCMLRequestHandler.h"
#include "NCMLResponseNames.h"
#include "NCMLContainerStorage.h"

using std::endl;
using namespace ncml_module;

static const char* const NCML_CATALOG = "catalog";

void
NCMLModule::initialize( const string &modname )
{
    BESDEBUG( modname, "Initializing NCML Module " << modname << endl );

    BESDEBUG( modname, "    adding " << modname << " request handler" << endl );
    BESRequestHandlerList::TheList()->add_handler( modname, new NCMLRequestHandler( modname ) ) ;

    // If new commands are needed, then let's declare this once here. If
    // not, then you can remove this line.
    addCommandAndResponseHandlers(modname);

    // Dap services
    BESDEBUG( modname, modname << " handles dap services" << endl );
    BESDapService::handle_dap_service( modname ) ;

    BESDEBUG( modname, "    adding " << NCML_CATALOG << " catalog" << endl );
    if( !BESCatalogList::TheCatalogList()->ref_catalog( NCML_CATALOG ) )
      {
        BESCatalogList::TheCatalogList()->add_catalog( new BESCatalogDirectory( NCML_CATALOG ) ) ;
     }
     else
     {
         BESDEBUG( modname, "    catalog already exists, skipping" << endl );
     }

     BESDEBUG( modname, "    adding catalog container storage " << NCML_CATALOG
                     << endl );
     if( !BESContainerStorageList::TheList()->ref_persistence( NCML_CATALOG ) )
     {
         BESContainerStorageCatalog *csc =
             new BESContainerStorageCatalog( NCML_CATALOG ) ;
         BESContainerStorageList::TheList()->add_persistence( csc ) ;
     }
     else
     {
         BESDEBUG( modname, "    storage already exists, skipping" << endl );
     }

    BESDEBUG( modname, "    adding " << modname << " container storage" << endl );
    BESContainerStorageList::TheList()->
	add_persistence( new NCMLContainerStorage( modname ) ) ;

    string key = "NCML.TempDirectory" ;
    BESDEBUG( modname, "    checking " << key << " parameter" << endl ) ;
    string val ;
    bool found = false ;
    TheBESKeys::TheKeys()->get_value( key, val, found ) ;
    if( !found || val.empty() || val == "/" )
    {
	string err = (string)"The parameter " + key
		     + " must be set to use the NCML module" ;
	throw BESInternalError( err, __FILE__, __LINE__ ) ;
    }
    NCMLContainerStorage::NCML_TempDir = val ;

    BESDEBUG( modname, "    adding NCML debug context" << endl );
    BESDebug::Register( modname ) ;

    BESDEBUG( modname, "Done Initializing NCML Module " << modname << endl );
}

void
NCMLModule::terminate( const string &modname )
{
    BESDEBUG( modname, "Cleaning NCML module " << modname << endl );

    BESDEBUG( modname, "    removing " << modname << " request handler" << endl );
    BESRequestHandler *rh = BESRequestHandlerList::TheList()->remove_handler( modname ) ;
    if( rh ) delete rh ;

    // If new commands were added, remove them here.
    removeCommandAndResponseHandlers();

    BESDEBUG( modname, "    removing catalog container storage"
                     << NCML_CATALOG << endl );
    BESContainerStorageList::TheList()->deref_persistence( NCML_CATALOG ) ;

    BESDEBUG( modname, "    removing ncml container storage" << endl ) ;
    BESContainerStorageList::TheList()->deref_persistence( modname ) ;

    BESDEBUG( modname, "    removing " << NCML_CATALOG << " catalog" << endl );
    BESCatalogList::TheCatalogList()->deref_catalog( NCML_CATALOG ) ;

    // TERM_END
    BESDEBUG( modname, "Done Cleaning NCML module " << modname << endl );
}

extern "C"
{
    BESAbstractModule *maker()
    {
	return new NCMLModule ;
    }
}

void
NCMLModule::dump( ostream &strm ) const
{
    strm << BESIndent::LMarg << "NCMLModule::dump - ("
			     << (void *)this << ")" << endl ;
}

void
NCMLModule::addCommandAndResponseHandlers(const string& modname)
{
  BESDEBUG(modname, "Adding module extensions..." << endl);
  addCacheAggCommandAndResponseHandlers(modname);
  BESDEBUG(modname, "... done adding module extensions." << endl);
}

void
NCMLModule::addCacheAggCommandAndResponseHandlers(const string& modname)
{
  string cmdName = ModuleConstants::CACHE_AGG_RESPONSE ;

  BESDEBUG( modname, "    adding "
      << cmdName
      << " response handler" << endl ) ;
  BESResponseHandlerList::TheList()->add_handler(
      cmdName,
      NCMLCacheAggResponseHandler::makeInstance);

  BESDEBUG(modname, "    adding " << cmdName << " command" << endl ) ;
  BESXMLCommand::add_command(
      cmdName,
      NCMLCacheAggXMLCommand::makeInstance);
}

void
NCMLModule::removeCommandAndResponseHandlers()
{
  BESDEBUG(ModuleConstants::NCML_NAME, "Removing module extensions..." << endl);
  removeCacheAggCommandAndResponseHandlers();
  BESDEBUG(ModuleConstants::NCML_NAME, "... done removing module extensions." << endl);
}

void
NCMLModule::removeCacheAggCommandAndResponseHandlers()
{
  string cmdName = ModuleConstants::CACHE_AGG_RESPONSE;

  BESDEBUG( ModuleConstants::NCML_NAME, "    removing " << cmdName
      << " response handler" << endl ) ;
  BESResponseHandlerList::TheList()->remove_handler( cmdName ) ;

  BESDEBUG( ModuleConstants::NCML_NAME, "    removing " << cmdName << " command" << endl ) ;
  BESXMLCommand::del_command( cmdName ) ;
}

