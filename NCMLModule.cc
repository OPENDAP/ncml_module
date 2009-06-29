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

