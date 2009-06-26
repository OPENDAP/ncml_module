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
