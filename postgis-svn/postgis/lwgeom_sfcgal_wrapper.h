#ifndef LWGEOM_SFCGAL_WRAPPER_H
#define LWGEOM_SFCGAL_WRAPPER_H

#include "lwgeom_sfcgal.h"
#include <boost/preprocessor.hpp>

/**
 *
 * Set of preprocessing macros used to declare PostGIS functions that call SFCGAL methods
 *
 * Each argument type must have the associated macros declared.
 * Please refer to the definition of WRAPPER_DECLARE_SFCGAL_FUNCTION if you want to add a new type.
 */
// Type index, unique for each type
#define WRAPPER_TYPE_Geometry 0
// How to extract a ith argument of type Geometry
#define WRAPPER_INPUT_Geometry( i )					\
	GSERIALIZED* BOOST_PP_CAT(input, i);				\
	BOOST_PP_CAT(input,i) = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(i)); \
	std::auto_ptr<SFCGAL::Geometry> BOOST_PP_CAT(geom,i);		\
	try								\
	{								\
		BOOST_PP_CAT( geom, i ) = POSTGIS2SFCGAL( BOOST_PP_CAT( input, i ) ); \
	}								\
	catch ( std::exception& e ) {					\
		lwerror( "Argument geometry could not be converted to SFCGAL: %s", e.what() ); \
		PG_RETURN_NULL();					\
	}
// Use dereference for std::auto_ptr<Geometry>
#define WRAPPER_ACCESS_INPUT_Geometry( i )  \
	* BOOST_PP_CAT( geom, i )

#define WRAPPER_CONVERT_RESULT_Geometry()					\
	GSERIALIZED* gresult;						\
	if ( result.get() ) {						\
		try {							\
			gresult = SFCGAL2POSTGIS( *result, result->is3D(), gserialized_get_srid(input0) ); \
		}							\
		catch ( std::exception& e ) {				\
			lwerror("Result geometry could not be converted to lwgeom: %s", e.what() ); \
			PG_RETURN_NULL();				\
		}							\
	}

#define WRAPPER_RETURN_Geometry() \
	PG_RETURN_POINTER( gresult )

#define WRAPPER_TO_CSTR_Geometry( i )				\
	"%s", BOOST_PP_CAT( geom, i )->asText().c_str()

#define WRAPPER_FREE_INPUT_Geometry( i )		\
	PG_FREE_IF_COPY( BOOST_PP_CAT( input, i ), i )

#define WRAPPER_DECLARE_RETURN_VAR_Geometry() \
	std::auto_ptr<SFCGAL::Geometry> result

/**
 * ref geometry argument wrapper
 */
#define WRAPPER_TYPE_refGeometry 1
#define WRAPPER_INPUT_refGeometry( i ) \
	SFCGAL::Geometry* BOOST_PP_CAT( input, i ) = get_geometry_arg( fcinfo, i );

#define WRAPPER_ACCESS_INPUT_refGeometry( i )  \
	* BOOST_PP_CAT( input, i )

#define WRAPPER_FREE_INPUT_refGeometry( i )  /* */
#define WRAPPER_CONVERT_RESULT_refGeometry()   /* */
#define WRAPPER_RETURN_refGeometry() \
	SFCGAL::Geometry* geo = result.release();		\
	GeometryPool::reference( get_reference_context(), geo );	\
	return return_geometry( geo );
#define WRAPPER_TO_CSTR_refGeometry( i )   			\
	"%s", BOOST_PP_CAT( input, i ) ->asText().c_str()
#define WRAPPER_DECLARE_RETURN_VAR_refGeometry() \
	std::auto_ptr<SFCGAL::Geometry> result

/**
 * exact geometry argument wrapper
 */
#define WRAPPER_TYPE_exactGeometry 2
#define WRAPPER_INPUT_exactGeometry( i ) \
	std::auto_ptr<SFCGAL::Geometry> BOOST_PP_CAT( input, i )  = unserializeExactGeometry( (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM(i)) );

#define WRAPPER_ACCESS_INPUT_exactGeometry( i )  \
	* BOOST_PP_CAT( input, i )

#define WRAPPER_FREE_INPUT_exactGeometry( i )  /* */
#define WRAPPER_CONVERT_RESULT_exactGeometry()   /* */
#define WRAPPER_DECLARE_RETURN_VAR_exactGeometry() \
	std::auto_ptr<SFCGAL::Geometry> result
#define WRAPPER_RETURN_exactGeometry() \
	PG_RETURN_POINTER( serializeExactGeometry( *result ) );

#define WRAPPER_TO_CSTR_exactGeometry( i )   			\
	"%s", BOOST_PP_CAT( input, i ) ->asText().c_str()

/**
 * bool argument wrapper
 */
#define WRAPPER_TYPE_bool 3
#define WRAPPER_CONVERT_RESULT_bool()
#define WRAPPER_RETURN_bool()			\
	PG_RETURN_BOOL( result )
#define WRAPPER_TO_CSTR_bool( i )		\
	"%d", BOOST_PP_CAT( input, i ) ? 1 : 0
#define WRAPPER_FREE_INPUT_bool( i )
#define WRAPPER_DECLARE_RETURN_VAR_bool() \
	bool result

/**
 * bool argument wrapper
 */
#define WRAPPER_TYPE_double 4
#define WRAPPER_INPUT_double( i )				\
	double BOOST_PP_CAT(input, i) = PG_GETARG_FLOAT8(i);
#define WRAPPER_ACCESS_INPUT_double( i )  \
	BOOST_PP_CAT( input, i )
#define WRAPPER_TO_CSTR_double( i )		\
	"%g", BOOST_PP_CAT( input, i )
#define WRAPPER_FREE_INPUT_double( i )
#define WRAPPER_CONVERT_RESULT_double()
#define WRAPPER_RETURN_double()			\
	PG_RETURN_FLOAT8( result )
#define WRAPPER_DECLARE_RETURN_VAR_double() \
	double result

#define _WR_TYPE_IDX( name ) \
	BOOST_PP_CAT( WRAPPER_TYPE_, name )
// convert an argument to a variable
// adds * if it's a Geometry
#define _WR_DEREFERENCE_ARG( type, i )		\
	BOOST_PP_CAT( WRAPPER_ACCESS_INPUT_, type)( i )
#define _WR_INSERT_COMMA_IF_NEEDED( i, msize )				\
	BOOST_PP_COMMA_IF( BOOST_PP_LESS( i, BOOST_PP_DEC(msize) ) )
#define _WR_DEREFERENCE_ARG_M( r, seqsize, i, elem )			\
	_WR_DEREFERENCE_ARG( elem, i ) _WR_INSERT_COMMA_IF_NEEDED( i, seqsize )
#define _WR_DEREFERENCE_LIST( types )					\
	BOOST_PP_SEQ_FOR_EACH_I( _WR_DEREFERENCE_ARG_M, BOOST_PP_SEQ_SIZE(types), types )

#define WRAPPER_DECLARE_INPUT_PARAM( r, data, i, elem ) \
	BOOST_PP_CAT( WRAPPER_INPUT_, elem )( i )

#define WRAPPER_TO_CSTR( tname, i )			\
	BOOST_PP_CAT( WRAPPER_TO_CSTR_, tname )( i )

#define WRAPPER_DECLARE_RETURN_VAR( return_type )			\
	BOOST_PP_CAT( WRAPPER_DECLARE_RETURN_VAR_, return_type)()

#define _WR_LWNOTICE( r, data, i, tname )				\
	lwnotice( BOOST_PP_STRINGIZE( BOOST_PP_CAT( input, i ) ) ": " WRAPPER_TO_CSTR( tname, i ) );
#define WRAPPER_NOTICES( types )				\
	BOOST_PP_SEQ_FOR_EACH_I( _WR_LWNOTICE, _, types )

#define _WR_FREE_INPUT( r, data, i, tname ) \
	BOOST_PP_CAT( WRAPPER_FREE_INPUT_, tname )( i );
#define WRAPPER_FREE_INPUTS( types )				\
	BOOST_PP_SEQ_FOR_EACH_I( _WR_FREE_INPUT, _, types )

/**
 * This is the heart of the wrapping function
 *
 * fname: name of the C function exposed as a PostGIS API
 * function: SFCGAL function
 * return_type: return type of the SFCGAL function
 * types: BOOST_PP sequence of argument types
 *
 * Example : WRAPPER_DECLARE_SFCGAL_FUNCTION( intersects, SFCGAL::algorithm::intersects, bool, (Geometry)(Geometry) )
 */
#define WRAPPER_DECLARE_SFCGAL_FUNCTION( fname, function, return_type, types ) \
	extern "C" { PG_FUNCTION_INFO_V1( BOOST_PP_CAT( sfcgal_, fname ) ); } \
	extern "C" Datum BOOST_PP_CAT( sfcgal_, fname )( PG_FUNCTION_ARGS ) \
	{								\
		BOOST_PP_SEQ_FOR_EACH_I( WRAPPER_DECLARE_INPUT_PARAM, _, types ); \
		WRAPPER_DECLARE_RETURN_VAR( return_type );		\
		try {							\
			result = function( _WR_DEREFERENCE_LIST( types ) ); \
		}							\
		catch ( std::exception& e )				\
		{							\
			WRAPPER_NOTICES( types );			\
			lwnotice( e.what() );				\
			lwerror( "Error during execution of sfcgal_" BOOST_PP_STRINGIZE(fname) ); \
			PG_RETURN_NULL();				\
		}							\
		BOOST_PP_CAT( WRAPPER_CONVERT_RESULT_, return_type )();	\
		WRAPPER_FREE_INPUTS( types );				\
		BOOST_PP_CAT( WRAPPER_RETURN_, return_type )();		\
	}

/**
 * Conversion from a GSERIALIZED to a SFCGAL::Geometry
 */
std::auto_ptr<SFCGAL::Geometry> POSTGIS2SFCGAL(GSERIALIZED *pglwgeom);

/**
 * Conversion from a GSERIALIZED to a SFCGAL::PreparedGeometry
 */
std::auto_ptr<SFCGAL::PreparedGeometry> POSTGIS2SFCGALp(GSERIALIZED *pglwgeom);

/**
 * Conversion from a SFCGAL::Geometry to a GSERIALIZED
 */
GSERIALIZED* SFCGAL2POSTGIS(const SFCGAL::Geometry& geom, bool force3D, int SRID );

/**
 * Wrappers around SFCGAL, when direct call is not possible
 */
bool _sfcgal_hasplane( const SFCGAL::Geometry& g );

bool _sfcgal_pointing_up( const SFCGAL::Geometry& g );

std::auto_ptr<SFCGAL::Geometry> _sfcgal_triangulate( const SFCGAL::Geometry& g );

std::auto_ptr<SFCGAL::Geometry> _sfcgal_triangulate2D( const SFCGAL::Geometry& g );

std::auto_ptr<SFCGAL::Geometry> _sfcgal_extrude( SFCGAL::Geometry& g, double dx, double dy, double dz );

std::auto_ptr<SFCGAL::Geometry> _sfcgal_make_solid( const SFCGAL::Geometry& g );

std::auto_ptr<SFCGAL::Geometry> _sfcgal_force_z_up( SFCGAL::Geometry& g );

#endif
