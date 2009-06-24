// NCMLRequestHandler.cc

#include "config.h"

#include "cgi_util.h"
#include "NCMLRequestHandler.h"
#include <BESResponseHandler.h>
#include <BESResponseNames.h>
#include "NCMLResponseNames.h"
#include <BESVersionInfo.h>
#include <BESTextInfo.h>
#include "BESDataDDSResponse.h"
#include "BESDDSResponse.h"
#include "BESDASResponse.h"
#include <BESConstraintFuncs.h>
#include <BESServiceRegistry.h>
#include <BESUtil.h>
#include <BESInternalError.h>
#include <BESContainerStorageList.h>
#include <BESContainerStorage.h>
#include <BESRequestHandlerList.h>
#include <BESDebug.h>
#include "NCMLParser.h"
#include "DDSLoader.h"

// HACK while we test the handler without a real NcML parser, this is the
// location to be used as the original data location.
static const string FLAT_LOCATION = "/data/temperature.csv";
static const string HDF_LOCATION("data/3B42.001003.5.HDF");
static const string& TEST_LOCATION = HDF_LOCATION;

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
    //string location = "/data/nc/fnoc1.nc" ;
    //  string location = TEST_LOCATION;
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

    dhi.container = ncml_container ;
    store->del_container( new_sym ) ;
    // once we have all this done we can return back to the calling
    // function below (build_das, build_dds, build_data). and do
    // whatever we need to do (add new attributes from the ncml file,
    // whatever.

    // if we are aggregating then the dhi needs to be updated with the
    // aggregation information.

    // ddx is built by building the dds object. The DDXResponseHandler
    // does the right thing to make it a DDX object.

    return true;
}


bool
NCMLRequestHandler::ncml_build_das( BESDataHandlerInterface &dhi )
{
    DDSLoader loader(dhi);
    string filename = dhi.container->access();
    NCMLParser parser(loader);
    BESDDSResponse* loaded_bdds = parser.parse(filename);

    // TODO if we really got a DDX, need to now conjure up a DAS response from it.
    if (!loaded_bdds)
      {
        throw BESInternalError("Null BESDDSResonse in ncml DAS handler.", __FILE__, __LINE__);
      }

    // Grab the loaded DDS to mutate.
    DDS* dds = loaded_bdds->get_dds();
    // Create an NCMLHelper based on the given filename we got loaded with in the dhi
    assert(dds);

    // Now fill in the desired DAS response object from the DDX

    BESResponseObject *response =
     dhi.response_handler->get_response_object();
    BESDASResponse *bdas = dynamic_cast < BESDASResponse * >(response);
    assert(bdas);

    DAS *das = bdas->get_das();

    // Copy the modified DDS attributes into the DAS response object!
    BESDEBUG("ncml", "Creating DAS response from the location DDX..." << endl);
    NCMLParser::populateDASFromDDS(das, *dds);

    // clean up!
    delete loaded_bdds;
    return false ;
}

bool
NCMLRequestHandler::ncml_build_dds( BESDataHandlerInterface &dhi )
{
    string filename = dhi.container->access();
    DDSLoader loader(dhi);
    NCMLParser parser(loader);
    BESDDSResponse* loaded_bdds = parser.parse(filename);

    // TODO if we really got a DDX, need to now conjure up a DAS response from it.
    if (!loaded_bdds)
      {
        throw BESInternalError("Null BESDDSResonse in ncml DAS handler.", __FILE__, __LINE__);
      }

    // Grab the loaded DDS to mutate.
    DDS* dds = loaded_bdds->get_dds();
    // Create an NCMLHelper based on the given filename we got loaded with in the dhi
    assert(dds);


    // Poke the response object with the loaded and modified one.
    BESResponseObject *response =
    dhi.response_handler->get_response_object();
    BESDDSResponse *bdds_out = dynamic_cast < BESDDSResponse * >(response);
    DDS *dds_out = bdds_out->get_dds();

    // If we just use assignment, we get into trouble with copied
    // pointers, bashing of the dataset name, etc etc so I specialize the copy for now.
    NCMLParser::copyVariablesAndAttributesInto(dds_out, *dds);

    // Also copy in the name of the original ncml request
    dds_out->filename(filename);
    dds_out->set_dataset_name(name_path(filename));
    // Is there anything else I need to shove in the DDS?

    // Might want to just set the DDS rather than the copy into the existing one (and delete existing)
    // but this seems fine for now.

    // Clean up the dynamic object I loaded
    delete loaded_bdds;
    return true;
}

// TODO For this special case we need to add a special call to the NCMLParser that just processes the <netcdf> element
// to load the dataset, then immediately returns it.
bool
NCMLRequestHandler::ncml_build_data( BESDataHandlerInterface &dhi )
{
  // The design doc says just load underlying data set and pass through,
  // which will be fine until we have the ability to add variables with ncml,
  // then this won't work.
  // For now, we'll just passthrough.

  // TODO implement this
  throw new BESInternalError("Unimplemented Method Called!", __FILE__, __LINE__);

    // Just redirect the dods call to the dataset at the new location.
  bool ret = ncml_build_redirect(dhi, TEST_LOCATION);

  /* BESResponseObject *response =
    dhi.response_handler->get_response_object();
  BESDataDDSResponse *bdds = dynamic_cast < BESDataDDSResponse * >(response);
  */

  // There's really nothing to do here since there's no metadata in dods now, right?
  // Once we can add new variables, THEN we need to fix this pathway.
  // TODO Handle new variables here for later ncml handler.

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

