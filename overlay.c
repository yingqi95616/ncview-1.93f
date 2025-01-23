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


#include "ncview.includes.h"
#include "ncview.defines.h"
#include "ncview.protos.h"

/* These are the arrays of coastline and outline data build into ncview
 * (i.e, they are NOT required to be loaded from an external directory)
 */
#include "overlay_coasts_p08deg.h"
#include "overlay_coasts_p8deg.h"
#include "overlay_usa.h"

/* Number and order of these must match the defines given in ncview.defines.h!
 * They are used as the labels for the radio buttons
 */
char *my_overlay_names[] = { "None",  
                        "0.8 degree coastlines",
		        "0.08 degree coastlines",
			"USA states",
			"custom" };

extern View  	*view;
extern Options  options;

static int	my_current_overlay;

static int 	gen_xform( float value, int n, float *dimvals );
static int 	*gen_overlay_internal( View *v, float *data, long n );
static void 	do_overlay_inner( View *v, float *data, long nvals, int suppress_screen_changes );

/*====================================================================================
 * This routine is only called when the state of the overlay is being changed
 */
	void
do_overlay( int n, char *custom_filename, int suppress_screen_changes )
{
	if( view == NULL ) {
		x_error( "You must select a variable before turning on overlays" );
		return;
		}

	/* Free space for previous overlay */
	if( options.overlay->doit && (options.overlay->overlay != NULL ))
		free( options.overlay->overlay );

	switch(n) {
		
		case OVERLAY_NONE:
			options.overlay->doit = FALSE;
			if( ! suppress_screen_changes ) {
				view->data_status = VDS_INVALID;
				invalidate_all_saveframes();
				change_view( 0, FRAMES );
				}
			break;

		case OVERLAY_P8DEG:
			do_overlay_inner( view, overlay_coasts_p8deg, n_overlay_coasts_p8deg,
					suppress_screen_changes );
			break;

		case OVERLAY_P08DEG:
			do_overlay_inner( view, overlay_coasts_p08deg, n_overlay_coasts_p08deg,
					suppress_screen_changes );
			break;

		case OVERLAY_USA:
			do_overlay_inner( view, overlay_usa, n_overlay_usa,
					suppress_screen_changes );
			break;

		case OVERLAY_CUSTOM:
			if( (custom_filename == NULL) || (strlen(custom_filename) == 0)) {
				in_error( "Specified custom overlay filename is not a valid filename!\n" );
				return;
				}
			options.overlay->overlay = gen_overlay( view, custom_filename ); 
			if( options.overlay->overlay != NULL ) {
				options.overlay->doit = TRUE;
				if( ! suppress_screen_changes ) {
					invalidate_all_saveframes();
					change_view( 0, FRAMES );
					}
				}
			break;

		default:
			fprintf( stderr, "Error, do_overlay called with an unknown index = %d\n", n );
			exit(-1);
		}

	my_current_overlay = n;
}

/*=========================================================================================
 */
	void
do_overlay_inner( View *v, float *data, long nvals, int suppress_screen_changes )
{
	options.overlay->overlay = gen_overlay_internal( v, data, nvals );
	if( options.overlay->overlay != NULL ) {
		options.overlay->doit = TRUE;
		if( ! suppress_screen_changes ) {
			invalidate_all_saveframes();
			change_view( 0, FRAMES );
			}
		}
}

/*=========================================================================================
 * This is called just once, when ncview starts up.  In particular,
 * it is NOT called every time we start a new overlay.
 */
	void
overlay_init()
{
	my_current_overlay       = OVERLAY_NONE;
	options.overlay->overlay = NULL;
	options.overlay->doit    = FALSE;
}

/*======================================================================================
 * NOTE: overlay_base_dir must already be allocated to length 'n'
 */
	void
determine_overlay_base_dir( char *overlay_base_dir, int n )
{
	char	*dir;

	dir = (char *)getenv( "NCVIEWBASE" );
	if( dir == NULL ) {
#ifdef NCVIEW_LIB_DIR
		if( strlen(NCVIEW_LIB_DIR) >= n ) {
			fprintf( stderr, "Error, routine determine_overlay_base_dir, string NCVIEW_LIB_DIR too long! Max=%d\n", n );
			exit(-1);
			}
		strcpy( overlay_base_dir, NCVIEW_LIB_DIR );
#else
		strcpy( overlay_base_dir, "." );
#endif
		}
	else
		{
		if( strlen(dir) >= n ) {
			fprintf( stderr, "Error, routine determine_overlay_base_dir, length of dir is too long! Max=%d\n", n );
			exit(-1);
			}
		strcpy( overlay_base_dir, dir );
		}
}

/******************************************************************************
 * Generate an overlay from data in an overlay file.
 */
	int *
gen_overlay_internal( View *v, float *data, long nvals )
{
	NCDim	*dim_x, *dim_y;
	size_t	x_size, y_size, ii;
	int	*overlay;
	float	x, y;
	long	i, j;

	dim_x = *(v->variable->dim + v->x_axis_id);
	dim_y = *(v->variable->dim + v->y_axis_id);

	x_size = *(v->variable->size + v->x_axis_id);
	y_size = *(v->variable->size + v->y_axis_id);

	overlay = (int *)malloc( x_size*y_size*sizeof(int) );
	if( overlay == NULL ) {
		in_error( "Malloc of overlay field failed\n" );
		return( NULL );
		}
	for( ii=0; ii<x_size*y_size; ii++ )
		*(overlay+ii) = 0;

	for( ii=0; ii<nvals; ii+=2 ) {
		x = data[ii];
		y = data[ii+1];

		i = gen_xform( x, x_size, dim_x->values );
		if( i == -2 ) 
			return( NULL );
		j = gen_xform( y, y_size, dim_y->values );
		if( j == -2 ) 
			return( NULL );
		if( (i > 0) && (j > 0)) 
			*(overlay + j*x_size + i) = 1;
		}

	return( overlay );
}

/******************************************************************************
 * Generate an overlay from data in an overlay file.
 */
	int *
gen_overlay( View *v, char *overlay_fname )
{
	FILE	*f;
	char	err_mess[1024], line[80], *id_string="NCVIEW-OVERLAY";
	float	x, y, version;
	long	i, j;
	size_t	x_size, y_size;
	int	*overlay;
	NCDim	*dim_x, *dim_y;

	/* Open the overlay file */
	if( (f = fopen(overlay_fname, "r")) == NULL ) {
		sprintf( err_mess, "Error: can't open overlay file named \"%s\"\n", 
			overlay_fname );
		in_error( err_mess );
		return( NULL );
		}

	/* Make sure it is a valid overlay file
	 */
	if( fgets(line, 80, f) == NULL ) {
		sprintf( err_mess, "Error trying to read overlay file named \"%s\"\n",
			overlay_fname );
		in_error( err_mess );
		return( NULL );
		}
	for( i=0; i<strlen(id_string); i++ )
		if( line[i] != id_string[i] ) {
			sprintf( err_mess, "Error trying to read overlay file named \"%s\"\nFile does not start with \"%s version-num\"\n", 
				overlay_fname, id_string );
			in_error( err_mess );
			return( NULL );
			}
	sscanf( line, "%*s %f", &version );
	if( (version < 0.95) || (version > 1.05)) {
		sprintf( err_mess, "Error, overlay file has unknown version number: %f\nI am set up for version 1.0\n", version );
		in_error( err_mess );
		return( NULL );
		}

	dim_x = *(v->variable->dim + v->x_axis_id);
	dim_y = *(v->variable->dim + v->y_axis_id);

	x_size = *(v->variable->size + v->x_axis_id);
	y_size = *(v->variable->size + v->y_axis_id);

	overlay = (int *)malloc( x_size*y_size*sizeof(int) );
	if( overlay == NULL ) {
		in_error( "Malloc of overlay field failed\n" );
		return( NULL );
		}
	for( i=0; i<x_size*y_size; i++ )
		*(overlay+i) = 0;

	/* Read in the overlay file -- skip lines with first char of #, 
	 * they are comments.
	 */
	while( fgets(line, 80, f) != NULL ) 
		if( line[0] != '#' ) {
			sscanf( line, "%f %f", &x, &y );
			i = gen_xform( x, x_size, dim_x->values );
			if( i == -2 ) 
				return( NULL );
			j = gen_xform( y, y_size, dim_y->values );
			if( j == -2 ) 
				return( NULL );
			if( (i > 0) && (j > 0)) 
				*(overlay + j*x_size + i) = 1;
			}

	return( overlay );
}

/******************************************************************************
 * Given the (dimensional) value from the overlay file, convert it to 
 * the nearest index along the proper dimension that the point corresponds to.
 */
	int
gen_xform( float value, int n, float *dimvals )
{
	float	min_dist, dist;
	int	i, min_place;

	min_dist  = 1.0e35;
	min_place = 0;

	/* See if off ends of dimvalues ... remember that it can be reversed */
	if( *dimvals > *(dimvals+n-1) ) {
		/* reversed */
		if( value > *dimvals )
			return( -1 );
		if( value < *(dimvals+n-1) )
			return( -1 );
		}
	else
		{
		if( value < *dimvals )
			return( -1 );
		if( value > *(dimvals+n-1) )
			return( -1 );
		}
	
	for( i=0; i<n; i++ ) {
		dist = fabs(*(dimvals+i) - value);
		if( dist < min_dist ) {
			min_dist  = dist;
			min_place = i;
			}
		}

	return( min_place );
}

/****************************************************************************************/
	char **
overlay_names( void )
{
	return( my_overlay_names );
}

/****************************************************************************************/
	int
overlay_current( void )
{
	return( my_current_overlay );
}

/****************************************************************************************/
	int
overlay_n_overlays( void )
{
	return( OVERLAY_N_OVERLAYS );
}

/****************************************************************************************/
	int
overlay_custom_n( void )
{
	return( OVERLAY_CUSTOM );
}

