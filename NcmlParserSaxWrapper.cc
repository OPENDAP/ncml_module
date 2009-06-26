/**

 */
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
