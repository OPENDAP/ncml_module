/*
 *  @author mjohnson <m.johnson@opendap.org>
 */
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

using namespace ncml_module;

unsigned int
DDSLoader::_sNextID = 0;

DDSLoader::DDSLoader(BESDataHandlerInterface& dhi)
: _dhi(dhi)
, _hijacked(false)
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
}

// TODO make sure that on exceptions being thrown we clean up the dhi on the way out
// or create the proper response
BESDDSResponse*
DDSLoader::createNewDDXForLocation(const string& location)
{
  // We need to make the proper response object as well, since the dhi is coming in with the
  // response object for the original ncml request.
  DDS *dds = new DDS( NULL, "virtual" ) ;
  BESDDSResponse *bdds = new BESDDSResponse( dds ) ;

  try
  {
    loadDDXForLocation(bdds, location);
  }
  catch (const BESError& ex)
  {
    // avoid leak
    delete bdds;
    throw ex;
  }
  catch(...)
  {
    // avoid leak
    delete bdds;
    throw BESInternalError("Unknown exception, restoring DHI.", __FILE__, __LINE__);
  }

  // return the dynamically created DDX response, passing ownership to caller.
  return bdds;
}

/* static */
void
DDSLoader::loadDDXForLocation(BESDDSResponse* ddsRespOut, const string& location)
{
  // This is the symbolic name for the NCML call...
   string sym_name = _dhi.container->get_symbolic_name() ;

   BESContainerStorageList *store_list = BESContainerStorageList::TheList() ;
   BESContainerStorage *store = store_list->find_persistence( "catalog" ) ;
   if( !store )
   {
       throw BESInternalError( "couldn't find the catalog storage",
                               __FILE__, __LINE__ ) ;
   }

   // TODO Generate a unique id string to shove on the end of this.
   string new_sym = sym_name + "_location_";

   // this will throw an exception if the location isn't found in the
   // catalog. Might want to catch this. Wish the add returned the
   // container object created. Might want to change it.
   store->add_container( new_sym, location, "" ) ;

   BESContainer *container = store->look_for( new_sym ) ;
   if( !container )
   {
       throw BESInternalError( "couldn't find the container" + sym_name,
                               __FILE__, __LINE__ ) ;
   }

   // Remember the DHI state we're gonna change, and the symbol and store for the new container so we can remove it
   // in restoreDHI as well if any exception occur.
   snapshotDHI(new_sym, store);

   // shove in our container and ask for a new DDX .
   _dhi.container = container;
   _dhi.response_handler->set_response_object(ddsRespOut);
   _dhi.action = DDS_RESPONSE;
   _dhi.action_name = DDX_RESPONSE_STR;

   // TODO mpj do we need to do these calls?
   BESDEBUG( "ncml", "about to set dap version to: "
          << ddsRespOut->get_dap_client_protocol() << endl);
   BESDEBUG( "ncml", "about to set xml:base to: "
                        << ddsRespOut->get_request_xml_base() << endl);
   DDS *dds = ddsRespOut->get_dds();
   dds->set_client_dap_version( ddsRespOut->get_dap_client_protocol() ) ;
   dds->set_request_xml_base( ddsRespOut->get_request_xml_base() );

#if 0
   // Also remember the action, then ask for the other one.
   string orig_action = dhi.action;
   string orig_action_name = dhi.action_name;
   // Also Remember the original response object to put back afterwards...
   BESResponseObject* orig_response = dhi.response_handler->get_response_object();

   // Hijack the dhi to get what we want.
   dhi.response_handler->set_response_object(ddsRespOut);
   dhi.action = DDS_RESPONSE;
   dhi.action_name = DDX_RESPONSE_STR;

   // TODO mpj do we need to do these calls?
   BESDEBUG( "ncml", "about to set dap version to: "
       << ddsRespOut->get_dap_client_protocol() << endl);
   BESDEBUG( "ncml", "about to set xml:base to: "
                     << ddsRespOut->get_request_xml_base() << endl);
   DDS *dds = ddsRespOut->get_dds();
   dds->set_client_dap_version( ddsRespOut->get_dap_client_protocol() ) ;
   dds->set_request_xml_base( ddsRespOut->get_request_xml_base() );
#endif

   // Attempt to get DDX for the given location.  Restore DHI on exception.
   BESDEBUG("ncml", "Attempting to get DDX response for location:" << location << endl);
   try
   {
     BESRequestHandlerList::TheList()->execute_current( _dhi ) ;
   }
   catch (BESError& ex)
   {
       // cleanup
       restoreDHI();
       // Consider modifying the exception with new info for this context....
       throw ex;
   }
   catch (...)
   {
     // cleanup dhi
     restoreDHI();
     string msg = "Unknown exception trying to load DDX:";
     BESDEBUG("ncml", msg << " referrer: " << sym_name << " requested location:" << location << endl);
     throw new BESInternalError(msg, __FILE__, __LINE__);
   }
   // Cleanup!  This removes the new catalog from the store as well.
   restoreDHI();

   BESDEBUG("ncml", "Got DDX response for location:" << location << endl);

#if 0
   // Put the actions back to the original request
   dhi.action = orig_action;
   dhi.action_name = orig_action_name;
   // Put the original response object back!!
   dhi.response_handler->set_response_object(orig_response);

   // reset and clear tempcontainer
   dhi.container = ncml_container ;
#endif

}

void
DDSLoader::snapshotDHI(const string& containerSymbol, BESContainerStorage* store)
{
  _containerSymbol = containerSymbol;

  // Store off the container for the original ncml file call and replace with the new one
  _origContainer = _dhi.container ;
  _origAction = _dhi.action;
  _origActionName = _dhi.action_name;
  _origResponse = _dhi.response_handler->get_response_object();

   _store = store;
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

  // Nuke the symbol we added
  if (_store)
    {
      _store->del_container( _containerSymbol ); // TODO do we want this in the cleanup call too on exception?
    }

  // clear state
  _store = 0;
  _origAction = "";
  _origActionName = "";
  _origResponse = 0;
  _origContainer = 0;
  _hijacked = false;
}

