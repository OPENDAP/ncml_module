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
#include "NCMLRequestHandler.h"

#include <BESConstraintFuncs.h>
#include <BESContainerStorage.h>
#include <BESContainerStorageList.h>
#include "BESDataDDSResponse.h"
#include <BESDataNames.h>
#include <BESDASResponse.h>
#include <BESDDSResponse.h>
#include <BESDebug.h>
#include <BESInternalError.h>
#include <BESRequestHandlerList.h>
#include <BESResponseHandler.h>
#include <BESResponseNames.h>
#include <BESServiceRegistry.h>
#include <BESTextInfo.h>
#include <BESUtil.h>
#include <BESVersionInfo.h>
#include "cgi_util.h"
#include "DDSLoader.h"
#include <memory>
#include "NCMLDebug.h"
#include "NCMLUtil.h"
#include "NCMLParser.h"
#include "NCMLResponseNames.h"
#include "SimpleLocationParser.h"


using namespace ncml_module;


NCMLRequestHandler::NCMLRequestHandler( const string &name )
    : BESRequestHandler( name )
{
    add_handler( DAS_RESPONSE, NCMLRequestHandler::ncml_build_das ) ;
    add_handler( DDS_RESPONSE, NCMLRequestHandler::ncml_build_dds ) ;
    add_handler( DATA_RESPONSE, NCMLRequestHandler::ncml_build_data ) ;
    add_handler( VERS_RESPONSE, NCMLRequestHandler::ncml_build_vers ) ;
    add_handler( HELP_RESPONSE, NCMLRequestHandler::ncml_build_help ) ;
}

NCMLRequestHandler::~NCMLRequestHandler()
{
}

// This is the original example from Patrick or James for loading local file within the BES...
// Still used by DataDDS call, but the other callbacks use DDSLoader
// to get a brandy new DDX response.
// @see DDSLoader
bool
NCMLRequestHandler::ncml_build_redirect( BESDataHandlerInterface &dhi, const string& location )
{
    // The current container in dhi is a reference to the ncml file.
    // Need to parse the ncml file here and get the list of locations
    // that we will be using. Any constraints defined?

    // do this for each of the locations retrieved from the ncml file.
    // If there are more than one locations in the ncml then we can't
    // set the context for dap_format to dap2. This will create a
    // structure for each of the locations in the resulting dap object.
    string sym_name = dhi.container->get_symbolic_name() ;
    BESContainerStorageList *store_list = BESContainerStorageList::TheList() ;
    BESContainerStorage *store = store_list->find_persistence( "catalog" ) ;
    if( !store )
    {
	throw BESInternalError( "couldn't find the catalog storage",
				__FILE__, __LINE__ ) ;
    }
    // this will throw an exception if the location isn't found in the
    // catalog. Might want to catch this. Wish the add returned the
    // container object created. Might want to change it.
    string new_sym = sym_name + "_location1" ;
    store->add_container( new_sym, location, "" ) ;

    BESContainer *container = store->look_for( new_sym ) ;
    if( !container )
    {
	throw BESInternalError( "couldn't find the container" + sym_name,
				__FILE__, __LINE__ ) ;
    }
    BESContainer *ncml_container = dhi.container ;
    dhi.container = container ;

    // this will throw an exception if there is a problem building the
    // response for this container. Might want to catch this
    BESRequestHandlerList::TheList()->execute_current( dhi ) ;

    // clean up
    dhi.container = ncml_container ;
    store->del_container( new_sym ) ;

    return true;
}

// Here we load the DDX response with by hijacking the current dhi via DDSLoader
// and hand it to our parser to load the ncml, load the DDX for the location,
// apply ncml transformations to it, then return the modified DDS.
bool
NCMLRequestHandler::ncml_build_das( BESDataHandlerInterface &dhi )
{
    string filename = dhi.container->access();

    // Any exceptions winding through here will cause the loader and parser dtors
    // to clean up dhi state, etc.
    DDSLoader loader(dhi);
    NCMLParser parser(loader);
    auto_ptr<BESDDSResponse> loaded_bdds = parser.parse(filename);

    if (!(loaded_bdds.get()))
      {
        throw BESInternalError("Null BESDDSResonse in ncml DAS handler.", __FILE__, __LINE__);
      }

    // Now fill in the desired DAS response object from the DDS
    DDS* dds = loaded_bdds->get_dds();
    VALID_PTR(dds);
    BESResponseObject *response =
     dhi.response_handler->get_response_object();
    BESDASResponse *bdas = dynamic_cast < BESDASResponse * >(response);
    VALID_PTR(bdas);

    // Copy the modified DDS attributes into the DAS response object!
    DAS *das = bdas->get_das();
    BESDEBUG("ncml", "Creating DAS response from the location DDX..." << endl);
    NCMLUtil::populateDASFromDDS(das, *dds);

    // loaded_bdds destroys itself.
    return false ;
}

bool
NCMLRequestHandler::ncml_build_dds( BESDataHandlerInterface &dhi )
{
    string filename = dhi.container->access();

    // Any exceptions winding through here will cause the loader and parser dtors
    // to clean up dhi state, etc.
    auto_ptr<BESDDSResponse> loaded_bdds(0);
    {
      DDSLoader loader(dhi);
      NCMLParser parser(loader);
      loaded_bdds = parser.parse(filename);
    }
    if (!loaded_bdds.get())
      {
        throw BESInternalError("Null BESDDSResonse in ncml DDS handler.", __FILE__, __LINE__);
      }

    // Poke the handed down original response object with the loaded and modified one.
    DDS* dds = loaded_bdds->get_dds();
    VALID_PTR(dds);
    BESResponseObject *response =
      dhi.response_handler->get_response_object();
    BESDDSResponse *bdds_out = dynamic_cast < BESDDSResponse * >(response);
    VALID_PTR(bdds_out);
    DDS *dds_out = bdds_out->get_dds();
    VALID_PTR(dds_out);

    // If we just use DDS::operator=, we get into trouble with copied
    // pointers, bashing of the dataset name, etc etc so I specialize the copy for now.
    NCMLUtil::copyVariablesAndAttributesInto(dds_out, *dds);

    // Apply constraints to the result
    dhi.data[POST_CONSTRAINT] = dhi.container->get_constraint();

    // Also copy in the name of the original ncml request
    // TODO @HACK Not sure I want just the basename for the filename,
    // but since the DDS/DataDDS response fills the dataset name with it,
    // Our bes-testsuite fails since we get local path info in the dataset name.
    dds_out->filename(name_path(filename));
    dds_out->set_dataset_name(name_path(filename));
    // Is there anything else I need to shove in the DDS?

    return true;
}

bool
NCMLRequestHandler::ncml_build_data( BESDataHandlerInterface &dhi )
{
  // The design doc says just load underlying data set and pass through,
  // which will be fine until we have the ability to add variables with ncml,
  // For now, we'll just passthrough.

  // First, parse the ncml to get the netcdf@location for the underlying dataset.
  string ncmlFilename = dhi.container->access();
  string location("");

  { // Force dtors
    SimpleLocationParser parser;
    location = parser.parseAndGetLocation(ncmlFilename);
  }

  BESDEBUG("ncml", "DataDDS request from NcML filename:" << ncmlFilename << " Redirecting to underlying location=" << location << endl);

  // Just redirect the dods call to the dataset at the new location to fill
  // the dhi BESDataDDSResponse object.
  bool ret = ncml_build_redirect(dhi, location);

  // Change the dataset name to the ncml filename though, since technically that's what it is,
  // though it's a "virtual" dataset.  It might make sense to automatically augment the global attribute table with a new
  // attribute call "OriginalDataset" with the netcdf@location in it, or something as we move forward so
  // clients can know where the un-AIS'd data lives.

  BESResponseObject *response =
    dhi.response_handler->get_response_object();
  BESDataDDSResponse *bdds = dynamic_cast < BESDataDDSResponse * >(response);
  if (!bdds)
    {
      BESDEBUG("ncml", "Failed to make DataDDS for ncml file: " << ncmlFilename << endl);
    }
  else
    {
      DDS* dds = bdds->get_dds();
      VALID_PTR(dds);
      string datasetName = dhi.container->get_real_name();
      // Use the basename... Is that what we want to do?  That's what nc handler does
      // and this will make our make check work by not having local paths in the output...
      dds->set_dataset_name(name_path(ncmlFilename));
      dds->filename(name_path(ncmlFilename));

      // Apply constraints to the result
      dhi.data[POST_CONSTRAINT] = dhi.container->get_constraint();
    }

  return ret;
}

bool
NCMLRequestHandler::ncml_build_vers( BESDataHandlerInterface &dhi )
{
    bool ret = true ;
    BESVersionInfo *info = dynamic_cast<BESVersionInfo *>(dhi.response_handler->get_response_object() ) ;
    info->add_module( PACKAGE_NAME, PACKAGE_VERSION ) ;
    return ret ;
}

bool
NCMLRequestHandler::ncml_build_help( BESDataHandlerInterface &dhi )
{
    bool ret = true ;
    BESInfo *info = dynamic_cast<BESInfo *>(dhi.response_handler->get_response_object());

    // This is an example. If you had a help file you could load it like
    // this and if your handler handled the following responses.
    map<string,string> attrs ;
    attrs["name"] = PACKAGE_NAME ;
    attrs["version"] = PACKAGE_VERSION ;
    list<string> services ;
    BESServiceRegistry::TheRegistry()->services_handled( NCML_NAME, services );
    if( services.size() > 0 )
    {
	string handles = BESUtil::implode( services, ',' ) ;
	attrs["handles"] = handles ;
    }
    info->begin_tag( "module", &attrs ) ;
    //info->add_data_from_file( "NCML.Help", "NCML Help" ) ;
    info->end_tag( "module" ) ;

    return ret ;
}

void
NCMLRequestHandler::dump( ostream &strm ) const
{
    strm << BESIndent::LMarg << "NCMLRequestHandler::dump - ("
			     << (void *)this << ")" << endl ;
    BESIndent::Indent() ;
    BESRequestHandler::dump( strm ) ;
    BESIndent::UnIndent() ;
}

