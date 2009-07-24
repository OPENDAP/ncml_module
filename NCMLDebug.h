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

#ifndef __NCML_MODULE__NCML_DEBUG__
#define __NCML_MODULE__NCML_DEBUG__

#include <string>
#include "BESDebug.h"
#include "BESInternalError.h"
#include "BESSyntaxUserError.h"
#include "BESNotFoundError.h"

/*
 * Some basic macros to reduce code clutter, cut & pasting, and to greatly improve readability.
 * I would have made them functions somewhere, but the __FILE__ and __LINE__ are useful.
 * We can also specialize these based on debug vs release builds etc. or new error types later as well.
 * */

// Where my BESDEBUG output goes
#define NCML_MODULE_DBG_CHANNEL "ncml"

// Spew the std::string msg to debug channel then throw BESInternalError.  for those errors that are internal problems, not user/parse errors.
#define THROW_NCML_INTERNAL_ERROR(msg) { BESDEBUG(NCML_MODULE_DBG_CHANNEL, std::string("NCMLModule InternalError: ") << (msg) << endl); \
                                   throw BESInternalError("NCMLModule InternalError: " + std::string(msg) , \
                                                          __FILE__, __LINE__); }

// Spew the std::string msg to debug channel then throw a BESSyntaxUserError.  For parse and syntax errors in the NCML.
#define THROW_NCML_PARSE_ERROR(msg) { BESDEBUG(NCML_MODULE_DBG_CHANNEL, "NCMLModule ParseError: " << (msg) << endl); \
                                   throw BESSyntaxUserError(std::string("NCMLModule ParseError: ") + std::string(msg), __FILE__, __LINE__); }

// My own assert to throw an internal error instead of assert() which calls abort(), which is not so nice to do on a server.
#define NCML_ASSERT(cond)   { if (!(cond)) { THROW_NCML_INTERNAL_ERROR(std::string("ASSERTION FAILED: ") + std::string(#cond)); } }

// An assert that can carry a std::string msg
#define NCML_ASSERT_MSG(cond, msg)  { if (!(cond)) { \
  BESDEBUG(NCML_MODULE_DBG_CHANNEL, (msg) << endl); \
  THROW_NCML_INTERNAL_ERROR(std::string("ASSERTION FAILED: condition=( ") + std::string(#cond) + std::string(" ) ") + std::string(msg)); } }

// Check pointers before dereferencing them.
#define VALID_PTR(ptr) NCML_ASSERT_MSG((ptr), std::string("Null pointer."));

#endif // __NCML_MODULE__NCML_DEBUG__
