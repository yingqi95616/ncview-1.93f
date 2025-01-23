/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993 through 2008 David W. Pierce
 *
 * This program  is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * David W. Pierce
 * 6259 Caminito Carrean
 * San Diego, CA   92122
 * pierce@cirrus.ucsd.edu
 */

/*
	This provides an interface to the udunits package
	(http://www.unidata.ucar.edu/packages/udunits/index.html)
	that can be bypassed if desired.
*/

#include "ncview.includes.h"
#include "ncview.defines.h"
#include "ncview.protos.h"
#include "math.h"

typedef struct {
	char	*name;
	void	*next;
} UniqList;

static int	valid_udunits_pkg;
static int	is_unique( char *units_name );
static UniqList *uniq = NULL;


/******************************************************************************/
#ifdef INC_UDUNITS

#include "udunits.h"
#include "utCalendar_cal.h"
#include "utCalendar_cal.c"

/******************************************************************************/
void udu_utinit( char *path )
{
	int	err;

	err = utInit( path );
	if( err == 0 )
		valid_udunits_pkg = 1;
	else
		{
		valid_udunits_pkg = 0;
		switch (err) {
			case UT_ENOFILE:
				fprintf( stderr, "Note: Udunits units file not found; no units conversion will be attmpted.\n" );
				fprintf( stderr, "(put path of units file in environmental variable UDUNITS_PATH)\n");
				break;

			case UT_ESYNTAX:
				fprintf( stderr, "Note: syntax error in udunits file; no units conversion will be attmpted.\n" );
				break;

			case UT_EUNKNOWN:
				fprintf( stderr, "Note: unknown specification in udunits file; no units conversion will be attmpted.\n" );
				break;

			case UT_EIO:
				fprintf( stderr, "Note: I/O error while reading udunits file; no units conversion will be attmpted.\n" );
				break;

			case UT_EALLOC:
				fprintf( stderr, "Note: Allocation error while reading udunits file; no units conversion will be attmpted.\n" );
				break;

			}
		}
}

/******************************************************************************/
int udu_utistime( char *dimname, char *units )
{
	utUnit	unit;

	if( units == NULL )
		return(0);

	if( valid_udunits_pkg ) {
		if( utScan( units, &unit ) != 0 ) {
			/* can't parse unit spec */
			if( is_unique(units) ) 
				fprintf( stderr, "Note: udunits: unknown units for %s: \"%s\"\n",
					dimname, units );
			return( 0 );
			}
		else
			{
			return( utHasOrigin( &unit) && utIsTime( &unit ));
			}
		}
	else
		return( 0 );
}

/******************************************************************************/
int udu_calc_tgran( int fileid, NCVar *v, int dimid )
{
	utUnit	unit, seconds;
	double	slope, intercept;
	int	retval;
	int	verbose, has_bounds;
	double	tval0, tval1, delta_sec, bound_min, bound_max;
	char	cval0[1024], cval1[1024];
	nc_type rettype;

	NCDim *d;
	d = *(v->dim+dimid);

	/* Not enough data to analyze */
	if( d->size < 3 )
		return(TGRAN_SEC);

	verbose = 0;

	if( ! valid_udunits_pkg )
		return( 0 );

	if( utScan( d->units, &unit ) != 0 ) {
		fprintf( stderr, "internal error: udu_calc_tgran with invalid unit string: %s\n",
			d->units );
		exit( -1 );
		}

	if( utScan( "seconds", &seconds ) != 0 ) {
		fprintf( stderr, "internal error: udu_calc_tgran can't parse seconds unit string!\n" );
		exit( -1 );
		}
	
	if( utConvert( &unit, &seconds, &slope, &intercept ) != 0 ) {
		fprintf( stderr, "internal error: udu_calc_tgran can't convert time units to seconds!\n" );
		exit( -1 );
		}

	/* Get a delta time to analyze */
	rettype = fi_dim_value( v, dimid, 1L, &tval0, cval0, &has_bounds, &bound_min, &bound_max );
	rettype = fi_dim_value( v, dimid, 2L, &tval1, cval1, &has_bounds, &bound_min, &bound_max );
	delta_sec = fabs(tval1 - tval0)*slope;
	
	if( verbose ) 
		printf( "units: %s  t1: %lf t2: %lf delta-sec: %lf\n", d->units, tval0, tval1, delta_sec );

	if( delta_sec < 57. ) {
		if(verbose)
			printf("data is TGRAN_SEC\n");
		retval = TGRAN_SEC;
		}
	else if( delta_sec < 3590. ) {
		if(verbose)
			printf("data is TGRAN_MIN\n");
		retval = TGRAN_MIN;
		}
	else if( delta_sec < 86300. ) {
		if(verbose)
			printf("data is TGRAN_HOUR\n");
		retval = TGRAN_HOUR;
		}
	else if( delta_sec < 86395.*28. ) {
		if(verbose)
			printf("data is TGRAN_DAY\n");
		retval = TGRAN_DAY;
		}
	else if(  delta_sec < 86395.*365. ) {
		if(verbose)
			printf("data is TGRAN_MONTH\n");
		retval = TGRAN_MONTH;
		}
	else
		{
		if(verbose)
			printf("data is TGRAN_YEAR\n");
		retval = TGRAN_YEAR;
		}

	return( retval );
}

/******************************************************************************/
void udu_fmt_time( char *temp_string, double new_dimval, NCDim *dim, int include_granularity )
{
	static utUnit	dataunits;
	int	year, month, day, hour, minute, debug;
	float	second;
	static  char last_units[1024];
	static	char months[12][4] = { "Jan\0", "Feb\0", "Mar\0", "Apr\0",
				       "May\0", "Jun\0", "Jul\0", "Aug\0",
				       "Sep\0", "Oct\0", "Nov\0", "Dec\0"};

	debug = 0;
	if( debug ) fprintf( stderr, "udu_fmt_time: entering with dim=%s, units=%s, value=%f\n",
		dim->name, dim->units, new_dimval );

	if( ! valid_udunits_pkg ) {
		sprintf( temp_string, "%lg", new_dimval );
		return;
		}

	if( (strlen(dim->units) > 1023) || strcmp(dim->units,last_units) != 0 ) {
		if( utScan( dim->units, &dataunits ) != 0 ) {
			fprintf( stderr, "internal error: udu_fmt_time can't parse data unit string!\n" );
			fprintf( stderr, "problematic units: \"%s\"\n", dim->units );
			fprintf( stderr, "dim->name: %s  dim->timelike: %d\n", dim->name, dim->timelike );
			exit( -1 );
			}
		strncpy( last_units, dim->units, 1023 );
		}

	if( utCalendar_cal( new_dimval, &dataunits, &year, &month, &day, &hour, 
				&minute, &second, dim->calendar ) != 0 ) {
		fprintf( stderr, "internal error: udu_fmt_time can't convert to calendar value!\n");
		fprintf( stderr, "units: >%s<\n", dim->units );
		exit( -1 );
		}
	
	if( include_granularity ) {
		switch( dim->tgran ) {
			
			case TGRAN_YEAR:
			case TGRAN_MONTH:
			case TGRAN_DAY:
				sprintf( temp_string, "%1d-%s-%04d", day, months[month-1], year );
				break;

			case TGRAN_HOUR:
			case TGRAN_MIN:
				sprintf( temp_string, "%1d-%s-%04d %02d:%02d", day, 
						months[month-1], year, hour, minute );
				break;

			default:
				sprintf( temp_string, "%1d-%s-%04d %02d:%02d:%02.0f",
					day, months[month-1], year, hour, minute, second );
			}
		}
	else
		{
		sprintf( temp_string, "%1d-%s-%04d %02d:%02d:%02.0f",
				day, months[month-1], year, hour, minute, second );
		}
}

/******************************************************************************/
	static int 
is_unique( char *units )
{
	UniqList	*ul, *ul_new, *prev_ul;

	if( uniq == NULL ) {
		ul_new = (UniqList *)malloc( sizeof(UniqList) );
		ul_new->name = (char *)malloc(strlen(units)+1);
		strcpy( ul_new->name, units );
		ul_new->next = NULL;
		uniq = ul_new;
		return( TRUE );
		}
	
	ul = uniq;
	while( ul != NULL ) {
		if( strcmp( ul->name, units ) == 0 ) 
			return( FALSE );
		prev_ul = ul;
		ul = ul->next;
		}

	ul_new = (UniqList *)malloc( sizeof(UniqList) );
	ul_new->name = (char *)malloc(strlen(units)+1);
	strcpy( ul_new->name, units );
	ul_new->next = NULL;
	prev_ul->next = ul_new;
	return( TRUE );
}

/******************************************************************************/
#else

void udu_utinit( char *path )
{
	;
}

int udu_utistime( char *dimname, char *units )
{
	return( 0 );
}

int udu_calc_tgran( int fileid, NCVar *v, int dimid )
{
	return( 0 );
}

void udu_fmt_time( char *temp_string, double new_dimval, NCDim *dim, int include_granularity )
{
	sprintf( temp_string, "%g", new_dimval );
}

#endif
