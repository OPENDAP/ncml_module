// NCMLContainerStorage.cc

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of gateway_module, A C++ module that can be loaded in to
// the OPeNDAP Back-End Server (BES) and is able to handle remote requests.

// Copyright (c) 2002,2003 OPeNDAP, Inc.
// Author: Patrick West <pwest@ucar.edu>
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
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

// (c) COPYRIGHT URI/MIT 1994-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      pcw       Patrick West <pwest@ucar.edu>

#include <fstream>
#include <iostream>
#include <cerrno>

using std::ofstream ;
using std::ifstream ;
using std::cerr ;
using std::endl ;
using std::ios_base ;

#include <BESSyntaxUserError.h>
#include <BESInternalError.h>

#include "NCMLContainerStorage.h"
#include "NCMLResponseNames.h"

using namespace ncml_module;

string NCMLContainerStorage::NCML_RootDir ;

/** @brief create an instance of this persistent store with the given name.
 *
 * Creates an instances of NCMLContainerStorage with the given name.
 *
 * @param n name of this persistent store
 * @see NCMLContainer
 */
NCMLContainerStorage::NCMLContainerStorage( const string &n )
    : BESContainerStorageVolatile( n )
{
}

NCMLContainerStorage::~NCMLContainerStorage()
{
}

/** @brief looks for a container in this persistent store
 *
 * This method looks for a container with the given symbolic name. The
 * symbolic name is the id of the ncml document as specified within,
 * say, a THREDDS catalog. The ncml file name will be this symbolic
 * name.
 *
 * @param sym_name The symbolic name of the container to look for
 * @return If sym_name is found, the BESContainer instance representing
 * that symbolic name, else NULL is returned.
 */
BESContainer *
NCMLContainerStorage::look_for( const string &s_name )
{
    BESContainer *c = 0 ;

    // need to grab the NCML directory parameter
    string ncml_dir = NCMLContainerStorage::NCML_RootDir ;

    // the name of the file is that directory plys s_name.ncml
    string new_s_name ;
    if( s_name[0] == '/' )
	new_s_name = s_name.substr( 1 ) ;
    else
	new_s_name = s_name ;

    if( new_s_name.empty() )
    {
	string err = (string)"The container name can not be empty or /" ;
	throw BESSyntaxUserError( err, __FILE__, __LINE__ ) ;
    }

    string new_r_name = ncml_dir + "/" + new_s_name + ".ncml" ;
    string ncml_file = _root_dir + "/" + new_r_name ;

    // first let's see if the container is already loaded into this BES.
    // If not, then we need to find it on disk (see if it exists) and
    // build a container for it from that information. Otherwise, just
    // return 0. No error should be thrown if we can't find the
    // container.
    c = BESContainerStorageVolatile::look_for( new_s_name ) ;
    if( !c )
    {
	// doesn't exist, find it on disk
	{
	    ifstream istrm( ncml_file.c_str() ) ;
	    if( istrm )
	    {
		// yep, it's on disk, create a new container and add it
		// to the list, return the container.
		BESContainerStorageVolatile::add_container( new_s_name,
							    new_r_name,
						ModuleConstants::NCML_NAME ) ;
		c = BESContainerStorageVolatile::look_for( new_s_name ) ;
	    }
	}
    }

    return c ;
}

/** @brief adds a container with the provided information
 *
 * @param s_name symbolic name for the container, the name of the ncml
 * file
 * @param r_name the ncml document
 * @param type ignored. The type of the target response is determined by the
 * request response, or could be passed in
 */
void
NCMLContainerStorage::add_container( const string &s_name,
				     const string &r_name,
				     const string &type )
{
    // need to grab the NCML directory parameter
    string ncml_dir = NCMLContainerStorage::NCML_RootDir ;

    // the name of the file is that directory plys s_name.ncml
    string new_s_name ;
    if( s_name[0] == '/' )
	new_s_name = s_name.substr( 1 ) ;
    else
	new_s_name = s_name ;

    if( new_s_name.empty() )
    {
	string err = (string)"The container name can not be empty or /" ;
	throw BESSyntaxUserError( err, __FILE__, __LINE__ ) ;
    }

    string new_r_name = ncml_dir + "/" + new_s_name + ".ncml" ;
    string ncml_file = _root_dir + "/" + new_r_name ;

    // validation is done in the add_container call, so no need to do it
    // here.

    // if the file exists, then we overwrite it. Just truncate it.
    bool file_new = true ;
    {
	ifstream istrm( ncml_file.c_str() ) ;
	if( istrm )
	{
	    file_new = false ;
	    cerr << "it's not new" << endl ;
	}
	// istrm should go out of scope here and close
    }
    ofstream ostrm ;
    int my_errno = 0 ;
    if( file_new )
    {
	cerr << "opening new file " << ncml_file << endl ;
	ostrm.open( ncml_file.c_str(), ios_base::out ) ;
	my_errno = errno ;
    }
    else
    {
	ostrm.open( ncml_file.c_str(), ios_base::out|ios_base::trunc ) ;
	my_errno = errno ;
    }
    if( !ostrm )
    {
	string err = (string)"Unable to write out the ncml document "
		     + new_r_name ;
	if( my_errno )
	{
	    char *str = strerror( my_errno ) ;
	    if( str ) err += (string)" " + str ;
	}
	throw BESInternalError( err, __FILE__, __LINE__ ) ;
    }

    // write out <?xml version="1.0" encoding="UTF-8"?>
    ostrm << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl ;

    // then write out the r_name as the ncml document (no validation)
    ostrm << r_name << endl ;

    ostrm.close() ;

    // This should be all that we need to write out. Now, do I validate
    // the document? At least load it into xmlDoc and parse it to
    // validate that it's good xml?

    // in the define, need to use the ncml store and add .ncml to the id

    // now use new_s_name, the full new path to the file, and the type ncml
    if( file_new )
    {
	BESContainerStorageVolatile::add_container( new_s_name, new_r_name,
						ModuleConstants::NCML_NAME ) ;
    }
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance along with information about
 * each of the NCMLContainers already stored.
 *
 * @param strm C++ i/o stream to dump the information to
 */
void
NCMLContainerStorage::dump( ostream &strm ) const
{
    strm << BESIndent::LMarg << "NCMLContainerStorage::dump - ("
			     << (void *)this << ")" << endl ;
    BESIndent::Indent() ;
    BESContainerStorageVolatile::dump( strm ) ;
    BESIndent::UnIndent() ;
}

