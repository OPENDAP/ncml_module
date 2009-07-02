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

#include "SaxParserWrapper.h"

#include <exception>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <stdio.h> // for vsnprintf
#include <string>

#include "BESDebug.h"
#include "BESError.h"
#include "BESInternalError.h"
#include "BESInternalFatalError.h"
#include "BESSyntaxUserError.h"
#include "BESForbiddenError.h"
#include "BESNotFoundError.h"
#include "SaxParser.h"

using namespace std;
using namespace ncml_module;

////////////////////////////////////////////////////////////////////////////////
// Helpers

// convert to a string... Might want a way around this.
static string makeString(const xmlChar* chars)
{
  // TODO HACK!  This cast might be dangerous, but since DAP specifies Strings and URL's are US-ASCII, this
  // cast _should_ do the right thing.  xmlChar is an unsigned char now.
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
    attrs += 2;
    count++;
  }
  return count;
}

/////////////////////////////////////////////////////////////////////
// Callback we will register that just pass on to our C++ engine
//
// NOTE WELL: New C handlers need to follow the given
//  other examples in order to avoid memory leaks
//  in libxml during an exception!

// To avoid cut & paste below, we use this macro to cast the void* into the wrapper and
// set up a proper error handling structure around the main call.
// The macro internally defines the symbol "parser" to the SaxParser contained in the wrapper.
// So for example, a safe handler call to SaxParser would look like:
// static void ncmlStartDocument(void* userData)
//{
//  BEGIN_SAFE_HANDLER_CALL(userData); // pass in the void*, which is a SaxParserWrapper*
//  parser.onStartDocument(); // call the dispatch on the wrapped parser using the autodefined name parser
//  END_SAFE_HANDLER_CALL; // end the error handling wrapper
//}

#define BEGIN_SAFE_PARSER_BLOCK(argName) { \
  SaxParserWrapper* _spw_ = static_cast<SaxParserWrapper*>(argName); \
    if (_spw_->isExceptionState()) \
    { \
      return; \
    } \
  else \
    { \
      try \
      { \
        SaxParser& parser = _spw_->getParser();

// This is required after the end of the actual calls to the parser.
#define END_SAFE_PARSER_BLOCK } \
      catch (BESError& theErr) \
      { \
        BESDEBUG("ncml", "Caught BESError&, deferring..." << endl); \
        _spw_->deferException(theErr); \
      } \
      catch (std::exception& ex) \
      { \
        BESDEBUG("ncml", "Caught std::exception&, wrapping and deferring..." << endl); \
        BESInternalError _badness_("Wrapped std::exception.what()=" + string(ex.what()), __FILE__, __LINE__);\
        _spw_->deferException(_badness_); \
      } \
      catch (...)  \
      {   \
        BESDEBUG("ncml", "Caught unknown (...) exception: deferring default error." << endl); \
        BESInternalError _badness_("SaxParserWrapper:: Unknown Exception Type: ", __FILE__, __LINE__); \
        _spw_->deferException(_badness_);  \
      }  \
    } \
}

//////////////////////////////////////////////
// Our C SAX callbacks, wrapped carefully.

static void ncmlStartDocument(void* userData)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);

  parser.onStartDocument();

  END_SAFE_PARSER_BLOCK;
}

static void ncmlEndDocument(void* userData)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);

  parser.onEndDocument();

  END_SAFE_PARSER_BLOCK;
}

static void ncmlStartElement(void *  userData,
    const xmlChar * name,
    const xmlChar ** attrs)
{
  // BESDEBUG("ncml", "ncmlStartElement called for:<" << name << ">" << endl);
  BEGIN_SAFE_PARSER_BLOCK(userData);

  string nameS = makeString(name);
  SaxParser::AttrMap map;
  toAttrMap(map, attrs);

  // These args will be valid for the scope of the call.
  parser.onStartElement(nameS, map);

  END_SAFE_PARSER_BLOCK;
}

static void ncmlEndElement(void *  userData,
    const xmlChar * name)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);

  string nameS = makeString(name);
  parser.onEndElement(nameS);

  END_SAFE_PARSER_BLOCK;
}

static void ncmlCharacters(void* userData, const xmlChar* content, int len)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);

  // len is since the content string might not be null terminated,
  // so we have to build out own and pass it up special....
  // TODO consider just using these xmlChar's upstairs to avoid copies, or make an adapter or something.
  string characters("");
  characters.reserve(len);
  const xmlChar* contentEnd = content+len;
  while(content != contentEnd)
    {
      characters += (const char)(*content++);
    }

  parser.onCharacters(characters);

  END_SAFE_PARSER_BLOCK;
}

static void ncmlWarning(void* userData, const char* msg, ...)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);
  char buffer[1024];
  va_list(args);
  va_start(args, msg);
  unsigned int len = sizeof(buffer);
  vsnprintf(buffer, len, msg, args);
  va_end(args);
  parser.onParseWarning(string(buffer));
  END_SAFE_PARSER_BLOCK;
}

static void ncmlFatalError(void* userData, const char* msg, ...)
{
  BEGIN_SAFE_PARSER_BLOCK(userData);
  char buffer[1024];
  va_list(args);
  va_start(args, msg);
  unsigned int len = sizeof(buffer);
  vsnprintf(buffer, len, msg, args);
  va_end(args);
  parser.onParseError(string(buffer));
  END_SAFE_PARSER_BLOCK;
}

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
// class SaxParserWrapper impl

SaxParserWrapper::SaxParserWrapper(SaxParser& parser)
: _parser(parser)
, _handler() // inits to all nulls.
, _state(NOT_PARSING)
, _errorMsg("")
, _errorType(0)
, _errorFile("")
, _errorLine(-1)
{
  // register the static C functions.
  setupParser(_handler);
}

SaxParserWrapper::~SaxParserWrapper()
{
  // Really not much to do...  everything cleans itself up.
  _state = NOT_PARSING;
}

bool
SaxParserWrapper::parse(const string& ncmlFilename)
{
  bool success = true;

  // It's illegal to call this until it's done.
  if (_state == PARSING)
    {
      throw BESInternalError("Parse called again while already in parse.", __FILE__, __LINE__);
    }

  // OK, now we're parsing
  _state = PARSING;
  // Any BESError thrown in SaxParser callbacks will be deferred in here.
  int errNo = xmlSAXUserParseFile(&_handler, this, ncmlFilename.c_str());
  success = (errNo == 0);

  // If we deferred an exception during the libxml parse call, now's the time to rethrow it.
  if (isExceptionState())
    {
      rethrowException();
    }

  // Otherwise, we're also done parsing.
  _state = NOT_PARSING;
  return (errNo == 0); // success
}

void
SaxParserWrapper::deferException(BESError& theErr)
{
  _state = EXCEPTION;
  _errorType = theErr.get_error_type();
  _errorMsg = theErr.get_message();
  _errorLine = theErr.get_line();
  _errorFile = theErr.get_file();
}

// HACK admittedly a little gross, but it's weird to have to copy an exception
// and this seemed the safest way rather than making dynamic storage, etc.
void
SaxParserWrapper::rethrowException()
{
  // Clear our state out so we can parse again though.
  _state = NOT_PARSING;

  switch (_errorType)
  {
    case BES_INTERNAL_ERROR:
      throw BESInternalError(_errorMsg, _errorFile, _errorLine);
      break;

    case BES_INTERNAL_FATAL_ERROR:
      throw BESInternalFatalError(_errorMsg, _errorFile, _errorLine);
      break;

    case BES_SYNTAX_USER_ERROR:
      throw BESSyntaxUserError(_errorMsg, _errorFile, _errorLine);
      break;

    case BES_FORBIDDEN_ERROR:
      throw BESForbiddenError(_errorMsg, _errorFile, _errorLine);
      break;

    case BES_NOT_FOUND_ERROR:
      throw BESNotFoundError(_errorMsg, _errorFile, _errorLine);
      break;

    default:
      throw BESInternalError("Unknown exception type.", __FILE__, __LINE__);
      break;
  }
}

