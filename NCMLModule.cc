// NCMLModule.cc

#include <iostream>

using std::endl ;

#include "NCMLModule.h"
#include "BESRequestHandlerList.h"
#include "NCMLRequestHandler.h"
#include "BESDebug.h"
#include "BESResponseHandlerList.h"
#include "BESResponseNames.h"
#include "NCMLResponseNames.h"

using namespace ncml_module;

void
NCMLModule::initialize( const string &modname )
{
    BESDEBUG( modname, "Initializing NCML Module " << modname << endl );

    BESDEBUG( modname, "    adding " << modname << " request handler" << endl );
    BESRequestHandlerList::TheList()->add_handler( modname, new NCMLRequestHandler( modname ) ) ;

    // If new commands are needed, then let's declare this once here. If
    // not, then you can remove this line.
    string cmd_name ;

    // INIT_END
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

    // If new commands are needed, then let's declare this once here. If
    // not, then you can remove this line.
    string cmd_name ;

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

