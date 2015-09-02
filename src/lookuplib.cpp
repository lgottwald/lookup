#if not defined USE_CUBIC_SPLINE and not defined USE_LINEAR_SPLINE
#define USE_CUBIC_SPLINE
#endif

#include <omp.h>
#include <memory>
#include <unordered_map>
#include <sdo/LookupTable.hpp>
#include <spline/SplineApproximation.hpp>
#include <spline/PiecewisePolynomial.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>

static std::vector< boost::shared_ptr< spline::BSplineCurve<3, double> > > splines;

#include <fstream>
#include <iostream>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include "md5_from_file.h"

struct indirect_equal
{
   template< typename T >
   bool operator()( T const &a, T const &b ) const
   {
      return *a == *b;
   }
};

template<typename T>
struct uniqueptr_hash : public boost::hash<T>
{

   using boost::hash<T>::operator();

   bool operator()( const std::unique_ptr<T> &x ) const
   {
      return ( *this )( *x );
   }

};

extern "C" {

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(_WIN32)
#define LKP_API __declspec(dllexport)
#define LKP_CALLCONV __stdcall
#else
#define LKP_API
#define LKP_CALLCONV
#endif

#define XCreate      xcreate
#define XFree        xfree
#define LibInit      libinit
#define Lookup       lookup
#define QueryLibrary querylibrary

#define rcOK     0
#define rcFUNC   1
#define rcGRAD   2
#define rcHESS   3
#define rcSYSTEM 4

#define ecOK       0
#define ecDOMAIN   1
#define ecSINGULAR 2
#define ecOVERFLOW 3
#define ecSIGLOSS  4

#define APIVER     1
#define LIBVER     1
#define CMPVER     1
#define MAXARGS    6

typedef void lkpRec_t;

typedef int ( LKP_CALLCONV *lkpLogError_t )( int retCode, int excCode, char *msg, void *usrmem );

LKP_API void LKP_CALLCONV XCreate( lkpRec_t **lkp )
{
}

LKP_API void LKP_CALLCONV XFree( lkpRec_t **lkp )
{
}

LKP_API int LKP_CALLCONV LibInit( lkpRec_t *lkp, const int version, char *msg )
{
   if( version < CMPVER )
   {
      sprintf( msg + 1, "Client is too old for this Library." );
      msg[0] = strlen( msg + 1 );
      return 1;
   }
   if( !splines.empty() )
      return 0;

   std::string lookupsmd5 = md5_from_file( "lookups.dat" );
   std::string splinesfile = "splines_";
   splinesfile += lookupsmd5.substr( 0, 10 );
   splinesfile += ".dat";
   {
      {
         std::ifstream ifs( splinesfile );
         if( ifs.good() )
         {
            boost::archive::binary_iarchive ia( ifs );
            ia >> splines;
            return 0;
         }
      }
      puts( "\n--- Fitting curves for lookups\n" );
      std::unordered_map <
      std::unique_ptr<sdo::LookupTable>,
            boost::shared_ptr< spline::BSplineCurve<3, double> > , uniqueptr_hash<sdo::LookupTable>, indirect_equal > lookups;
      double begin = omp_get_wtime();
      std::ifstream file( "lookups.dat" );

      double minkntdistance = 1e-7;
      double delta = 2;
      double eps = 1e-7;
      double max_rel_err = 0.01;
      bool allerrok = true;

      std::string lookupsLine;
      printf( "| %-6s | %-6s | %-11s | %-9s | %-10s | %-12s | %-19s | %-10s |\n",
               "lookup", "duplic", "err mixed", "err delta", "#intervals", "min knt dist", "min accept knt dist" , "obj tol" );
      while( std::getline( file, lookupsLine ) )
      {
         std::size_t pos = lookupsLine.find( '=' );
         if( pos != std::string::npos )
         {
            std::string setting = lookupsLine.substr( 0, pos );
            std::string valstring = lookupsLine.substr( pos + 1 );
            boost::trim( setting );
            boost::trim( valstring );
            double val;
            try
            {
               val = boost::lexical_cast<double>( valstring );
            }
            catch( const boost::bad_lexical_cast &e )
            {
               std::cerr << "warning: bad value '" << valstring << "' for option '" << setting << "' is ignored\n";
               std::cerr << "expected a real value\n";
               continue;
            }

            if( boost::iequals( setting, "max_mixed_err" ) )
            {
               max_rel_err = val;
            }
            else if( boost::iequals( setting, "min_knot_distance" ) )
            {
               minkntdistance = val;
            }
            else if( boost::iequals( setting, "mixed_err_delta" ) )
            {
               delta = val;
            }
            else if( boost::iequals( setting, "obj_tolerance" ) )
            {
               eps = val;
            }
            else
            {
               std::cerr << "warning: unknown option ignored '" << setting << "'\n";
               std::cerr << "supported options are: max_mixed_err, min_knot_distance, mixed_err_delta, obj_tolerance\n";
            }
            continue;
         }

         double x, y;
         std::unique_ptr<sdo::LookupTable> lookup( new sdo::LookupTable() );
         std::stringstream lineStream( lookupsLine );
         while( ( lineStream >> x ) && ( lineStream >> y ) )
         {
            lookup->addPoint( x, y );
         }
         spline::BSplineCurve<1, double> linear(
            lookup->getXvals(),
            lookup->getYvals()
         );
         double a = lookup->getXvals().front() - ( lookup->getXvals().back() - lookup->getXvals().front() ) / 2.;
         double b = lookup->getXvals().back() + ( lookup->getXvals().back() - lookup->getXvals().front() ) / 2.;
         boost::shared_ptr< spline::BSplineCurve<3, double> >  spline;
         auto lkpit = lookups.find( lookup );
         if( lkpit != lookups.end() )
         {
            spline = lkpit->second;
            std::size_t d;
            for( d = 0; d < splines.size(); ++d )
            {
               if( splines[d] == spline )
                  break;
            }
            printf( "|  %-5lu | %-6lu | %-11s | %-9s | %-10s | %-12s | %-19s | %-10s |\n",
                     splines.size(), d, "-----------", "---------", "----------", "------------", "-------------------", "----------" );
         }
         else
         {
            double rel_err = max_rel_err;
            spline = boost::make_shared< spline::BSplineCurve<3, double> >(
                        spline::ApproximatePiecewiseLinear(
                           linear,
                           a,
                           b,
                           rel_err,
                           delta,
                           eps,  //epsilon value
                           minkntdistance
                        )
                     );

            double m = ( b - a );

            for( std::size_t i = 0; i < spline->numIntervals(); ++i )
            {
               m = std::min( spline->getSupremum( i ) - spline->getInfimum( i ), m );
            }
            bool errok = rel_err < max_rel_err;
            allerrok &= errok;
            lookups.emplace( std::move( lookup ), spline );
            printf( "| %s%-5lu | %-6s | %-11.5e | %-9.2f | %-10lu | %-12.5e | %-19.5e | %-10.2e |\n",
                     errok ? " " : "*", splines.size() , "------", rel_err, delta, spline->numIntervals(), m, minkntdistance, eps );
         }
         splines.push_back( spline );
         fflush( stdout );
      }
      double end = omp_get_wtime();
      if( !allerrok )
         puts( "\n * spline exceeds maximum error within knot limit of 300" );
      printf( "\n--- Fitting took %f seconds", end - begin );
      fflush( stdout );
   }
   {
      std::ofstream ofs( splinesfile );
      boost::archive::binary_oarchive oa( ofs );
      oa << splines;
   }
   return 0;
}

LKP_API int LKP_CALLCONV Lookup( lkpRec_t *lkp,
                                 const int DR, /* in  Derivative request */
                                 const int args, /* in  number of arguments */
                                 const double x[], /* in  the arguments */
                                 double *f, /* out function value */
                                 double g[], /* out gradient */
                                 double h[], /* out rolled out hessian */
                                 lkpLogError_t cb , /* in  error callback */
                                 void *usermem ) /* in  user memory for error callback */
{
   /* F, G, H were set to 0 before we call this function */
   char msg[257];

   if( args < 2 || args > 2 )
   {
      sprintf( msg + 1, "Lookup: two arguments expected. Called with %d", args );
      msg[0] = strlen( msg + 1 );
      return cb( rcFUNC, ecDOMAIN, msg, usermem );
   }
   unsigned i = static_cast<unsigned>( x[1] );
   auto interval = splines[i]->findInterval( x[0] );
   switch( DR )
   {
   default:
   case 2:
      h[0] = splines[i]->template evaluate<2>( x[0], interval );
   case 1:
      g[0] = splines[i]->template evaluate<1>( x[0], interval );
   case 0:
      *f = splines[i]->template evaluate<0>( x[0], interval );
   }
   return rcOK;
}


#include "lookuplibql.h"

}
