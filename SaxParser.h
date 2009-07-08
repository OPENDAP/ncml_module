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
#ifndef __NCML_MODULE__SAX_PARSER_H__
#define __NCML_MODULE__SAX_PARSER_H__

#include <string>
#include <map>

namespace ncml_module
{
  /**
   * @brief Interface class for the wrapper between libxml C SAX parser and our NCMLParser.
   *
   * Also contains definition for AttrMap, which is how the attrs will be returned to the parser.
   * The user should also be careful about making copies of any returned const reference objects (string or AttrMap) as
   * they are only valid in memory for the scope of the handler calls!
   *
   * @author mjohnson <m.johnson@opendap.org>
   */
class SaxParser
{
protected:
  SaxParser(); // Interface class

public:
  /** Attributes will be shoved into a map for calls below. */
  typedef std::map<std::string, std::string> AttrMap;

  /** @brief Return the value of the given attribute from the map, else the given default.
   * @param map  map to search
   * @param name name of the attribute to find
   * @param default  what to return if name not found
   * @return the attribute value or default as a const reference.
   */
  static const std::string& findAttrValue(const SaxParser::AttrMap& map, const std::string& name, const std::string& def="");

  virtual ~SaxParser() {};

  virtual void onStartDocument() = 0;
  virtual void onEndDocument() = 0;

  /** Called at the start of the element with the given name and attribute dictionary
    *  The args are only valid for the duration of the call, so copy if necessary to keep.
    * @param name name of the element
    * @param attrs a map of any attributes -> values.  Volatile for this call.
    * */
  virtual void onStartElement(const std::string& name, const AttrMap& attrs) = 0;

  /** Called at the end of the element with the given name.
    *  The args are only valid for the duration of the call, so copy if necessary to keep.
    * */
  virtual void onEndElement(const std::string& name) = 0;

  /** Called when characters are encountered within an element.
   * content is only valid for the call duration.
   * Note: this will return all whitespace in the document as well, which makes it messy to use.
   */
  virtual void onCharacters(const std::string& content) = 0;

  /** A recoverable parse error occured. */
  virtual void onParseWarning(std::string msg) = 0;

  /** An unrecoverable parse error occurred */
  virtual void onParseError(std::string msg) = 0;

}; // class SaxParser

} // namespace ncml_module

#endif /* __NCML_MODULE__SAX_PARSER_H__ */
