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

#ifndef NCMLPARSERSAXWRAPPER_H_
#define NCMLPARSERSAXWRAPPER_H_

#include <string>
#include <libxml/parser.h>

using namespace std;

namespace ncml_module
{
  class SaxParser;
}

namespace ncml_module
{
/**
 * @brief Wrapper for libxml SAX parser C callbacks.
 *
 * On a parse call, the filename is parsed using the libxml SAX parser
 * and the calls are passed into our C++ parser via the SaxParser interface class.
 *
 * TODO BUG What happens if the SaxParser calls throw an exception?  Do we need to
 * clean the memory of the parser?  This needs to be looked into.
 *
 * TODO BUG The onParseWarning and onParseError do not get the error message back.
 * We need to use glib to generate a std::string I think...
 *
 * @author mjohnson <m.johnson@opendap.org>
 */
class NcmlParserSaxWrapper
{
private:
  // Struct with all the callback functions in it used by parse.
  // We add them in the constructor.  They are all static functions
  // in the impl file since they're callbacks from C.
  xmlSAXHandler _handler;

public:
  NcmlParserSaxWrapper();
  virtual ~NcmlParserSaxWrapper();

  /** @brief Do a SAX parse of the ncmlFilename
   * and pass the calls to the engine.
   *
   * @throws Can throw if the engine handlers throw.
   *
   * @return successful parse
   */
  bool parse(const string& ncmlFilename, ncml_module::SaxParser& engine);
}; // class NcmlParserSaxWrapper

} // namespace ncml_module

#endif /* NCMLPARSERSAXWRAPPER_H_ */
