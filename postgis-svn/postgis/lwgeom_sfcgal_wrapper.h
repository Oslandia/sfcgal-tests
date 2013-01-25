#ifndef LWGEOM_SFCGAL_WRAPPER_H
#define LWGEOM_SFCGAL_WRAPPER_H

#include "lwgeom_sfcgal.h"

#include <boost/preprocessor.hpp>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/PreparedGeometry.h>

/**
 *
 * Set of preprocessing macros used to declare PostGIS functions that call SFCGAL methods
 *
 * Each argument type must have the associated macros declared.
 * Please refer to the definition of SFCGAL_WRAPPER_DECLARE_FUNCTION if you want to add a new type.
 */

/**
 * generic bool argument wrapper
 */
#define SFCGAL_TYPE_bool_WRAPPER_INPUT( i )                  /* not used */
#define SFCGAL_TYPE_bool_WRAPPER_ACCESS_INPUT( i )           /* not used */
#define SFCGAL_TYPE_bool_WRAPPER_TO_CSTR( i )                "%d", BOOST_PP_CAT( input, i ) ? 1 : 0
#define SFCGAL_TYPE_bool_WRAPPER_DECLARE_RETURN_VAR()        bool result
#define SFCGAL_TYPE_bool_WRAPPER_CONVERT_RESULT()            /**/
#define SFCGAL_TYPE_bool_WRAPPER_FREE_INPUT( i )             /**/
#define SFCGAL_TYPE_bool_WRAPPER_RETURN()                    PG_RETURN_BOOL( result )

/**
 * generic double argument wrapper
 */
#define SFCGAL_TYPE_double_WRAPPER_INPUT( i )                double BOOST_PP_CAT(input, i) = PG_GETARG_FLOAT8(i);
#define SFCGAL_TYPE_double_WRAPPER_ACCESS_INPUT( i )         BOOST_PP_CAT( input, i )
#define SFCGAL_TYPE_double_WRAPPER_TO_CSTR( i )              "%g", BOOST_PP_CAT( input, i )
#define SFCGAL_TYPE_double_WRAPPER_DECLARE_RETURN_VAR()      double result
#define SFCGAL_TYPE_double_WRAPPER_CONVERT_RESULT()          /**/
#define SFCGAL_TYPE_double_WRAPPER_FREE_INPUT( i )           /**/
#define SFCGAL_TYPE_double_WRAPPER_RETURN()                  PG_RETURN_FLOAT8( result )

/**
 *  auxiliary macros
 */

// Returns SFCGAL_TYPE_type_WRAPPERname
// name must start with '_'
#define _SFCGAL_WR_type_expansion( type, name )              BOOST_PP_CAT( BOOST_PP_CAT( BOOST_PP_CAT( SFCGAL_TYPE_, type ), _WRAPPER), name )

#define _SFCGAL_WR_dereference_arg( type, i )	             _SFCGAL_WR_type_expansion( type, _ACCESS_INPUT )( i )
#define _SFCGAL_WR_insert_comma( i, msize )                  BOOST_PP_COMMA_IF( BOOST_PP_LESS( i, BOOST_PP_DEC(msize) ) )

#define _SFCGAL_WR_dereference_arg_m( r, seqsize, i, elem )  _SFCGAL_WR_dereference_arg( elem, i ) _SFCGAL_WR_insert_comma( i, seqsize )
// build a call list of input arguments
#define _SFCGAL_WR_dereference_list( types )	             BOOST_PP_SEQ_FOR_EACH_I( _SFCGAL_WR_dereference_arg_m, BOOST_PP_SEQ_SIZE(types), types )

#define _SFCGAL_WR_declare_input_param( r, data, i, type )   _SFCGAL_WR_type_expansion( type, _INPUT )( i )
#define _SFCGAL_WR_to_c_str( type, i )                       _SFCGAL_WR_type_expansion( type, _TO_CSTR)( i )
#define _SFCGAL_WR_declare_return_var( type )                _SFCGAL_WR_type_expansion( type, _DECLARE_RETURN_VAR)()
#define _SFCGAL_WR_lwnotice( r, data, i, type )              lwnotice( BOOST_PP_STRINGIZE( BOOST_PP_CAT( input, i ) ) ": " _SFCGAL_WR_to_c_str( type, i ) );
#define _SFCGAL_WR_notices( types )                          BOOST_PP_SEQ_FOR_EACH_I( _SFCGAL_WR_lwnotice, _, types )

#define _SFCGAL_WR_free_input( r, data, i, type )            _SFCGAL_WR_type_expansion( type, _FREE_INPUT)( i );
#define _SFCGAL_WR_free_inputs( types )                      BOOST_PP_SEQ_FOR_EACH_I( _SFCGAL_WR_free_input, _, types )

/**
 * This is the core of the substitution macro that is used to declare wrapping functions.
 *
 * fname: name of the C function that will be exposed as part of the PostGIS API (prefixed with 'sfcgal_')
 * function: SFCGAL C++ function
 * return_type: return type of the SFCGAL function
 * types: BOOST_PP sequence of argument types
 *
 * Example : SFCGAL_WRAPPER_DECLARE_FUNCTION( intersects, SFCGAL::algorithm::intersects, bool, (Geometry)(Geometry) )
 *
 * This will declare a function sfcgal_intersects that takes two geometries in argument and returns a bool.
 * Type names refer to the name used in SFCGAL_TYPE_xxx_WRAPPER_yyy() macros
 */
#define SFCGAL_WRAPPER_DECLARE_FUNCTION( fname, function, return_type, types ) \
	extern "C" { PG_FUNCTION_INFO_V1( BOOST_PP_CAT( sfcgal_, fname ) ); } \
	extern "C" Datum BOOST_PP_CAT( sfcgal_, fname )( PG_FUNCTION_ARGS ) \
	{								\
		BOOST_PP_SEQ_FOR_EACH_I( _SFCGAL_WR_declare_input_param, _, types ); \
		_SFCGAL_WR_declare_return_var( return_type );		\
		try {							\
			result = function( _SFCGAL_WR_dereference_list( types ) ); \
		}							\
		catch ( std::exception& e )				\
		{							\
			_SFCGAL_WR_notices( types );			\
			lwnotice( e.what() );				\
			lwerror( "Error during execution of sfcgal_" BOOST_PP_STRINGIZE(fname) ); \
			PG_RETURN_NULL();				\
		}							\
		_SFCGAL_WR_type_expansion( return_type, _CONVERT_RESULT )(); \
		_SFCGAL_WR_free_inputs( types );			\
		_SFCGAL_WR_type_expansion( return_type, _RETURN )();	\
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
 * Conversion from a SFCGAL::PreparedGeometry to a GSERIALIZED
 */
GSERIALIZED* SFCGAL2POSTGIS( const SFCGAL::PreparedGeometry& geom, bool force3D );

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

//
//
// copy input to output, used for serialization tests
std::auto_ptr<SFCGAL::Geometry> _sfcgal_copy( SFCGAL::Geometry& g );

#endif
