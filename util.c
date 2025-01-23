/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993 through 2008  David W. Pierce
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

/*******************************************************************************
 * 	util.c
 *
 *	utility routines for ncview
 *
 *	should be independent of both the user interface and the data
 * 	file format.
 *******************************************************************************/

#include "ncview.includes.h"
#include "ncview.defines.h"
#include "ncview.protos.h"

#include "math.h"

#ifdef INC_UDUNITS
#include <udunits.h>
#endif

extern Options   options;
extern NCVar     *variables;
extern ncv_pixel *pixel_transform;
extern FrameStore framestore;

static void get_min_max_onestep( NCVar *var, size_t n_other, size_t tstep, float *data, 
					float *min, float *max, int verbose );
static void handle_time_dim( int fileid, NCVar *v, int dimid );
static int  months_calc_tgran( int fileid, NCDim *d );
static float util_mean( float *x, size_t n, float fill_value );
static float util_mode( float *x, size_t n, float fill_value );
static void contract_data( float *small_data, View *v, float fill_value );
static int equivalent_FDBs( NCVar *v1, NCVar *v2 );
static int data_has_mv( float *data, size_t n, float fill_value );

/* Variables local to routines in this file */
static  char    *month_name[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/*******************************************************************************
 * Determine whether the data is "close enough" to the fill value
 */
int
close_enough( float data, float fill )
{
	float	criterion, diff;
	int	retval;

	if( fill == 0.0 )
		criterion = 1.0e-5;
	else if( fill < 0.0 )
		criterion = -1.0e-5*fill;
	else
		criterion = 1.0e-5*fill;

	diff = data - fill;
	if( diff < 0.0 ) 
		diff = -diff;

	if( diff <= criterion )
		retval = 1;
	else
		retval = 0;

 /* printf( "d=%f f=%f c=%f r=%d\n", data, fill, criterion, retval );  */
	return( retval );
}

/*******************************************************************************
 * Adds the given string to the list, and returns a pointer to the 
 * new list element.
 */
	Stringlist *
add_to_stringlist( Stringlist **list, char *new_string, void *aux )
{
	Stringlist 	*cursor, *new_el;
	int		i;

	new_stringlist( &new_el );
	new_el->string = (char *)malloc( strlen( new_string )+1);
	if( new_el == NULL ) {
		fprintf( stderr, "ncview: add_to_stringlist: malloc failed\n" );
		fprintf( stderr, "string trying to add: %s\n", new_string );
		exit( -1 );
		}
	strcpy( new_el->string, new_string );
	new_el->aux    = aux;

	i = 0;
	if( *list == NULL ) {
		*list = new_el;
		new_el->prev = NULL;
		}
	else
		{
		i = 1;
		cursor = *list;
		while( cursor->next != NULL ) {
			cursor = cursor->next;
			i++;
			}
		cursor->next = new_el;
		new_el->prev = cursor;
		}
	new_el->index = i;

	return( new_el );
}

/*******************************************************************************
 * Adds the given string to the list, and returns a pointer to the 
 * new list element, with alphabetic ordering.
 */
	Stringlist *
add_to_stringlist_ordered( Stringlist **list, char *new_string, void *aux )
{
	Stringlist 	*cursor, *new_el, *prev_el;
	int		i;

	new_stringlist( &new_el );
	new_el->string = (char *)malloc( strlen( new_string )+1);
	if( new_el == NULL ) {
		fprintf( stderr, "ncview: add_to_stringlist: malloc failed\n" );
		fprintf( stderr, "string trying to add: %s\n", new_string );
		exit( -1 );
		}
	strcpy( new_el->string, new_string );
	new_el->aux    = aux;

	i = 0;
	if( *list == NULL ) {
		*list = new_el;
		new_el->prev = NULL;
		}
	else
		{
		i = 1;
		cursor = *list;
		prev_el = NULL;
		while( (cursor != NULL) && (strcmp( new_string, cursor->string) > 0)) {
			prev_el = cursor;
			cursor  = cursor->next;
			i++;
			}
		if( cursor == NULL ) {
			prev_el->next = new_el;
			new_el->prev  = prev_el;
			}
		else if( prev_el == NULL ) {
			*list = new_el;
			new_el->next = cursor;
			cursor->prev = new_el;
			}
		else
			{
			new_el->next = cursor;
			cursor->prev = new_el;
			prev_el->next = new_el;
			new_el->prev  = prev_el;
			}
		}
	new_el->index = i;

	return( new_el );
}


/*******************************************************************************
 * Concatenate onto a stringlist
 */
	void
sl_cat( Stringlist **dest, Stringlist **src )
{
	Stringlist *sl;

	sl = *src;
	while( sl != NULL ) {
		add_to_stringlist( dest, sl->string, NULL );
		sl = sl->next;
		}
}

/*******************************************************************************
 * Allocate space for a new Stringlist element 
 */
	void
new_stringlist( Stringlist **el )
{
	(*el)       = (Stringlist *)malloc( sizeof( Stringlist ));
	(*el)->next = NULL;
}

/******************************************************************************
 * Add the passed NCVar element to the list 
 */
	void
add_to_varlist( NCVar **list, NCVar *new_el )
{
	int	i;
	NCVar	*cursor;

	i = 0;
	if( *list == NULL ) {
		*list = new_el;
		new_el->prev = NULL;
		}
	else
		{
		i = 1;
		cursor = *list;
		while( cursor->next != NULL ) {
			cursor = cursor->next;
			i++;
			}
		cursor->next = new_el;
		new_el->prev = cursor;
		}
}

/******************************************************************************
 * Allocate space for a new NCVar element
 */
	void
new_variable( NCVar **el )
{
	(*el)       = (NCVar *)malloc( sizeof( NCVar ));
	(*el)->next = NULL;
}


/******************************************************************************
 * Allocate space for a new FDBlist element
 */
	void
new_fdblist( FDBlist **el )
{
	NetCDFOptions	*new_netcdf_options;

	(*el)           = (FDBlist *)malloc( sizeof( FDBlist ));
	(*el)->next     = NULL;
	(*el)->filename     = (char *)malloc( MAX_FILE_NAME_LEN );
	(*el)->recdim_units = (char *)malloc( MAX_RECDIM_UNITS_LEN );

#ifdef INC_UDUNITS
	(*el)->udunits	    = (utUnit *)malloc( sizeof(utUnit) );
#endif

	strcpy( (*el)->filename, "UNINITIALIZED" );

	new_netcdf( &new_netcdf_options );
	(*el)->aux_data = new_netcdf_options;
}

/******************************************************************************
 * Allocate space for a NetCDFOptions structure.
 */
	void
new_netcdf( NetCDFOptions **n )
{
	(*n) = (NetCDFOptions *)malloc( sizeof( NetCDFOptions ));
	(*n)->valid_range_set  = FALSE;
	(*n)->valid_min_set    = FALSE;
	(*n)->valid_max_set    = FALSE;
	(*n)->scale_factor_set = FALSE;
	(*n)->add_offset_set   = FALSE;

	(*n)->valid_range[0] = 0.0;
	(*n)->valid_range[1] = 0.0;
	(*n)->valid_min      = 0.0;
	(*n)->valid_max      = 0.0;
	(*n)->scale_factor   = 1.0;
	(*n)->add_offset     = 0.0;
}

/******************************************************************************
 * What's in this stringlist, anyway?
 */
	void
dump_stringlist( Stringlist *s )
{
	while( s != NULL ) {
		printf( "%d: %s\n", s->index, s->string );
		s = s->next;
		}
}

/******************************************************************************
 * Return 1 if any data value is missing, 0 otherwise
 */
	int
data_has_mv( float *data, size_t n, float fill_value )
{
	size_t i;

	for( i=0; i<n; i++ )
		if( close_enough( data[i], fill_value ))
			return(1);

	return(0);
}

/******************************************************************************
 * Scale the data, replicate it, and convert to a pixel type array.  I'm afraid
 * that for speed, this considers 'ncv_pixel' to be a single byte value.  Make sure
 * to change it if you change the definition of ncv_pixel!  Returns 0 on
 * success, -1 on failure.
 */
	int
data_to_pixels( View *v )
{
	long	i, j, j2;
	size_t	x_size, y_size, new_x_size, new_y_size;
	ncv_pixel pix_val;
	float	data_range, rawdata, data, fill_value, *scaled_data;
	long	blowup, result, orig_minmax_method;
	char	error_message[1024];

	/* Make sure the limits have been set on this variable.
	 * They won't always be because an initial expose event can 
	 * cause this routine to be executed before the min and
	 * maxes are calcuclated.
	 */
	if( ! v->variable->have_set_range )
		return( -1 );

	blowup   = options.blowup;	/* NOTE: can be negative if shrinking data! -N means size is 1/Nth */

	x_size     = *(v->variable->size + v->x_axis_id);
	y_size     = *(v->variable->size + v->y_axis_id);

	view_get_scaled_size( options.blowup, x_size, y_size, &new_x_size, &new_y_size );

	scaled_data   = (float *)malloc( new_x_size*new_y_size*sizeof(float));
	if( scaled_data == NULL ) {
		fprintf( stderr, "ncview: data_to_pixels: can't allocate data expansion array\n" );
		fprintf( stderr, "requested size: %ld bytes\n", new_x_size*new_y_size*sizeof(float) );
		fprintf( stderr, "new_x_size, new_y_size, float_size: %ld %ld %ld\n", 
				new_x_size, new_y_size, sizeof(float) );
		fprintf( stderr, "blowup: %d\n", options.blowup );
		exit( -1 );
		}

	/* If we are doing overlays, implement them */
	if( options.overlay->doit && (options.overlay->overlay != NULL)) {
		for( i=0; i<(x_size*y_size); i++ ) {
			*((float *)v->data + i) = 
			     (float)(1 - *(options.overlay->overlay+i)) * *((float *)v->data + i) +
			     (float)(*(options.overlay->overlay+i)) * v->variable->fill_value;
			}
		}

	fill_value = v->variable->fill_value;

	if( blowup > 0 )
		expand_data( scaled_data, v );
	else
		contract_data( scaled_data, v, fill_value );

	data_range = v->variable->user_max - v->variable->user_min;

	if( (v->variable->user_max == 0) &&
	    (v->variable->user_min == 0) ) {
		in_set_cursor_normal();
		in_button_pressed( BUTTON_PAUSE, MOD_1 );
		if( options.min_max_method == MIN_MAX_METHOD_EXHAUST ) {
	    		sprintf( error_message, "min and max both 0 for variable %s.\n(Checked all data)", 
								v->variable->name );
			in_error( error_message );
			if( ! data_has_mv( v->data, x_size*y_size, fill_value ) )
				return( -1 );
			v->variable->user_max = 1;
			}
	    	sprintf( error_message, "min and max both 0 for variable %s.\nI can check ALL the data instead of subsampling if that's OK,\nor just cancel viewing this variable.",
	    				v->variable->name );
		result = in_dialog( error_message, NULL, TRUE );
		if( result == MESSAGE_OK ) {
			orig_minmax_method = options.min_max_method;
			options.min_max_method = MIN_MAX_METHOD_EXHAUST;
			init_min_max( v->variable );
			options.min_max_method = orig_minmax_method;
			if( (v->variable->user_max == 0) &&
	    		    (v->variable->user_min == 0) ) {
	    			sprintf( error_message, "min and max both 0 for variable %s.\n(Checked all data)", 
								v->variable->name );
				in_error( error_message );
				if( ! data_has_mv( v->data, x_size*y_size, fill_value ) )
					return( -1 );
				v->variable->user_max = 1;
				}
			else
				return( data_to_pixels(v) );
			}
		else
			{
			if( ! data_has_mv( v->data, x_size*y_size, fill_value ) )
				return( -1 );
			v->variable->user_max = 1;
			}
	    	}

	if( v->variable->user_max == v->variable->user_min ) {
		in_set_cursor_normal();
	    	sprintf( error_message, "min and max both %g for variable %s",
	    		v->variable->user_min, v->variable->name );
		x_error( error_message );
		if( ! data_has_mv( v->data, x_size*y_size, fill_value ) )
			return( -1 );
		/* If we get here, data is all same, but have a missing value,
		 * so let's go ahead and show it
		 */
		if( v->variable->user_max == 0 )
			v->variable->user_max = 1;
		else if( v->variable->user_max > 0 )
			v->variable->user_min = 0;
		else
			v->variable->user_max = 0;
	    	}

	for( j=0; j<new_y_size; j++ ) {

		if( options.invert_physical )
			j2 = j;
		else
			j2 = new_y_size - j - 1;

		for( i=0; i<new_x_size; i++ ) {
			rawdata =  *(scaled_data + i + j2*new_x_size);
			if( close_enough(rawdata, fill_value) || (rawdata == FILL_FLOAT))
				pix_val = *pixel_transform;
			else
				{
				data = (rawdata - v->variable->user_min) / data_range;
				clip_f( &data, 0.0, .9999 );
				switch( options.transform ) {
					case TRANSFORM_NONE:	break;

					/* This might cause problems.  It is at odds with what
					 * the manual claims--at least for Ultrix--but works, 
					 * whereas what the manual claims works, doesn't!
					 */
					case TRANSFORM_LOW:	data = sqrt( data );  
								data = sqrt( data );
								break;

					case TRANSFORM_HI:	data = data*data*data*data;     break;
					}		
				if( options.invert_colors )
					data = 1. - data;
				pix_val = (ncv_pixel)(data * options.n_colors) + 10;
				if( options.display_type == PseudoColor )
					pix_val = *(pixel_transform+pix_val);
				}
			*(v->pixels + i + j*new_x_size) = pix_val;
			}
		}
	free( scaled_data );
	return( 0 );
}

/******************************************************************************
 * Returns the number of entries in the NCVarlist
 */
	int
n_vars_in_list( NCVar *v )
{
	NCVar *cursor;
	int   i;

	i      = 0;
	cursor = v;
	while( cursor != NULL ) {
		i++;
		cursor = cursor->next;
		}

	return( i );
}

/******************************************************************************
 * Returns the number of entries in the Stringlist
 */
	int
n_strings_in_list( Stringlist *s )
{
	Stringlist *c;
	int	   i;

	i = 0;
	c = s;
	while( c != NULL ) {
		i++;
		c = c->next;
		}

	return( i );
}
	
/******************************************************************************
 * Given a list of variable *names*, initialize the variable *structure* and
 * add it to the global list of variables.  Input arg 'nfiles' is the total
 * number of files indicated on the command line, this can be useful for 
 * initializing arrays.
 */
	void
add_vars_to_list( Stringlist *var_list, int id, char *filename, int nfiles )
{
	Stringlist *var;

	if( options.debug )
		fprintf( stderr, "adding vars to list for file %s\n", filename );
	var = var_list;
	while( var != NULL ) {
		if( options.debug ) 
			fprintf( stderr, "adding variable %s to list\n", var->string );
		add_var_to_list( var->string, id, filename, nfiles );
		var = var->next;
		}
	if( options.debug ) 
		fprintf( stderr, "done adding vars for file %s\n", filename );
}

/******************************************************************************
 * For the given variable name, fill out the variable and file structures,
 * and add them into the global variable list.
 */
	void
add_var_to_list( char *var_name, int file_id, char *filename, int nfiles )
{
	NCVar	*var, *new_var;
	int	n_dims, err;
	FDBlist	*new_fdb, *fdb;

	/* make a new file description entry for this var/file combo */
	new_fdblist( &new_fdb );
	new_fdb->id       = file_id;
	new_fdb->var_size = fi_var_size( file_id, var_name );
	if( strlen(filename) > (MAX_FILE_NAME_LEN-1)) {
		fprintf( stderr, "Error, input file name is too long; longest I can handle is %d\nError occurred on file %s\n",
			MAX_FILE_NAME_LEN, filename );
		exit(-1);
		}
	strcpy( new_fdb->filename, filename );

	/* fill out auxilliary (data-file format dependent) information
	 * for the new fdb.
	 */
	fi_fill_aux_data( file_id, var_name, new_fdb );
#ifdef INC_UDUNITS
	err = utScan( new_fdb->recdim_units, new_fdb->udunits );
	if( err != 0 )
		new_fdb->udunits = NULL;
#endif

	/* Does this variable already have an entry on the global var list "variables"? */
	var = get_var( var_name );
	if( var == NULL ) {	/* NO -- make a new NCVar structure */
		new_variable( &new_var );
		new_var->name       = (char *)malloc( strlen(var_name)+1 );
		strcpy( new_var->name, var_name );
		n_dims              = fi_n_dims( file_id, var_name );
		new_var->n_dims     = n_dims;
		if( options.debug )
			fprintf( stderr, "adding variable %s with %d dimensions\n",
				var_name, n_dims );
		new_var->first_file = new_fdb;
		new_var->last_file  = new_fdb;
		new_var->global_min = 0.0;
		new_var->global_max = 0.0;
		new_var->user_min   = 0.0;
		new_var->user_max   = 0.0;
		new_var->have_set_range = FALSE;
		new_var->size       = fi_var_size( file_id, var_name );
		new_var->fill_value = DEFAULT_FILL_VALUE;
		fi_fill_value( new_var, &(new_var->fill_value) );
		new_fdb->prev       = NULL;
		fill_dim_structs( new_var );
		add_to_varlist  ( &variables, new_var );
		new_var->is_virtual = FALSE;
		}
	else	/* YES -- just add the FDB to the list of files in which 
		 * this variable appears, and accumulate the variable's size.
		 */
		{
		/* Go to the end of the file list and add it there */
		if( options.debug )
			fprintf( stderr, "adding another file with variable %s in it\n",
				var_name );
		if( var->last_file == NULL ) {
			fprintf( stderr, "ncview: add_var_to_list: internal ");
			fprintf( stderr, "inconsistancy; var has no last_file\n" );
			exit( -1 );
			}
		fdb = var->first_file;
		while( fdb->next != NULL )
			fdb = fdb->next;
		fdb->next         = new_fdb;
		new_fdb->prev     = fdb;
		var->last_file    = new_fdb;
		*(var->size)      += *(new_fdb->var_size);
		var->is_virtual   = TRUE;
		}
}

/******************************************************************************
 * Calculate the min and max values for the passed variable.
 */
	void
init_min_max( NCVar *var )
{
	long	n_other, i, step;
	size_t	n_timesteps;
	float	*data, init_min, init_max;
	int	verbose;

	init_min =  9.9e30;
	init_max = -9.9e30;
	var->global_min = init_min;
	var->global_max = init_max;

	printf( "calculating min and maxes for %s", var->name );

	/* n_other is the number of elements in a single timeslice of the data array */
	n_timesteps = *(var->size);
	n_other     = 1L;
	for( i=1; i<var->n_dims; i++ )
		n_other *= *(var->size+i);

	data = (float *)malloc( n_other * sizeof( float ));
	if( data == NULL ) {
		fprintf( stderr, "ncview: init_min_max_file: failed on malloc of data array\n" );
		exit( -1 );
		}

	/* We always get the min and max of the first, middle, and last time 
	 * entries if they are distinct.
	 */
	verbose = TRUE;
	step    = 0L;
	get_min_max_onestep( var, n_other, step, data, 
			&(var->global_min), &(var->global_max), verbose );
	if( n_timesteps == 1 ) {
		if( verbose )
			printf( "\n" );
		free( data );
		check_ranges( var );
		return;
		}

	step = n_timesteps-1L;
	get_min_max_onestep( var, n_other, step, data, 
			&(var->global_min), &(var->global_max), verbose );
	if( n_timesteps == 2 ) {
		if( verbose )
			printf( "\n" );
		free( data );
		check_ranges( var );
		return;
		}

	step = (n_timesteps-1L)/2L;
	get_min_max_onestep( var, n_other, step, data, 
			&(var->global_min), &(var->global_max), verbose );
	if( n_timesteps == 3 ) {
		if( verbose )
			printf( "\n" );
		free( data );
		check_ranges( var );
		return;
		}

	switch( options.min_max_method ) {
		case MIN_MAX_METHOD_FAST: 
			if( verbose )
				printf( "\n" );
			break;
			
		case MIN_MAX_METHOD_MED:     
			verbose = TRUE;
			step = (n_timesteps-1L)/4L;
			get_min_max_onestep( var, n_other, step, data, 
				&(var->global_min), &(var->global_max), verbose );
			step = (3L*(n_timesteps-1L))/4L;
			get_min_max_onestep( var, n_other, step, data, 
				&(var->global_min), &(var->global_max), verbose );
			if( verbose )
				printf( "\n" );
			break;
				
		case MIN_MAX_METHOD_SLOW:
			verbose = TRUE;
			for( i=2; i<=9; i++ ) { 
				printf( "." );
				step = (i*(n_timesteps-1L))/10L;
				get_min_max_onestep( var, n_other, step, data, 
					&(var->global_min), &(var->global_max), verbose );
				}
			if( verbose )
				printf( "\n" );
			break;
			
		case MIN_MAX_METHOD_EXHAUST:
			verbose = TRUE;
			for( i=1; i<(n_timesteps-2L); i++ ) {
				step = i;
				get_min_max_onestep( var, n_other, step, data, 
					&(var->global_min), &(var->global_max), verbose );
				}
			if( verbose )
				printf( "\n" );
			break;
		}

	if( (var->global_min == init_min) && (var->global_max == init_max) ) {
		var->global_min = 0.0;
		var->global_max = 0.0;
		}
		
	check_ranges( var );
	free( data );
}

/******************************************************************************
 * Try to reconcile the computed and specified (if any) data range
 */
	void
check_ranges( NCVar *var )
{
	float	min, max;
	int	message;
	char	temp_string[ 1024 ];

	if( netcdf_min_max_option_set( var, &min, &max ) ) {
		if( var->global_min < min ) {
			sprintf( temp_string, "Calculated minimum (%g) is less than\nvalid_range minimum (%g).  Reset\nminimum to valid_range minimum?", var->global_min, min );
			message = in_dialog( temp_string, NULL, TRUE );
			if( message == MESSAGE_OK )
				var->global_min = min;
			}
		if( var->global_max > max ) {
			sprintf( temp_string, "Calculated maximum (%g) is greater\nthan valid_range maximum (%g). Reset\nmaximum to valid_range maximum?", var->global_max, max );
			message = in_dialog( temp_string, NULL, TRUE );
			if( message == MESSAGE_OK )
				var->global_max = max;
			}
		}

	if( netcdf_min_option_set( var, &min ) ) {
		if( var->global_min < min ) {
			sprintf( temp_string, "Calculated minimum (%g) is less than\nvalid_min minimum (%g).  Reset\nminimum to valid_min value?", var->global_min, min );
			message = in_dialog( temp_string, NULL, TRUE );
			if( message == MESSAGE_OK )
				var->global_min = min;
			}
		}

	if( netcdf_max_option_set( var, &max ) ) {
		if( var->global_max > max ) {
			sprintf( temp_string, "Calculated maximum (%g) is greater than\nvalid_max maximum (%g).  Reset\nmaximum to valid_max value?", var->global_max, max );
			message = in_dialog( temp_string, NULL, TRUE );
			if( message == MESSAGE_OK )
				var->global_max = max;
			}
		}

	var->user_min = var->global_min;
	var->user_max = var->global_max;
	var->have_set_range = TRUE;
}

/******************************************************************************
 * get_min_max utility routine; is passed timestep number where want to 
 * determine extrema 
 */
	static void
get_min_max_onestep( NCVar *var, size_t n_other, size_t tstep, float *data, 
					float *min, float *max, int verbose )
{
	size_t	*start, *count, n_time;
	size_t	j;
	int	i;
	float	dat, fill_v;
	
	count  = (size_t *)malloc( var->n_dims * sizeof( size_t ));
	start  = (size_t *)malloc( var->n_dims * sizeof( size_t ));
	fill_v = var->fill_value;

	n_time = *(var->size);
	if( tstep > (n_time-1) )
		tstep = n_time-1;

	*(count) = 1L;
	*(start) = tstep;
	for( i=1; i<var->n_dims; i++ ) {
		*(start+i) = 0L;
		*(count+i) = *(var->size + i);
		}

	if( verbose ) {
		printf( "." );
		fflush( stdout );
		}

	fi_get_data( var, start, count, data );

	for( j=0; j<n_other; j++ ) {
		dat = *(data+j);
		if( dat != dat )
			dat = fill_v;
		if( (! close_enough(dat, fill_v)) && (dat != FILL_FLOAT) )
			{
			if( dat > *max )
				*max = dat;
			if( dat < *min )
				*min = dat;
			}
		}
		
	free( count );
	free( start );
}

/******************************************************************************
 * convert a variable name to a NCVar structure
 */
	NCVar *
get_var( char *var_name )
{
	NCVar	*ret_val;

	ret_val = variables;
	while( ret_val != NULL )
		if( strcmp( var_name, ret_val->name ) == 0 )
			return( ret_val );
		else
			ret_val = ret_val->next;

	return( NULL );
}

/******************************************************************************
 * Clip out of range floats 
 */
	void
clip_f( float *data, float min, float max )
{
	if( *data < min )
		*data = min;
	if( *data > max )
		*data = max;
}

/******************************************************************************
 * Turn a virtual variable 'place' array into a file/place pair.  Which is
 * to say, the virtual size of a variable spans the entries in all the files; 
 * the actual place where the entry for a particular virtual location can
 * be found is in a file/actual_place pair.  This routine does the conversion.
 * Note that this routine is assuming the netCDF convention that ONLY THE
 * FIRST index can be contiguous across files.  The first index is typically
 * the time index in netCDF files.  NOTE! that 'act_pl' must be allocated 
 * before calling this!
 */
	void
virt_to_actual_place( NCVar *var, size_t *virt_pl, size_t *act_pl, FDBlist **file )
{
	FDBlist	*f;
	size_t	v_place, cur_start, cur_end;
	int	i, n_dims;

	f       = var->first_file;
	n_dims  = fi_n_dims( f->id, var->name );
	v_place = *(virt_pl);

	if( v_place >= *(var->size) ) {
		fprintf( stderr, "ncview: virt_to_actual_place: error trying ");
		fprintf( stderr, "to convert the following virtual place to\n" );
		fprintf( stderr, "an actual place for variable %s:\n", var->name );
		for( i=0; i<n_dims; i++ )
			fprintf( stderr, "[%1d]: %ld\n", i, *(virt_pl+i) );
		exit( -1 );
		}

	cur_start = 0L;
	cur_end   = *(f->var_size) - 1L;
	while( v_place > cur_end ) {
		cur_start += *(f->var_size);
		f          = f->next;
		cur_end   += *(f->var_size);
		}

	*file = f;
	*act_pl = v_place - cur_start;

	/* Copy the rest of the indices over */
	for( i=1; i<n_dims; i++ )
		*(act_pl+i) = *(virt_pl+i);
}

/******************************************************************************
 * Initialize all the fields in the dim structure by reading from the data file
 */
	void
fill_dim_structs( NCVar *v )
{
	int	i, fileid;
	NCDim	*d;
	char	*dim_name, *tmp_units;
	static  int global_id = 0;
	FDBlist	*cursor;

	fileid = v->first_file->id;
	v->dim = (NCDim **)malloc( v->n_dims*sizeof( NCDim *));
	for( i=0; i<v->n_dims; i++ ) {
		if( is_scannable( v, i ) ) {
			dim_name     	= fi_dim_id_to_name( fileid, v->name, i );
			*(v->dim + i)	= (NCDim *)malloc( sizeof( NCDim ));
			d            	= *(v->dim+i);
			d->name      	= dim_name;
			d->long_name 	= fi_dim_longname( fileid, dim_name );
			d->have_calc_minmax = 0;
			d->units     	= fi_dim_units   ( fileid, dim_name );
			d->units_change = 0;
			d->size      	= *(v->size+i);
			d->calendar  	= fi_dim_calendar( fileid, dim_name );
			d->global_id 	= ++global_id;
			handle_time_dim( fileid, v, i );
			if( options.debug ) 
				fprintf( stderr, "adding scannable dim to var %s: dimname: %s dimsize: %ld\n", v->name, dim_name, d->size );
			}
		else
			{
			/* Indicate non-scannable dimensions by a NULL */
			*(v->dim + i) = NULL;
			if( options.debug ) 
				fprintf( stderr, "adding non-scannable dim to var %s\n", v->name );
			}
		}

	/* If this variable lives in more than one file, it might have 
	 * different time units in each one.  Check for this.
	 */
	if( v->is_virtual && (*(v->dim) != NULL) && (v->first_file->next != NULL) ) {
		/* The timelike dimension MUST be the first one! */
		if( d->timelike ) {
			/* Go through each file and see if it has the same units
			 * as the first file, which is stored in d->units 
			 */
			cursor = v->first_file->next;
			while( cursor != NULL ) {
				tmp_units = fi_dim_units( cursor->id, d->name );
				if( strcmp( d->units, tmp_units ) != 0 ) {
					printf( "** Warning: different time units found in different files.  Trying to compensate...\n" );
					d->units_change = 1;
					}
				}
			}
		}
}

/******************************************************************************
 * Is this variable a "scannable" variable -- i.e., accessable by the taperecorder
 * style buttons?
 */
	int
is_scannable( NCVar *v, int i )
{
	/* The unlimited record dimension is always scannable */
	if( i == 0 )
		return( TRUE );

	if( *(v->size+i) > 1 )
		return( TRUE );
	else
		return( FALSE );
}

/******************************************************************************
 * Return the mode (most common value) of passed array "x".  We assume "x"
 * contains the floating point representation of integers.
 */
	float
util_mode( float *x, size_t n, float fill_value )
{
	long 	i, n_vals;
	double 	sum;
	long 	ival, *count_vals, max_count, *unique_vals;
	int	foundval, j, max_index;
	float	retval;

	count_vals  = (long *)malloc( n*sizeof(long) );
	unique_vals = (long *)malloc( n*sizeof(long) );

	sum = 0.0;
	n_vals = 0;
	for( i=0L; i<n; i++ ) {
		if( close_enough( x[i], fill_value )) {
			free(count_vals);
			free(unique_vals);
			return( fill_value );
			}
		ival = (x[i] > 0.) ? (long)(x[i]+.4) : (long)(x[i]-.4); /* round x[i] to nearest integer */
		foundval = -1;
		for( j=0; j<n_vals; j++ ) {
			if( unique_vals[j] == ival ) {
				foundval = j;
				break;
				}
			}
		if( foundval == -1 ) {
			unique_vals[n_vals] = ival;
			count_vals[n_vals] = 1;
			n_vals++;
			}
		else
			count_vals[foundval]++;
		}

	max_count = -1;
	max_index = -1;
	for( i=0L; i<n_vals; i++ )
		if( count_vals[i] > max_count ) {
			max_count = count_vals[i];
			max_index = i;
			}

	retval = (float)unique_vals[max_index];

	free(count_vals);
	free(unique_vals);

	return( retval );
}

/******************************************************************************/
	float
util_mean( float *x, size_t n, float fill_value )
{
	long i;
	double sum;

	sum = 0.0;
	for( i=0L; i<n; i++ ) {
		if( close_enough( x[i], fill_value ))
			return( fill_value );
		sum += x[i];
		}

	sum = sum / (double)n;
	return( sum );
}

/******************************************************************************/
	int
equivalent_FDBs( NCVar *v1, NCVar *v2 )
{
	FDBlist *f1, *f2;

	f1 = v1->first_file;
	f2 = v2->first_file;
	while( f1 != NULL ) {
		if( f2 == NULL ) {
			return(0); /* files differ */
			}
		if( f1->id != f2->id ) {
			return(0); /* files differ */
			}
		f1 = f1->next;
		f2 = f2->next;
		}

	/* Here f1 == NULL */
	if( f2 != NULL ) {
		return(0); /* files differ */
		}

	return(1);
}

/******************************************************************************/
	void
copy_info_to_identical_dims( NCVar *vsrc, NCDim *dsrc, size_t dim_len )
{
	NCVar	*v;
	int	i, dims_are_same;
	NCDim	*d;
	size_t	j;

	v = variables;
	while( v != NULL ) {
		for( i=0; i<v->n_dims; i++ ) {
			d = *(v->dim+i);
			if( (d != NULL) && (d->have_calc_minmax == 0)) {
				/* See if this dim is same as passed dim */
				dims_are_same = (strcmp( dsrc->name, d->name ) == 0 ) &&
						equivalent_FDBs( vsrc, v );
				if( dims_are_same ) {
					if( options.debug ) 
						fprintf( stderr, "Dim %s (%d) is same as dim %s (%d), copying min&max from former to latter...\n", dsrc->name, dsrc->global_id, d->name, d->global_id );
					d->min = dsrc->min;
					d->max = dsrc->max;
					d->have_calc_minmax = 1;
					d->values = (float *)malloc(dim_len*sizeof(float));
					for( j=0L; j<dim_len; j++ )
						*(d->values + j) = *(dsrc->values + j);
					d->is_lat = dsrc->is_lat;
					d->is_lon = dsrc->is_lon;
					}
				}
			}
		v = v->next;
		}
}

/******************************************************************************
 * Calculate the minimum and maximum values in the dimension structs.  While
 * we are messing with the dims, we also try to determine if they are lat and
 * lon.
 */
	void
calc_dim_minmaxes( void )
{
	int	i, j;
	NCVar	*v;
	NCDim	*d;
	char	temp_str[1024];
	nc_type	type;
	double	temp_double, bounds_max, bounds_min;
	int	has_bounds, name_lat, name_lon, units_lat, units_lon;
	size_t	dim_len;

	v = variables;
	while( v != NULL ) {
		for( i=0; i<v->n_dims; i++ ) {
			d = *(v->dim+i);
			if( (d != NULL) && (d->have_calc_minmax == 0)) {
				if( options.debug ) 
					fprintf( stderr, "...min & maxes for dim %s (%d)...\n", d->name, d->global_id );
				dim_len = *(v->size+i);
				d->values = (float *)malloc(dim_len*sizeof(float));
				type = fi_dim_value( v, i, 0L, &temp_double, temp_str, &has_bounds, &bounds_min, &bounds_max );
				if( type == NC_DOUBLE ) {
					for( j=0; j<dim_len; j++ ) {
						type = fi_dim_value( v, i, j, &temp_double, temp_str, &has_bounds, &bounds_min, &bounds_max );
						*(d->values+j) = (float)temp_double;
						}
					d->min  = *(d->values);
					d->max  = *(d->values + dim_len - 1);
					}
				else
					{
					if( options.debug ) 
						fprintf( stderr, "**Note: non-float dim found; i=%d\n", i );
					d->min  = 1.0;
					d->max  = (float)dim_len;
					for( j=0; j<dim_len; j++ )
						*(d->values+j) = (float)j;
					}
				d->have_calc_minmax = 1;
				
				/* Try to see if the dim is a lat or lon.  Not an exact science by a long shot */
				name_lat  = strncmp_nocase(d->name,  "lat",    3)==0;
				units_lat = strncmp_nocase(d->units, "degree", 6) == 0;
				name_lon  = strncmp_nocase(d->name,  "lon",    3)==0;
				units_lon = strncmp_nocase(d->units, "degree", 6) == 0;
				d->is_lat = ((name_lat || units_lat) && (d->max <  90.01) && (d->min > -90.01));
				d->is_lon = ((name_lon || units_lon) && (d->max < 360.01) && (d->min > -180.01));

				/* There is a funny thing we need to do at this point.  Think about the following case.
				 * We want to look at 3 different files, and they all have a dim named 'lon' in them,
				 * and each is different.  Because this might happen, we can't use the name as an
				 * indication of a unique dimension.  On the other hand, it is very slow to repeatedly
				 * reprocess the same dim over and over, especially if it's the time dim in a series
				 * of virtually concatenated input files.  For that reason, we copy the min and max
				 * values we just found to all identical dims.
				 */
				copy_info_to_identical_dims( v, d, dim_len );
				}
			}
		v = v->next;
		}
}
	
/********************************************************************************
 * Actually do the "shrinking" of the FLOATING POINT (not pixel) data, converting 
 * it to the small version by either finding the most common value in the square,
 * or by averaging over the square.  Remember that our standard for how to 
 * interpret 'options.blowup' is that a value of "-N" means to shrink by a factor
 * of N.  So, blowup == -2 means make it half size, -3 means 1/3 size, etc.
 */
	void
contract_data( float *small_data, View *v, float fill_value )
{
	long 	i, j, n, nx, ny, ii, jj;
	size_t	new_nx, new_ny, idx, ioffset, joffset;
	float 	*tmpv;

	if( options.blowup > 0 ) {
		fprintf( stderr, "internal error, contract_data called with a positive blowup factor!\n" );
		exit(-1);
		}

	n = -options.blowup;
	tmpv = (float *)malloc( n*n * sizeof(float) );
	if( tmpv == NULL ) {
		fprintf( stderr, "internal error, failed to allocate array for calculating reduced means\n" );
		exit( -1 );
		}

	/* Get old and new sizes (new size is smaller in this routine) */
	nx   = *(v->variable->size + v->x_axis_id);
	ny   = *(v->variable->size + v->y_axis_id);
	view_get_scaled_size( options.blowup, nx, ny, &new_nx, &new_ny );

	for( j=0; j<new_ny; j++ )
	for( i=0; i<new_nx; i++ ) {
		for( jj=0; jj<n; jj++ )
		for( ii=0; ii<n; ii++ ) {
			ioffset = i*n + ii;
			joffset = j*n + jj;
			if( ioffset >= nx )
				ioffset = nx-1;
			if( joffset >= ny )
				joffset = ny-1;
			idx = ioffset + joffset*nx;
			tmpv[ii + jj*n] = *((float *)v->data + idx);
			}

		if( options.shrink_method == SHRINK_METHOD_MEAN )
			small_data[i + j*new_nx] = util_mean( tmpv, n*n, fill_value );

		else if( options.shrink_method == SHRINK_METHOD_MODE ) {
			small_data[i + j*new_nx] = util_mode( tmpv, n*n, fill_value );
			}
		else
			{
			fprintf( stderr, "Error in contract_data: unknown value of options.shrink_method!\n" );
			exit( -1 );
			}
		}


	free(tmpv);
}

/******************************************************************************
 * Actually do the "blowup" of the FLOATING POINT (not pixel) data, converting 
 * it to the large version by either interpolation or replication.
 * NOTE this routine is only called when options.blowup > 0!
 */
	void
expand_data( float *big_data, View *v )
{
	size_t	to_width, x_size, y_size;
	long	line, i, j, i2, j2;
	int	blowup, blowupsq;
	float	step;
	float	base_val, right_val, below_val, val, bupr;
	float	base_x, base_y, del_x, del_y;
	float	est1, est2, frac_x, frac_y;
	float 	fill_val;

	fill_val = v->variable->fill_value;
	x_size   = *(v->variable->size + v->x_axis_id);
	y_size   = *(v->variable->size + v->y_axis_id);
	
	blowup   = options.blowup;
	blowupsq = blowup * blowup;
	to_width = blowup * x_size;
	if( (blowupsq < blowup) || (to_width < blowup) ) {
		fprintf( stderr, "ncview: data_to_pixels: too much magnification\n" );
		fprintf( stderr, "blowup=%d  blowupsq=%d  to_width=%ld\n",
				blowup, blowupsq, to_width );
		exit( -1 );
		}

	if( options.blowup_type == BLOWUP_REPLICATE ) { 
		for( j=0; j<y_size; j++ ) {
			for( i=0; i<x_size; i++ )
				for( i2=0; i2<blowup; i2++ )
					*(big_data + i*blowup + j*to_width*blowup + i2) = *((float *)((float *)v->data)+i+j*x_size);
			for( line=1; line<blowup; line++ )
				for( i2=0; i2<to_width; i2++ )
					*(big_data + i2 + j*to_width*blowup + line*to_width) =
						*(big_data + i2 + j*to_width*blowup);
			}
		} 
	else 	{ /* BLOWUP_BILINEAR */
		bupr = 1.0/(float)blowup;

		/* Horizontal base lines */
		for( j=0; j<y_size; j++ )
		for( i=0; i<x_size-1; i++ ) {
			base_val  = *((float *)v->data + i   + j*x_size);
			right_val = *((float *)v->data + i+1 + j*x_size);

			if( close_enough(base_val,  fill_val) || 
			    close_enough(right_val, fill_val))
				step = 0.0;
			else
				step = (right_val-base_val)*bupr;
			val = base_val;

			for( i2=0; i2 < blowup; i2++ ) {
				*(big_data + i*blowup+i2 + j*x_size*blowupsq ) = val;
				val += step;
				}
			}

		/* Vertical base lines */
		for( j=0; j<y_size-1; j++ )
		for( i=0; i<x_size; i++ ) {
			base_val  = *((float *)v->data + i   + j*x_size);
			below_val = *((float *)v->data + i + (j+1)*x_size);
			if( close_enough(base_val,  fill_val) || 
			    close_enough(below_val, fill_val))
				step = 0.0;
			else
				step = (below_val-base_val)*bupr;
			val = base_val;

			for( j2=0; j2 < blowup; j2++ ) {
				*(big_data + i*blowup + j*to_width*blowup + j2*to_width ) = val;
				val += step;
				}
			}
		/* Now, fill in the interior of the square by 
		 * interpolating from the horizontal and vertical
		 * base lines.
		 */
		for( j=0; j<y_size-1; j++ )
		for( i=0; i<x_size-1; i++ ) {
			for( j2=1; j2<blowup; j2++ )
			for( i2=1; i2<blowup; i2++ ) {
				frac_x = (float)i2*bupr;
				frac_y = (float)j2*bupr;
				base_x = *(big_data + i*blowup + j*to_width*blowup+j2*to_width);
				right_val = *(big_data + (i+1)*blowup + j*to_width*blowup+ j2*to_width);
				base_y = *(big_data + i*blowup+i2 + j*to_width*blowup);
				below_val = *(big_data + i*blowup+i2 + (j+1)*to_width*blowup);

				if( close_enough(base_x,    fill_val) || 
				    close_enough(right_val, fill_val) || 
				    (i == x_size-1) )
					del_x = 0.0;
				else
					del_x  = right_val - base_x;
				if( close_enough(base_y,    fill_val) || 
				    close_enough(below_val, fill_val) || 
				    (j == y_size-1) )
					del_y = 0.0;
				else
					del_y  = below_val - base_y;
				est1 = frac_x*del_x + base_x;
				est2 = frac_y*del_y + base_y;
				*(big_data + i*blowup+i2 + j*to_width*blowup + j2*to_width ) =
					(est1 + est2)*.5;
				}
			}
		/* Fill in right hand side */
		for( j=0; j<blowup*y_size; j++ )
		for( i=0; i<blowup; i++ )
			*(big_data + (x_size-1)*blowup+i + j*to_width ) = fill_val;

		/* Fill in bottom */
		for( j=0; j<blowup; j++ )
		for( i=0; i<blowup*x_size; i++ )
			*(big_data + i + (y_size-1)*to_width*blowup + j*to_width ) = fill_val;
		}
}

/******************************************************************************
 * Set the style of blowup we want to do.
 */
	void
set_blowup_type( int new_type )
{
	if( new_type == BLOWUP_REPLICATE ) 
		in_set_label( LABEL_BLOWUP_TYPE, "Repl"   );
	else
		in_set_label( LABEL_BLOWUP_TYPE, "Bi-lin" );

	options.blowup_type = new_type;
}

/******************************************************************************
 * If we allowed strings of arbitrary length, some of the widgets
 * would crash when trying to display them.
 */
	char *
limit_string( char *s )
{
	int	i;

	i = strlen(s)-1;
	while( *(s+i) == ' ' )
		i--;
	*(s+i+1) = '\0';

	if( strlen(s) > MAX_DISPLAYED_STRING_LENGTH )
		*(s+MAX_DISPLAYED_STRING_LENGTH) = '\0';

	return( s );
}

/******************************************************************************
 * If we try to print to an already existing file, then warn the user
 * before clobbering it.
 */
	int
warn_if_file_exits( char *fname )
{
	int	retval;
	FILE	*f;
	char	message[1024];

	if( (f = fopen(fname, "r")) == NULL )
		return( MESSAGE_OK );
	fclose(f);

	sprintf( message, "OK to overwrite existing file %s?\n", fname );
	retval = in_dialog( message, NULL, TRUE );
	return( retval );
}

/******************************************************************************/

	static void
handle_time_dim( int fileid, NCVar *v, int dimid )
{
	NCDim   *d;

	d = *(v->dim+dimid);

	if( udu_utistime( d->name, d->units ) ) {
		d->timelike = 1;
		d->time_std = TSTD_UDUNITS;
		d->tgran    = udu_calc_tgran( fileid, v, dimid );
		}
	else if( epic_istime0( fileid, v, d )) {
		d->timelike = 1;
		d->time_std = TSTD_EPIC_0;
		d->tgran    = epic_calc_tgran( fileid, d );
		}
	else if( (d->units != NULL) &&
		 (strlen(d->units) >= 5) &&
		 (strncasecmp( d->units, "month", 5 ) == 0 ))  {
		d->timelike = 1;
		d->time_std = TSTD_MONTHS;
		d->tgran    = months_calc_tgran( fileid, d );
		}
	else
		d->timelike = 0;
}

/******************************************************************************/
	static int
months_calc_tgran( int fileid, NCDim *d )
{
	char	temp_string[128];
	float	delta, v0, v1;
	int	type, has_bounds;
	double	temp_double, bounds_min, bounds_max;

	if( d->size < 2 ) {
		return( TGRAN_DAY );
		}

	type = netcdf_dim_value( fileid, d->name, 0L, &temp_double, temp_string, 0L, &has_bounds, &bounds_min, &bounds_max );
	if( type == NC_DOUBLE )
		v0 = (float)temp_double;
	else
		{
		fprintf( stderr, "Note: can't calculate time granularity, unrecognized timevar type (%d)\n",
			type );
		return( TGRAN_DAY );
		}

	type = netcdf_dim_value( fileid, d->name, 1L, &temp_double, temp_string, 1L, &has_bounds, &bounds_min, &bounds_max );
	if( type == NC_DOUBLE )
		v1 = (float)temp_double;
	else
		{
		fprintf( stderr, "Note: can't calculate time granularity, unrecognized timevar type (%d)\n",
			type );
		return( TGRAN_DAY );
		}
	
	delta = v1 - v0;

	if( delta > 11.5 ) 
		return( TGRAN_YEAR );
	if( delta > .95 ) 
		return( TGRAN_MONTH );
	if( delta > .03 ) 
		return( TGRAN_DAY );

	return( TGRAN_MIN );
}

/******************************************************************************/
void fmt_time( char *temp_string, double new_dimval, NCDim *dim, int include_granularity )
{
	int 	year, month, day;

	if( ! dim->timelike ) {
		fprintf( stderr, "ncview: internal error: fmt_time called on non-timelike axis!\n");
		fprintf( stderr, "dim name: %s\n", dim->name );
		exit( -1 );
		}

	if( dim->time_std == TSTD_UDUNITS ) 
		udu_fmt_time( temp_string, new_dimval, dim, include_granularity );

	else if( dim->time_std == TSTD_EPIC_0 ) 
		epic_fmt_time( temp_string, new_dimval, dim );

	else if( dim->time_std == TSTD_MONTHS ) {
		/* Format for months standard */
		year  = (int)( (new_dimval-1.0) / 12.0 );
		month = (int)( (new_dimval-1.0) - year*12 + .01 );
		month = (month < 0) ? 0 : month;
		month = (month > 11) ? 11 : month;
		day   =
		   (int)( ((new_dimval-1.0) - year*12 - month) * 30.0) + 1;
		sprintf( temp_string, "%s %2d %4d", month_name[month],
				day, year+1 );
		}

	else
		{
		fprintf( stderr, "Internal error: uncaught value of tim_std=%d\n", dim->time_std );
		exit( -1 );
		}
}

/*********************************************************************************************
 * like strncmp, but ignoring case
 */
	int
strncmp_nocase( char *s1, char *s2, size_t n )
{
	char 	*s1_lc, *s2_lc;
	int	i, retval;

	if( (s1==NULL) || (s2==NULL))
		return(-1);

	s1_lc = (char *)malloc(strlen(s1)+1);
	s2_lc = (char *)malloc(strlen(s2)+1);

	for( i=0; i<strlen(s1); i++ )
		s1_lc[i] = tolower(s1[i]);
	s1_lc[i] = '\0';
	for( i=0; i<strlen(s2); i++ )
		s2_lc[i] = tolower(s2[i]);
	s2_lc[i] = '\0';

	retval = strncmp( s1_lc, s2_lc, n );

	free(s1_lc);
	free(s2_lc);

	return(retval);
}

