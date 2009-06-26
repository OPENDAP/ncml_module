// NCMLRequestHandler.h

#ifndef I_NCMLRequestHandler_H
#define I_NCMLRequestHandler_H

#include "BESRequestHandler.h"
#include "BESDDSResponse.h"

namespace ncml_module
{
/**
 * Handler for AIS Using NCML
 */
class NCMLRequestHandler : public BESRequestHandler
{
private: // rep

private:

    // example for loading other locations by hijacking the dhi of the ncml request.
    static bool		ncml_build_redirect( BESDataHandlerInterface &dhi, const string& location ) ;

public:
			NCMLRequestHandler( const string &name ) ;
    virtual		~NCMLRequestHandler( void ) ;

    virtual void	dump( ostream &strm ) const ;

    static bool		ncml_build_das( BESDataHandlerInterface &dhi ) ;
    static bool		ncml_build_dds( BESDataHandlerInterface &dhi ) ;
    static bool		ncml_build_data( BESDataHandlerInterface &dhi ) ;
    static bool		ncml_build_vers( BESDataHandlerInterface &dhi ) ;
    static bool		ncml_build_help( BESDataHandlerInterface &dhi ) ;
}; // class NCMLRequestHandler
} // namespace ncml_module

#endif // NCMLRequestHandler.h

