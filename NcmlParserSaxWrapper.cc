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

#include <string>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <iostream>

#include "BESDebug.h"
#include "NcmlParserSaxWrapper.h"
#include "SaxParser.h"


using namespace std;
using namespace ncml_module;

////////////////////////////////////////////////////////////////////////////////
// Helpers

// convert to a string... Might want a way around this.
static string makeString(const xmlChar* chars)
{
  // TODO HACK!  This cast might be dangerous, but I think we can assume non UTF-8 data...
  // xmlChar is an unsigned char now.  I think we need Glib to do this right...
  return string( (const char*)chars );
}

// Empty then fill the map with <attribName.attribValue> pairs from attrs.
static int toAttrMap(SaxParser::AttrMap& map, const xmlChar** attrs)
{
  map.clear();
  int count=0;
  while (attrs && *attrs != NULL)
  {
    map.insert(make_pair<string,string>(makeString(*attrs), makeString(*(attrs+1))));
    attrs += 2; // stride 2
    count++;
  }
  return count;
}

/////////////////////////////////////////////////////////////////////
// Callback we will register that just pass on to our C++ engine

static void ncmlStartDocument(void* userData)
{
  SaxParser* p = static_cast<SaxParser*>(userData);
  p->onStartDocument();
}

static void ncmlEndDocument(void* userData)
{
   SaxParser* p = static_cast<SaxParser*>(userData);
   p->onEndDocument();
}

static void ncmlStartElement(void *  userData,
    const xmlChar * name,
    const xmlChar ** attrs)
{
  // BESDEBUG("ncml", "ncmlStartElement called for:<" << name << ">" << endl);
  SaxParser* p = static_cast<SaxParser*>(userData);


  string nameS = makeString(name);
  SaxParser::AttrMap map;
  toAttrMap(map, attrs);

  // These args will be valid for the scope of the call.
  p->onStartElement(nameS, map);
}

static void ncmlEndElement(void *  userData,
    const xmlChar * name)
{
  SaxParser* p = static_cast<SaxParser*>(userData);
  string nameS = makeString(name);
  p->onEndElement(nameS);
}

static void ncmlCharacters(void* userData, const xmlChar* content, int len)
{
  // len is since the content string might not be null terminated,
  // so we have to build out own and pass it up special....
  // TODO consider just using these xmlChar's upstairs to avoid copies, or make an adapter or something.
  string characters("");
  characters.reserve(len);
  // end iterator
  const xmlChar* contentEnd = content+len;
  while(content != contentEnd)
    {
      characters += (const char)(*content++);
    }
  SaxParser* p = static_cast<SaxParser*>(userData);
  p->onCharacters(characters);
}

static void ncmlWarning(void* userData, const char* msg, ...)
{
  // TODO This seems to pass down some UTF varargs so we need GLib to generate the msg or something
  SaxParser* p = static_cast<SaxParser*>(userData);
  p->onParseWarning(msg);
}

static void ncmlFatalError(void* userData, const char* msg, ...)
{
  // TODO This seems to pass down some UTF varargs so we need GLib to generate the msg or something
  SaxParser* p = static_cast<SaxParser*>(userData);
  p->onParseError(msg);
}

// Register some handlers
static void setupParser(xmlSAXHandler& handler)
{
  handler.startDocument = ncmlStartDocument;
  handler.endDocument = ncmlEndDocument;
  handler.startElement = ncmlStartElement;
  handler.endElement = ncmlEndElement;
  handler.warning = ncmlWarning;
  handler.error = ncmlFatalError;
  handler.fatalError = ncmlFatalError;
  handler.characters = ncmlCharacters;
}

////////////////////////////////////////////////////////////////////////////////

NcmlParserSaxWrapper::NcmlParserSaxWrapper()
: _handler()
{
  setupParser(_handler);
}

NcmlParserSaxWrapper::~NcmlParserSaxWrapper()
{

}

bool
NcmlParserSaxWrapper::parse(const string& ncmlFilename, ncml_module::SaxParser& engine)
{
  return (xmlSAXUserParseFile(&_handler, &engine, ncmlFilename.c_str()) >= 0);
}
