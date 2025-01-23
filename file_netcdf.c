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

/*****************************************************************************
 *
 *	The netcdf file interface to ncview.  I.e., routines to interface
 *	netCDF format files to the ncview program.  For descriptions, see
 *	the generalized interface routines in 'file.c'.
 *
 *****************************************************************************/

#include "ncview.includes.h"
#include "ncview.defines.h"
#include "ncview.protos.h"

extern	NCVar   *variables;
extern  Options options;
extern  char *defvar;
extern  int iUseVar; 
extern  int iUseVarLength;

void warn_about_char_dims();
int safe_ncdimid( int fileid, char *dim_name1 );
int netcdf_dimvar_id( int fileid, char *dim_name );
int get_att_util( int id, int varid, char *var_name, char *att_name, int expected_len, void *value );

char *nc_type_to_string( nc_type type );

/*******************************************************************************************/
int netcdf_fi_confirm( char *name )
{
	int	fd;

	if( (fd = ncopen( name, NC_NOWRITE )) < 0 )
		return( FALSE );

	ncclose( fd );
	return ( TRUE );
}

/*******************************************************************************************/
int netcdf_fi_writable( char *name )
{
	int	fd;

	fd = ncopen( name, NC_WRITE );
	ncclose( fd );

	if( fd < 0 )
		return( FALSE );
	else
		return( TRUE );
}

/*******************************************************************************************/
int netcdf_fi_initialize( char *name )
{
	int	cdfid;

	cdfid = ncopen( name, NC_NOWRITE );

	if( cdfid < 0 ) {
		fprintf( stderr, "fi_initialize: can't properly open file %s\n",
			name );
		exit( -1 );
		}

	return( cdfid );
}
		
/*******************************************************************************************/
int netcdf_fi_recdim_id( int fileid )
{
	int	n_vars, err, n_dims, n_gatts, rec_dim;

	err = ncinquire( fileid, &n_dims, &n_vars, &n_gatts, &rec_dim );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_fi_list_vars: error on ncinqire, cdfid=%d\n", fileid );
		exit( -1 );
		}
	return( rec_dim );
}

/*******************************************************************************************/
Stringlist *netcdf_fi_list_vars( int fileid )
{
	int	n_vars, err, i, jj, n_dims, n_var_dims, eff_ndims;
	char	*var_name;
      char  var_name1[16];
	Stringlist *ret_val, *dimlist;
	int	n_gatts, rec_dim;
	size_t	*size, total_size;

	ret_val = NULL;

	err = ncinquire( fileid, &n_dims, &n_vars, &n_gatts, &rec_dim );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_fi_list_vars: error on ncinqire, cdfid=%d\n", fileid );
		exit( -1 );
		}

	/* Here is where we set the requirements for a variable to appear
	 * as a displayable variable.  At present, we require: 1) that the
	 * variable have at least 1 scannable dimensions. 2) It shouldn't
	 * be a "dimension variable"; i.e., there should be no dimension
	 * with the same name as this variable. 3) It's total size should
	 * be > 1.
	 */

	for( i=0; i<n_vars; i++ ) {
		var_name = netcdf_varindex_to_name( fileid, i );
            strcpy(var_name1,var_name);
            var_name1[iUseVarLength]=NULL;
            
            if ( iUseVar==0 || (iUseVar==1 && strcmp(var_name1, defvar)==0) ) { 
		  if( netcdf_dim_name_to_id( fileid, var_name, var_name ) == -1 ){
			/* then it's NOT a dimension variable */
			size = netcdf_fi_var_size( fileid, var_name );
			n_var_dims = netcdf_fi_n_dims( fileid, var_name );
			total_size = 1L;
			eff_ndims  = 0;
			for(jj=0; jj<n_var_dims; jj++ ) {
				total_size *= *(size+jj);
				if( *(size+jj) > 1 ) 
					eff_ndims++;
				}
			dimlist  = fi_scannable_dims( fileid, var_name );
			if( (total_size > 1L) && (n_strings_in_list( dimlist ) >= 1))
				/* Hack to make version 1.70+ emulate older versions
				 * that did not display 1-d vars.
				 */
				if( ! (options.no_1d_vars && (eff_ndims == 1) ))
					add_to_stringlist( &ret_val, var_name, NULL );
			}
		}
        }
	
	return( ret_val );
}

/*******************************************************************************************/
Stringlist *netcdf_scannable_dims( int fileid, char *var_name )
{
	int	var_id, n_dims, i, err;
	char	*dim_name;
	size_t	dim_size;
	Stringlist *dimlist = NULL;
	int	n_atts, dim[MAX_VAR_DIMS];
	nc_type	var_type;

	dim_name = (char *)malloc( MAX_NC_NAME ); /* defined in netcdf.h */

	err = nc_inq_varid( fileid, var_name, &var_id );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_scannable_dims: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}
	ncvarinq( fileid, var_id, var_name, &var_type, &n_dims, dim, &n_atts );

	for( i=0; i<n_dims; i++ ) {
		err = nc_inq_dim( fileid, *(dim+i), dim_name, &dim_size );
		if( err < 0 ) {
			fprintf( stderr, "ncview: netcdf_scannable_dims: ");
			fprintf( stderr, "error on ncdiminq call\n" );
			fprintf( stderr, "fileid=%d, variable name=%s\n",
					fileid, var_name );
			exit( -1 );
			}
		/* Here is where we set the requirements for a "scannable"
		 * dimension.  For a netcdf file, it makes sense to pick
		 * the first dimension (which is, by convention in netCDF files,
		 * the 'time' dimension if it exists) and otherwise pick
		 * dimensions which are greater in size than some cutoff.  Here,
		 * the cutoff is just one so that 'layer' can be picked up
		 * in layer models -- typically just 1 to 2 in that case.
		 */
		if( (i == 0) || (dim_size > 1) )
			add_to_stringlist( &dimlist, dim_name, NULL );
		}

	return( dimlist );
}

/*******************************************************************************************/
int netcdf_fi_n_dims( int fileid, char *var_name )
{
	int	n_dims, err, varid;
	int	n_atts, dim[MAX_VAR_DIMS];
	nc_type	var_type;

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_fi_n_dims: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}
	err   = ncvarinq( fileid, varid, var_name, &var_type, &n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_fi_n_dims: error on ncvarinq\n" );
		fprintf( stderr, "netcdfid=%d, var_name=%s\n",
			fileid, var_name );
		exit( -1 );
		}
	return( n_dims );
}

/*******************************************************************************************/
size_t netcdf_dim_size( fileid, dimid )
{
	size_t	ret_val;
	int	err;

	err = nc_inq_dimlen( fileid, dimid, &ret_val );
	if( err != NC_NOERR ) {
		fprintf( stderr, "netcdf_dim_size: failed on nc_inq_dimlen call!\n" );
		exit(-1);
		}
	return( ret_val );
}

/*******************************************************************************************/
size_t * netcdf_fi_var_size( int fileid, char *var_name )
{
	int	n_dims, varid, err, i;
	size_t	*ret_val, dim_size;
	int	n_atts, dim[MAX_VAR_DIMS];
	nc_type var_type;

	n_dims  = netcdf_fi_n_dims( fileid, var_name );
	ret_val = (size_t *)malloc( n_dims * sizeof(size_t) );

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_fi_var_size: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	err = ncvarinq( fileid, varid, var_name, &var_type, &n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_fi_var_size: error on ncvarinq\n" );
		fprintf( stderr, "netcdfid=%d, var_name=%s\n",
			fileid, var_name );
		exit( -1 );
		}

	for( i=0; i<n_dims; i++ ) {
		err = nc_inq_dimlen( fileid, *(dim+i), &dim_size );
		*(ret_val+i) = dim_size;
		}

	return( ret_val );
}

/*******************************************************************************************/
char *netcdf_dim_id_to_name( int fileid, char *var_name, int dim_id )
{
	int	netcdf_dim_id, netcdf_var_id;
	int	n_dims, *dim, err, n_atts;
	char	*dim_name;
	nc_type	var_type;

	/* see notes under "netcdf_dim_name_to_id".  "dim_id" is NOT
	 * the netCDF dimension ID, it is the entry into the size array
	 * for the passed variable.
	 */
	err = nc_inq_varid( fileid, var_name, &netcdf_var_id );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_dim_id_to_name: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}
	n_dims        = fi_n_dims( fileid, var_name );
	dim           = (int *)malloc( n_dims * sizeof( int ));
	err           = ncvarinq( fileid, netcdf_var_id, var_name, &var_type,
				&n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "ncview: netcdf_dim_id_to_name: error on ");
		fprintf( stderr, "ncvarinq call.  Variable=%s\n", var_name );
		exit( -1 );
		}

	netcdf_dim_id = *(dim+dim_id);
	dim_name = (char *)malloc( MAX_NC_NAME ); /* defined in netcdf.h */
	err      = nc_inq_dimname( fileid, netcdf_dim_id, dim_name );
	if( err < 0 ) {
		fprintf( stderr, "ncview: netcdf_dim_id_to_name: error on ");
		fprintf( stderr, "ncdiminq call.  Variable=%s\n", var_name );
		exit( -1 );
		}
	return( dim_name );
}

/*******************************************************************************************/
int netcdf_dim_name_to_id( int fileid, char *var_name, char *dim_name )
{
	int	netcdf_dim_id, netcdf_var_id, n_dims, *dim, err, i, n_atts;
	nc_type	var_type;

	/* It is important to note that this routine does NOT return
	 * the dimension ID of the passed dimension.  That concept is
	 * too netCDF specific.  It returns the dimension's index into 
	 * the size and count arrays, for this particular variable. 
	 * Thus calling this routine with the same dimension name, but
	 * for different variables, will in general give different
	 * return values.  Returns -1 if the dimension is not found
	 * in the requested variable.
	 */

	netcdf_dim_id = safe_ncdimid( fileid, dim_name );
	if( netcdf_dim_id == -1 )
		return( -1 );
	err = nc_inq_varid( fileid, var_name, &netcdf_var_id );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_dim_name_to_id: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	n_dims = fi_n_dims( fileid, var_name );
	dim    = (int *)malloc( n_dims * sizeof( int ));
	err    = ncvarinq( fileid, netcdf_var_id, var_name, &var_type,
				&n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "ncview: netcdf_dim_name_to_id: error on ");
		fprintf( stderr, "ncvarinq call.  Variable %s, Dimension %s\n",
					var_name, dim_name );
		exit( -1 );
		}

	for( i=0; i<n_dims; i++ )
		if( *(dim+i) == netcdf_dim_id )
			return( i );

	return( -1 );
}

/*******************************************************************************************/
void netcdf_fi_get_data( int fileid, char *var_name, size_t *start_pos, 
		size_t *count, float *data, NetCDFOptions *aux_data )
{
	int	i, err, varid;
	size_t	tot_size, n_dims;

	tot_size = 1L;
	n_dims = netcdf_fi_n_dims( fileid, var_name );
	for( i=0; i<n_dims; i++ )
		tot_size *= *(count+i);

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_fi_get_data: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	if( options.debug ) {
		fprintf( stderr, "About to call nc_get_vara_float on variable %s\n",
				var_name );
		fprintf( stderr, "Index, start, count:\n" );
		for( i=0; i<netcdf_fi_n_dims(fileid, var_name); i++ )
			fprintf( stderr, "[%d]: %ld %ld\n", i, *(start_pos+i), *(count+i) );
		}

	err = nc_get_vara_float( fileid, varid, start_pos, count, data );
	if( err != NC_NOERR ) {
		fprintf( stderr, "netcdf_fi_get_data: error on nc_get_vara_float call\n" );
		fprintf( stderr, "cdfid=%d   variable=%s\n", fileid, var_name );
		fprintf( stderr, "start, count:\n" );
		for( i=0; i<netcdf_fi_n_dims(fileid, var_name); i++ )
			fprintf( stderr, "[%1d]: %ld  %ld\n", 
				i, *(start_pos+i), *(count+i) );
		fprintf( stderr, "%s\n", nc_strerror(err) );
		exit( -1 );
		}

	/* Eliminate nans */
        for( i=0L; i<tot_size; i++ ) {
		if( isnan(data[i]))
			data[i] = FILL_FLOAT;
		}

#ifdef ELIM_DENORMS
        /* Eliminate denormalized numbers and NaNs */
	n_nans = 0L;
        for( i=0L; i<tot_size; i++ ) {
                c = (unsigned char *)&(data[i]);
                if((*(c+3)==0) && ((*(c+0)!=0)||(*(c+1)!=0)||(*(c+2)!=0))) {
                        fprintf( stderr,
                          "Denormalized number in position %ld: Setting to zero!\n",
                          i, data[i] );
                        data[i] = 0.0;
                        }
		/* this reports bogus nans, for example on file
		 * /cirrus06/users/pierce/hcm_t3p0/sst_even_anoms.HOPE.NMC.65-02.nc
                if((*(c+0)==255) && ((*(c+1)==255)||(*(c+2)==103)||(*(c+3)==63))) {
			n_nans++;
                        data[i] = 1.0e30;
                        }
		*/
                }
	if( n_nans > 0 ) 
		fprintf( stderr, "Found %ld NaNs: Set to 1.e30!\n", n_nans );
        /*
	  for( i=0L; i<tot_size; i++ ) {
                c = (unsigned char *)&(data[i]);
		i0 = *(c+0);
		i1 = *(c+1);
		i2 = *(c+2);
		i3 = *(c+3);
		printf( "%x %x %x %x %f\n", i0, i1, i2, i3, data[i] );
		}
	*/
#endif

	/* Implement the "add_offset" and "scale_factor" attributes */
	if( aux_data->add_offset_set && aux_data->scale_factor_set )
		for( i=0; i<tot_size; i++ )
			*(data+i) = *(data+i) * aux_data->scale_factor 
					+ aux_data->add_offset;
	else if( aux_data->add_offset_set )
		for( i=0; i<tot_size; i++ )
			*(data+i) = *(data+i) + aux_data->add_offset;
	else if( aux_data->scale_factor_set ) 
		for( i=0; i<tot_size; i++ )
			*(data+i) = *(data+i) * aux_data->scale_factor;

	if( options.debug ) 
		fprintf( stderr, "returning from netcdf_fi_get_data\n" );
}

/*******************************************************************************************/
void netcdf_fi_close( int fileid )
{
	int	err;

	err = ncclose( fileid );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_fi_close: error on ncclose\n" );
		exit( -1 );
		}
}

/****************************************************************************************/
/* netCDF utility routines.  Analogs are not required for each data file format.	*/
/****************************************************************************************/

/*******************************************************************************************/
/* How many dimensions does this variable have? 
*/
int netcdf_n_dims( int cdfid, char *varname )
{
	int	varid, err, n_dims;
	char 	var_name[MAX_NC_NAME];	
	nc_type	var_type;
	int	n_atts, dim[MAX_VAR_DIMS];

	err = nc_inq_varid( cdfid, varname, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_n_dims: could not find var named \"%s\" in file!\n",
			varname );
		exit(-1);
		}

	err = ncvarinq( cdfid, varid, var_name, &var_type, &n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_n_dims: error calling ncvarinq for cdfid=%d, ", 
					cdfid);
		fprintf( stderr, "varname=%s\n", varname );
		exit( -1 );
		}

	return( n_dims );
}

/*******************************************************************************************/
/* What type of variable is this? 
*/
int netcdf_vartype( int cdfid, char *varname )
{
	int	varid, err, n_dims;
	char 	var_name[MAX_NC_NAME];	
	nc_type	var_type;
	int	n_atts, dim[MAX_VAR_DIMS];

	err = nc_inq_varid( cdfid, varname, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_vartype: could not find var named \"%s\" in file!\n",
			varname );
		exit(-1);
		}

	err = ncvarinq( cdfid, varid, var_name, &var_type, &n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_n_dims: error calling ncvarinq for cdfid=%d, ", 
					cdfid);
		fprintf( stderr, "varname=%s\n", varname );
		exit( -1 );
		}

	return( var_type );
}

/*******************************************************************************************/
/* Given the variable INDEX, what is the variable's name? 
*/
char *netcdf_varindex_to_name( int cdfid, int index )
{
	char	*var_name;
	int	err;
	nc_type	var_type;
	int	n_dims, n_atts, dim[MAX_VAR_DIMS];

	if( (var_name = (char *)malloc( MAX_NC_NAME )) == NULL ) {
		fprintf( stderr, "netcdf_varindex_to_name: couldn't allocate %d bytes\n",
			MAX_NC_NAME );
		exit( -1 );
		}

	err = ncvarinq( cdfid, index, var_name, &var_type, &n_dims, dim, &n_atts );
	if( err < 0 ) {
		fprintf( stderr, "netcdf_varindex_to_name: error on ncvarinq call\n" );
		exit( -1 );
		}

	return( var_name );
}

/*******************************************************************************************/
/* A 'safe' way to turn an attribute name into an attribute number.
 * If it returns -1, no attribute of that name exists for the given
 * variable (which might be NC_GLOBAL).
 */
int netcdf_att_id( int fileid, int varid, char *name )
{
	int	err, n_atts, i;
	char	att_name[MAX_NC_NAME], var_name[MAX_NC_NAME];
	nc_type	var_type;
	int	n_vars, n_dims, rec_dim, dim[MAX_VAR_DIMS];

	if( varid == NC_GLOBAL )
		ncinquire( fileid, &n_dims, &n_vars, &n_atts, &rec_dim );
	else
		ncvarinq( fileid, varid, var_name, &var_type, &n_dims, dim, &n_atts );

	for( i=0; i<n_atts; i++ ) {
		err = ncattname( fileid, varid, i, att_name );
		if( err == -1 ) {
			fprintf( stderr, "netcdf_att_id: failed on ncattname call!\n" );
			exit(-1);
			}
		if( strcmp( att_name, name ) == 0 )
			return( i );
		}
	return( -1 );
}

/*******************************************************************************************/
char * netcdf_title( int fileid )
{
	int	title_len, err, attid;
	char	*ret_val;
	nc_type	type;

	attid = netcdf_att_id( fileid, NC_GLOBAL, "title" );
	if( attid < 0 )
		return( NULL );

	err = ncattinq( fileid, NC_GLOBAL, "title", &type, &title_len );
	if( type != NC_CHAR ) {
		fprintf( stderr, "ncview: netcdf_title: internal error in the " );
		fprintf( stderr, "format of the netCDF input file; title\n" );
		fprintf( stderr, "not in character format!  Setting title to NULL.\n" );
		return( NULL );
		}
	if( err < 0 )
		return( NULL );

	ret_val = (char *)malloc( title_len+1 );
	err = ncattget( fileid, NC_GLOBAL, "title", ret_val );
	if( err < 0 )
		return( NULL );

	if( *(ret_val+title_len-1) != '\0' )
		*(ret_val + title_len) = '\0';
	return( ret_val );
}

/*******************************************************************************************/
char *netcdf_long_var_name( int fileid, char *var_name )
{
	int	varid, err, name_length;
	nc_type	type;
	char	*ret_val;

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_long_var_name: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	if( netcdf_att_id( fileid, varid, "long_name" ) < 0 )
		return( NULL );

	err = ncattinq( fileid, varid, "long_name", &type, &name_length );
	if( (err < 0) || (type != NC_CHAR))
		return( NULL );

	ret_val = (char *)malloc( name_length+1 );
	err = ncattget( fileid, varid, "long_name", ret_val );
	if( err < 0 )
		return( NULL );

	if( *(ret_val+name_length-1) != '\0' )
		*(ret_val + name_length) = '\0';

	return( ret_val );
}

/*******************************************************************************************/
char *netcdf_var_units( int fileid, char *var_name )
{
	int	varid, err, name_length;
	nc_type	type;
	char	*ret_val;

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_var_units: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	if( netcdf_att_id( fileid, varid, "units" ) < 0 )
		return( NULL );

	err = ncattinq( fileid, varid, "units", &type, &name_length );
	if( (err < 0) || (type != NC_CHAR))
		return( NULL );

	ret_val = (char *)malloc( name_length+1 );
	err = ncattget( fileid, varid, "units", ret_val );
	if( err < 0 )
		return( NULL );

	if( *(ret_val+name_length-1) != '\0' )
		*(ret_val + name_length) = '\0';

	return( ret_val );
}

/*******************************************************************************************/
char *netcdf_dim_calendar( int fileid, char *dim_name )
{
	int	err, name_length;
	nc_type	type;
	char	*ret_val;
	int	dimvar_id;

	dimvar_id = netcdf_dimvar_id( fileid, dim_name );

	if( dimvar_id < 0 )
		return( NULL );

	if( netcdf_att_id( fileid, dimvar_id, "calendar" ) < 0 )
		return( NULL );

	err = ncattinq( fileid, dimvar_id, "calendar", &type, &name_length );
	if( (err < 0) || (type != NC_CHAR))
		return( NULL );

	ret_val = (char *)malloc( name_length+1 );
	err = ncattget( fileid, dimvar_id, "calendar", ret_val );
	if( err < 0 )
		return( NULL );

	if( *(ret_val+name_length-1) != '\0' )
		*(ret_val + name_length) = '\0';

	return( ret_val );
}

/*******************************************************************************************/
char *netcdf_dim_units( int fileid, char *dim_name )
{
	int	err, name_length;
	nc_type	type;
	char	*ret_val;
	int	dimvar_id;

	dimvar_id = netcdf_dimvar_id( fileid, dim_name );

	if( dimvar_id < 0 )
		return( NULL );

	if( netcdf_att_id( fileid, dimvar_id, "units" ) < 0 )
		return( NULL );

	err = ncattinq( fileid, dimvar_id, "units", &type, &name_length );
	if( (err < 0) || (type != NC_CHAR))
		return( NULL );

	ret_val = (char *)malloc( name_length+1 );
	err = ncattget( fileid, dimvar_id, "units", ret_val );
	if( err < 0 )
		return( NULL );

	if( *(ret_val+name_length-1) != '\0' )
		*(ret_val + name_length) = '\0';

	return( ret_val );
}

/*******************************************************************************************/
int netcdf_dimvar_id( int fileid, char *dim_name )
{
	int	i, err, n_dims, n_vars, rec_dim;
	char	var_name[256];
	nc_type	var_type;
	int	n_atts, dim[MAX_VAR_DIMS];

	err = ncinquire( fileid, &n_dims, &n_vars, &n_atts, &rec_dim );
	if( err < 0 )
		return( -1 );

	for( i=0; i<n_vars; i++ ) {
		ncvarinq( fileid, i, var_name, &var_type, &n_dims, dim, &n_atts );
		if( strcmp( dim_name, var_name ) == 0 ) {
			if( (var_type == NC_CHAR) && (options.no_char_dims) )
				return( -1 );
			else
				return( i );
			}
		}

	return( -1 );
}

/*******************************************************************************************/
char *netcdf_dim_longname( int fileid, char *dim_name )
{
	int	dimvar_id, err, len;
	nc_type	att_type;
	char	*dim_longname;

	dimvar_id = netcdf_dimvar_id( fileid, dim_name );
	if( dimvar_id < 0 )
		return( dim_name );

	if( netcdf_att_id( fileid, dimvar_id, "long_name" ) < 0 )
		return( dim_name );

	err = ncattinq( fileid, dimvar_id, "long_name", &att_type, &len );
	if( err < 0 )
		return( dim_name );

	dim_longname = (char *)malloc( len+1 );
	err = ncattget( fileid, dimvar_id, "long_name", dim_longname );
	if( err < 0 )
		return( dim_name );
	else	
		{
		*(dim_longname + len) = '\0';
		return( dim_longname );
		}
}

/*******************************************************************************************/
/* A netCDF file has dim values if it has a *variable* with the same
 * name as the *dimension*.  This is a netCDF convention...which not
 * all datafiles might follow, of course.
 */
int netcdf_has_dim_values( int fileid, char *dim_name )
{
	int	dimvar_id;

	dimvar_id = netcdf_dimvar_id( fileid, dim_name );
	if( dimvar_id < 0 )
		return( FALSE );
	else
		return( TRUE );
}

/*******************************************************************************************/
/* Only one of the two possible returns, ret_val_double and ret_val_char, will
 * be filled out.  If the return value of the call is NC_CHAR, then ret_val_char
 * will have been filled out.  If the return value of the call is NC_DOUBLE, then
 * ret_val_double will have been filled out.
 */
nc_type netcdf_dim_value( int fileid, char *dim_name, size_t place, 
		double *ret_val_double, char *ret_val_char, size_t virt_place, 
		int *return_has_bounds, double *return_bounds_min, double *return_bounds_max )
{
	int	err, dimvar_id, nvertices;
	char	var_name[MAX_NC_NAME];
	nc_type type, ret_type;
	size_t	limit;
	long	i;
	size_t	char_place[2], bstart[2], bcount[2];
	int	n_dims, n_atts, dim[MAX_VAR_DIMS], dimvar_bounds_id;
	double	boundvals[50], boundvals_min, boundvals_max;

	if( ! netcdf_has_dim_values( fileid, dim_name ) ) {
		*ret_val_double = (double)virt_place;
		return( NC_DOUBLE );
		}

	dimvar_id = netcdf_dimvar_id( fileid, dim_name );
	if( dimvar_id < 0 ) {
		*ret_val_double = (double)virt_place;
		return( NC_DOUBLE );
		}

	err = ncvarinq( fileid, dimvar_id, var_name, &type, &n_dims, dim, &n_atts );
	if( err == -1 ) {
		fprintf( stderr, "netcdf_dim_value: failed on ncvarinq call!\n" );
		exit(-1);
		}
	switch( type ) {
		case NC_CHAR:
			/* this one is really complicated because the netCDF standard
			 * doesn't have the concept of "strings".  So, each individual
			 * is left to decide for theirself how to handle it.  Some 
			 * reasonable ways of doing it are either as null-terminated
			 * C strings or as non-terminated assemblages of fortran 
			 * characters.  You could specify the length of fortran strings
			 * by an additional dimension to the dimvar.
			 */
			warn_about_char_dims();
			ret_type = NC_CHAR;
			if( n_dims == 2 ) 
				limit = netcdf_dim_size( fileid, dim[1] );
			else
				limit = 1024;
			i = 0L;
			char_place[0] = place;
			do	{
				char_place[1] = i;
				err = nc_get_var1_schar( fileid, dimvar_id, char_place, (ret_val_char+i));
				i++;
				}
			while
				((i < limit) &&
					(*(ret_val_char+i-1) != '\0'));
			if( *(ret_val_char+i-1) != '\0')
				*(ret_val_char+i-1) = '\0';
			break;

		case NC_BYTE:
		case NC_SHORT:
		case NC_LONG:
		case NC_FLOAT:
		case NC_DOUBLE:

			/* If we have a 'bounds' attribute for the dimvar, returned the value
			 * centered between the boundaries.  Some files have the dim value NOT
			 * centered between the boundaries, which isn't so useful.
			 */
			dimvar_bounds_id = netcdf_dimvar_bounds_id( fileid, dim_name, &nvertices );
			if( dimvar_bounds_id < 0 ) { 

				*return_has_bounds = 0;
				err = nc_get_var1_double( fileid, dimvar_id, &place, ret_val_double );
#ifdef ELIM_DENORMS
				/* Eliminate denormalized numbers */
				c = (unsigned char *)ret_val_double;
				if((*(c+7)==0) && ((*(c+0)!=0)||(*(c+1)!=0)||(*(c+2)!=0)||(*(c+3)!=0)||(*(c+4)!=0)||(*(c+5)!=0)||(*(c+6)!=0))) {
					fprintf( stderr,
					  "Denormalized number in dimvar %s, position %ld: Setting to zero!\n",
					  var_name, place );
					*ret_val_double = 0.0;
					}
#endif
				}
			else
				{
				*return_has_bounds = nvertices;
				/* OK, have a bounds dimvar here, read it and compute mean
				 * to get the value to return.
				 */
				if( nvertices > 50 ) {
					fprintf( stderr, "Error, compiled with max number of vertices for bounds var of 50!  But found a var with n=%d\n", 
						nvertices );
					exit(-1);
					}
				bstart[0] = place;
				bstart[1] = 0L;
				bcount[0] = 1L;
				bcount[1] = nvertices;
				err = nc_get_vara_double( fileid, dimvar_bounds_id, bstart, bcount, boundvals );
				if( err != NC_NOERR ) {	
					fprintf( stderr, "Error reading boundary dim values from file!\n" );
					fprintf( stderr, "%s\n", nc_strerror( err ) );
					exit(-1);
					}
				*ret_val_double = 0.0;
				boundvals_min = 1.e35;
				boundvals_max = -1.e35;
				for( i=0; i<nvertices; i++ ) {
#ifdef ELIM_DENORMS
					/* Eliminate denormalized numbers */
					c = (unsigned char *)boundvals[i];
					if((*(c+7)==0) && ((*(c+0)!=0)||(*(c+1)!=0)||(*(c+2)!=0)||(*(c+3)!=0)||(*(c+4)!=0)||(*(c+5)!=0)||(*(c+6)!=0))) {
						fprintf( stderr,
						  "Denormalized number in dimvar %s, position %ld: Setting to zero!\n",
						  var_name, place );
						boundvals[i] = 0.0;
						}
#endif
					*ret_val_double += boundvals[i];
					boundvals_min = (boundvals[i] < boundvals_min) ? boundvals[i] : boundvals_min;
					boundvals_max = (boundvals[i] > boundvals_max) ? boundvals[i] : boundvals_max;
					}
				*ret_val_double /= (double)nvertices;
				*return_bounds_min = boundvals_min;
				*return_bounds_max = boundvals_max;
				}
			ret_type = NC_DOUBLE;
			break;

		default:
			fprintf( stderr, "ncview: netcdf_dim_value: " );
			fprintf( stderr, "unknown data type (%d) for\n", type );
			fprintf( stderr, "dimension %s\n", dim_name );
			*ret_val_double = (double)virt_place;
			ret_type = NC_DOUBLE;
			break;
		}
	return( ret_type );
}

/*******************************************************************************************/
void netcdf_fill_aux_data( int id, char *var_name, FDBlist *fdb )
{
	int	err, varid, n_dims, dim[MAX_NC_DIMS], n_atts, unlimdimvar_id, recdim_id;
	char	dummy_var_name[ MAX_NC_NAME ], unlimdim_name[MAX_NC_NAME], *unlimdimvar_units;
	nc_type	type;
	NetCDFOptions *netcdf;

	netcdf = (NetCDFOptions *)(fdb->aux_data);

	err = nc_inq_varid( id, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_fill_aux_data: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	/* Record the recdim units in this file
	 */
	recdim_id = netcdf_fi_recdim_id( id );
	if( recdim_id == -1 ) {
		fdb->recdim_units = NULL;
		}
	else
		{
		/* Get NAME of the record dimension */
		err = nc_inq_dimname( id, recdim_id, unlimdim_name );
		if( err != 0 ) {
			fprintf( stderr, "Error in netcdf_fill_aux_data: could not get recdim name\n%s\n",
				nc_strerror( err ));
			exit(-1);
			}
		/* See if there is a variable with the same name */
		err = nc_inq_varid( id, unlimdim_name, &unlimdimvar_id );
		if( err != 0 ) 
			fdb->recdim_units = NULL;
		else
			{
			/* Get the units for the dimvar. Note: can be NULL */
			fdb->recdim_units = netcdf_var_units( id, unlimdim_name );
			}
		}

	err = ncvarinq( id, varid, dummy_var_name, &type, &n_dims, dim, &n_atts );
	if( err == -1 ) {
		fprintf( stderr, "netcdf_fill_aux_data: failed on ncvarinq call!\n" );
		exit(-1);
		}

	if( n_atts == 0 )
		return;

	netcdf->valid_range_set = 
	    get_att_util( id, varid, var_name, "valid_range",  2, netcdf->valid_range );
	netcdf->valid_min_set = 
	    get_att_util( id, varid, var_name, "valid_min",    1, &(netcdf->valid_min) );
	netcdf->valid_max_set = 
	    get_att_util( id, varid, var_name, "valid_max",    1, &(netcdf->valid_max) );
	netcdf->add_offset_set = 
	    get_att_util( id, varid, var_name, "add_offset",   1, &(netcdf->add_offset) );
	netcdf->scale_factor_set = 
	    get_att_util( id, varid, var_name, "scale_factor", 1, &(netcdf->scale_factor) );

	/* Special case: if we have add_offset and scale_factor attributes,
	 * then assume they apply to the valid range also.  Q: is this
	 * always true?  The netCDF specification doesn't really say. 
	 */
	if( netcdf->add_offset_set && netcdf->scale_factor_set ) {
		if( netcdf->valid_range_set ) {
			netcdf->valid_range[0] = netcdf->valid_range[0] * netcdf->scale_factor
					+ netcdf->add_offset;
			netcdf->valid_range[1] = netcdf->valid_range[1] * netcdf->scale_factor
					+ netcdf->add_offset;
			}
		else 
			{
			if( netcdf->valid_min_set ) {
				netcdf->valid_min = netcdf->valid_min * netcdf->scale_factor
					+ netcdf->add_offset;
				}
			if( netcdf->valid_max_set ) {
				netcdf->valid_max = netcdf->valid_max * netcdf->scale_factor
					+ netcdf->add_offset;
				}
			}
		}
	else if( netcdf->add_offset_set ) {
		if( netcdf->valid_range_set ) {
			netcdf->valid_range[0] = netcdf->valid_range[0] + netcdf->add_offset;
			netcdf->valid_range[1] = netcdf->valid_range[1] + netcdf->add_offset;
			}
		else 
			{
			if( netcdf->valid_min_set ) {
				netcdf->valid_min = netcdf->valid_min + netcdf->add_offset;
				}
			if( netcdf->valid_max_set ) {
				netcdf->valid_max = netcdf->valid_max + netcdf->add_offset;
				}
			}
		}
	else if( netcdf->scale_factor_set ) {
		if( netcdf->valid_range_set ) {
			netcdf->valid_range[0] = netcdf->valid_range[0] * netcdf->scale_factor;
			netcdf->valid_range[1] = netcdf->valid_range[1] * netcdf->scale_factor;
			}
		else 
			{
			if( netcdf->valid_min_set ) {
				netcdf->valid_min = netcdf->valid_min * netcdf->scale_factor;
				}
			if( netcdf->valid_max_set ) {
				netcdf->valid_max = netcdf->valid_max * netcdf->scale_factor;
				}
			}
		}
}

/*******************************************************************************************/
/* return TRUE if found and set the value, and FALSE otherwise */
int get_att_util( int id, int varid, char *var_name, char *att_name, int expected_len, void *value )
{
	int	len, i;
	nc_type	type;
	char	*char_att;
	short	*short_att, short_1;
	double	*double_att, double_1;
	nclong	*long_att, long_1;

	if( netcdf_att_id( id, varid, att_name ) >= 0 ) {
		ncattinq( id, varid, att_name, &type, &len );
		if( type != NC_FLOAT ) {
			switch( type ) {
				case NC_CHAR:	
					char_att = (char *)malloc( len+1 );
					ncattget( id, varid, att_name, char_att );
					sscanf( char_att, "%f", (float *)value );
					free(char_att);
					break;

				case NC_SHORT:
					short_att = (short *)malloc( len*sizeof(short) );
					ncattget( id, varid, att_name, short_att );
					for( i=0; i<len; i++ ) {
						short_1 = *(short_att+i);
						*((float *)value + i) = (float)short_1;
						}
					free(short_att);
					break;

				case NC_DOUBLE:
					double_att = (double *)malloc( len*sizeof(double));
					ncattget( id, varid, att_name, double_att );
					for( i=0; i<len; i++ ) {
						double_1 = *(double_att+i);
						*((float *)value + i) = (float)double_1;
						}
					free(double_att);
					break;

				case NC_LONG:
					long_att = (nclong *)malloc( len*sizeof(nclong));
					ncattget( id, varid, att_name, long_att );
					for( i=0; i<len; i++ ) {
						long_1 = *(long_att+i);
						*((float *)value + i) = (float)long_1;
						}
					free(long_att);
					break;
				default:	
					fprintf( stderr, "can't handle conversions from %s to FLOAT yet\n", nc_type_to_string( type ) );
				}
			}
		else if( len != expected_len ) {
			fprintf( stderr, "error in specification of \"%s\" attribute for\n", att_name);
			fprintf( stderr, "variable %s: %d values specified (should be %d)\n",
				var_name, len, expected_len );
			return( FALSE );
			}
		else
			ncattget( id, varid, att_name, value );
			return( TRUE );
		}
	else
		return( FALSE );
}

/*******************************************************************************************/
int netcdf_min_max_option_set( NCVar *var, float *ret_min, float *ret_max )
{
	FDBlist		*f;
	NetCDFOptions 	*netcdf;
	int		range_set = FALSE;
	float		min, max, t_min, t_max;

	min =  9.9e30;
	max = -9.9e30;

	f = var->first_file;
	while( f != NULL ) {
		netcdf = (NetCDFOptions *)(f->aux_data);
		if( netcdf->valid_range_set ) {
			range_set = TRUE;
			if( netcdf->valid_range[0] <  netcdf->valid_range[1] ) {
				t_min = netcdf->valid_range[0];
				t_max = netcdf->valid_range[1];
				}
			else
				{
				t_min = netcdf->valid_range[1];
				t_max = netcdf->valid_range[0];
				}
			min = (t_min < min) ? t_min : min;
			max = (t_max > max) ? t_max : max;
			}
		f = f->next;
		}

	if( range_set ) {
		*ret_min = min;
		*ret_max = max;
		}

	return( range_set );
}

/*******************************************************************************************/
int netcdf_min_option_set( NCVar *var, float *ret_min )
{
	FDBlist		*f;
	NetCDFOptions 	*netcdf;
	int		min_set = FALSE;
	float		min, t_min;

	min =  9.9e30;

	f = var->first_file;
	while( f != NULL ) {
		netcdf = (NetCDFOptions *)(f->aux_data);
		if( netcdf->valid_min_set ) {
			min_set = TRUE;
			t_min   = netcdf->valid_min;
			min     = (t_min < min) ? t_min : min;
			}
		f = f->next;
		}

	if( min_set )
		*ret_min = min;

	return( min_set );
}

/*******************************************************************************************/
int netcdf_max_option_set( NCVar *var, float *ret_max )
{
	FDBlist		*f;
	NetCDFOptions 	*netcdf;
	int		max_set = FALSE;
	float		max, t_max;

	max =  -9.9e30;

	f = var->first_file;
	while( f != NULL ) {
		netcdf = (NetCDFOptions *)(f->aux_data);
		if( netcdf->valid_max_set ) {
			max_set = TRUE;
			t_max   = netcdf->valid_max;
			max     = (t_max > max) ? t_max : max;
			}
		f = f->next;
		}

	if( max_set )
		*ret_max = max;

	return( max_set );
}

/*******************************************************************************************/
void netcdf_fill_value( int file_id, char *var_name, float *v, NetCDFOptions *aux_data )
{
	int	err, varid, foundit;

	if( options.debug ) 
		fprintf( stderr, "Checking %s for a missing value...\n",
				var_name );

	foundit = FALSE;
	err = nc_inq_varid( file_id, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_fill_value: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	if( get_att_util( file_id, varid, var_name, "missing_value", 1, v ) ) {
		if( options.debug )
			fprintf( stderr, "found a \"missing_value\" attribute=%g\n",
				*v );
		foundit = TRUE;
		}

	if( get_att_util( file_id, varid, var_name, "_FillValue", 1, v ) ) {
		if( options.debug )
			fprintf( stderr, "found a \"_FillValue\" attribute=%g\n",
				*v );
		foundit = TRUE;
		}

	/* Is there a global missing value? */
	if( get_att_util( file_id, NC_GLOBAL, var_name, "missing_value", 1, v ) ) {
		if( options.debug )
			fprintf( stderr, "found a \"missing_value\" attribute=%g\n",
				*v );
		foundit = TRUE;
		}

#ifdef ELIM_DENORMS
        c = (unsigned char *)v;
	if((*(c+0)==255) && ((*(c+1)==255)||(*(c+2)==103)||(*(c+3)==63))) {
		fprintf( stderr, "Missing value is a NaN! Setting to 1.e30\n" );
		*v = 1.e30;
		}
#endif

	if( foundit ) {
		/* Implement the "add_offset" and "scale_factor" attributes */
		if( aux_data->add_offset_set && aux_data->scale_factor_set )
			*v = *v * aux_data->scale_factor 
					+ aux_data->add_offset;
		else if( aux_data->add_offset_set )
			*v = *v + aux_data->add_offset;
		else if( aux_data->scale_factor_set ) 
			*v = *v * aux_data->scale_factor;

		/* Turn nan's into a more useful value */
		if( isnan(*v)) {
			*v = FILL_FLOAT;
			if( options.debug )
				fprintf( stderr, "fillvalue is nan; resetting to default=%g\n",
					*v );
			}
		return;
		}

	/* default behavior, if no specified "_FillValue" attribute */
	*v = FILL_FLOAT;
	if( options.debug )
		fprintf( stderr, "setting fillvalue to default=%g\n",
			*v );
}

/*******************************************************************************************/
/* This is a "safe" version of the standard ncdimid routine, in
 * that it returns -1 if there is no dimension of that name in
 * the file, EVEN IF the netcdf library is compiled to barf on 
 * errors of this type (rather than always returning -1, as the
 * documentation says it should!!!)
 */
int safe_ncdimid( int fileid, char *dim_name1 )
{
	int	n_vars, err, i, n_dims;
	char	dim_name2[MAX_NC_NAME];
	int	n_gatts, rec_dim;
	size_t	dim_size;

	err = ncinquire( fileid, &n_dims, &n_vars, &n_gatts, &rec_dim );
	if( err < 0 ) {
		fprintf( stderr, "ncview: safe_ncdimid: error in ncinquire call\n" );
		exit( -1 );
		}

	for( i=0; i<n_dims; i++ ) {
		err = nc_inq_dim( fileid, i, dim_name2, &dim_size );
		if( strcmp( dim_name1, dim_name2 ) == 0 )
			return( i );
		}

	return( -1 );
}	

/*******************************************************************************************/
char *nc_type_to_string( nc_type type )
{
	switch( type ) {
		
		case NC_BYTE:	return( "BYTE" );

		case NC_CHAR:	return( "CHAR" );

		case NC_SHORT:	return( "SHORT" );

		case NC_LONG:	return( "LONG" );

		case NC_FLOAT:	return( "FLOAT" );

		case NC_DOUBLE:	return( "DOUBLE" );

		default: 	return( "UNKNOWN" );
		}
}

/*******************************************************************************************/
char *netcdf_att_string( int fileid, char *var_name ) 
{
	int	iatt, varid, len, size_to_use, n_dims, 
		i, dim[50], n_atts, err;
	nc_type	datatype, type;
	char	att_name[MAX_NC_NAME], dummy_var_name[MAX_NC_NAME];
	char	*data, *ret_string, line[2000];

	ret_string    = (char *)malloc(10000);
	sprintf( ret_string, "Attributes for variable %s:\n------------------------------\n", var_name );

	err = nc_inq_varid( fileid, var_name, &varid );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error in netcdf_att_string: could not find var named \"%s\" in file!\n",
			var_name );
		exit(-1);
		}

	err = ncvarinq( fileid, varid, dummy_var_name, &type, &n_dims, dim, &n_atts );
	if( err == -1 ) {
		fprintf( stderr, "netcdf_att_string: failed on ncvarinq call!\n" );
		exit(-1);
		}

	for( iatt=0; iatt<n_atts; iatt++ ) {

		ncattname( fileid, varid, iatt, att_name );
		ncattinq(  fileid, varid, att_name, &datatype, &len );
		switch( datatype ) {
			case NC_BYTE:   size_to_use = sizeof(char);   break;
			case NC_CHAR:   size_to_use = sizeof(char);   break;
			case NC_SHORT:  size_to_use = sizeof(short);  break;
			case NC_LONG:   size_to_use = sizeof(nclong); break;
			case NC_FLOAT:  size_to_use = sizeof(float);  break;
			case NC_DOUBLE: size_to_use = sizeof(double); break;
			case NC_NAT:    fprintf( stderr, "Error, can't handle attribute of type NC_NAT, ignoring\n" ); break;
			}
	
		data = (char *)malloc(size_to_use*len);
			
		ncattget( fileid, varid, att_name, data );

		strcat( ret_string, att_name );
		strcat( ret_string, ": "      );

		for(i=0; i<len; i++) {
			switch( datatype ) {
				case NC_BYTE:   sprintf( line, "%d ",  *((char   *)data+i) ); break;
				case NC_CHAR:   sprintf( line, "%c",   *((char   *)data+i) ); break;
				case NC_SHORT:  sprintf( line, "%d ",  *((short  *)data+i) ); break;
				case NC_LONG:   sprintf( line, "%ld ", *((nclong *)data+i) ); break;
				case NC_FLOAT:  sprintf( line, "%f ",  *((float  *)data+i) ); break;
				case NC_DOUBLE: sprintf( line, "%lf ", *((double *)data+i) ); break;
				case NC_NAT:    sprintf( line, "(NC_NAT) ");                  break;
				}
			strcat( ret_string, line );
			}
		strcat( ret_string, "\n" );
		}

	strcat( ret_string, netcdf_global_att_string( fileid ));
	return( ret_string );
}

/*******************************************************************************************/
char *netcdf_global_att_string( int fileid )
{
	int	iatt, len, size_to_use, i, n_atts, err;
	nc_type	datatype;
	char	att_name[MAX_NC_NAME];
	char	*data, *ret_string, line[2000];

	err = nc_inq_natts( fileid, &n_atts );
	if( err == -1 ) {
		fprintf( stderr, "netcdf_gobal_att_string: failed on nc_inq_natts call!\n" );
		exit(-1);
		}
	
	if( n_atts == 0 ) {
		ret_string    = (char *)malloc(2);
		ret_string[0] = '\0';
		return( ret_string );
		}

	ret_string    = (char *)malloc(10000);
	sprintf( ret_string, "\nGlobal attributes:\n--------------------------\n" );

	for( iatt=0; iatt<n_atts; iatt++ ) {

		ncattname( fileid, NC_GLOBAL, iatt, att_name );
		ncattinq(  fileid, NC_GLOBAL, att_name, &datatype, &len );
		switch( datatype ) {
			case NC_BYTE:   size_to_use = sizeof(char);   break;
			case NC_CHAR:   size_to_use = sizeof(char);   break;
			case NC_SHORT:  size_to_use = sizeof(short);  break;
			case NC_LONG:   size_to_use = sizeof(nclong); break;
			case NC_FLOAT:  size_to_use = sizeof(float);  break;
			case NC_DOUBLE: size_to_use = sizeof(double); break;
			case NC_NAT:    fprintf(stderr,"Error, cannot handle attributes of type NC_NAT; ignoring\n" ); break;
			}
	
		data = (char *)malloc(size_to_use*len);
			
		ncattget( fileid, NC_GLOBAL, att_name, data );

		strcat( ret_string, att_name );
		strcat( ret_string, ": "      );

		for(i=0; i<len; i++) {
			switch( datatype ) {
				case NC_BYTE:   sprintf( line, "%d ",  *((char   *)data+i) ); break;
				case NC_CHAR:   sprintf( line, "%c",   *((char   *)data+i) ); break;
				case NC_SHORT:  sprintf( line, "%d ",  *((short  *)data+i) ); break;
				case NC_LONG:   sprintf( line, "%ld ", *((nclong *)data+i) ); break;
				case NC_FLOAT:  sprintf( line, "%f ",  *((float  *)data+i) ); break;
				case NC_DOUBLE: sprintf( line, "%lf ", *((double *)data+i) ); break;
				case NC_NAT:    sprintf( line, "(NC_NAT) "); break;
				}
			strcat( ret_string, line );
			}
		strcat( ret_string, "\n" );
		}

	return( ret_string );
}

/*******************************************************************************************/
void warn_about_char_dims()
{
	static int	have_done_it = FALSE;

	if( ! have_done_it ) {
		fprintf( stderr, "******************************************************\n" );
		fprintf( stderr, "Warning: you are using character-type dimensions.\n" );
		fprintf( stderr, "Unfortunately, the netCDF standard version 2 does not\n" );
		fprintf( stderr, "include string types.  Therefore, I may not be able to\n" );
		fprintf( stderr, "intuit what you have done.  If the program crashes,\n" );
		fprintf( stderr, "try rerunning with the -no_char_dims option.\n" ); 
		fprintf( stderr, "******************************************************\n" );
		have_done_it = TRUE;
		}
}

/********************************************************************************************/
/* Returns -1 if the dim has NO bounds dimvar.  If the dim DOES have a bounds dimvar,
 * this returns the dimvarid of the bounds dimvar, and sets nvertices to the number
 * of vertices the bounds var has 
 */
int netcdf_dimvar_bounds_id( int fileid, char *dim_name, int *nvertices )
{
	int	reg_dimvar_id, bounds_dimvar_id, dimvar_ndims, err, name_length;
	char	*attname = "bounds";
	char 	*bounds_dimvarname;
	nc_type	type;
	size_t	st_nvertices;
	int dimids[MAX_NC_DIMS]; 

	/* First get the regular dimvar for this dim, then see if that dimvar
	 * has an attribute named "bounds".
	 */
	reg_dimvar_id = netcdf_dimvar_id( fileid, dim_name );
	if( reg_dimvar_id < 0 )
		return( -1 );

	if( netcdf_att_id( fileid, reg_dimvar_id, attname ) < 0 )
		return( -1 );

	err = ncattinq( fileid, reg_dimvar_id, attname, &type, &name_length );
	if( (err < 0) || (type != NC_CHAR))
		return( -1 );

	bounds_dimvarname = (char *)malloc( name_length+1 );
	err = ncattget( fileid, reg_dimvar_id, attname, bounds_dimvarname );
	if( err < 0 ) {
		free( bounds_dimvarname );
		return( -1 );
		}

	if( *(bounds_dimvarname+name_length-1) != '\0' )
		*(bounds_dimvarname + name_length) = '\0';

	err = nc_inq_varid( fileid, bounds_dimvarname, &bounds_dimvar_id );
	if( err != 0 ) {
		free( bounds_dimvarname );
		return( -1 );
		}

	/* Currently only know how to handle 2-d bounds variables */
	err = nc_inq_varndims( fileid, bounds_dimvar_id, &dimvar_ndims );
	if( (err != NC_NOERR) || (dimvar_ndims != 2)) {
		fprintf( stderr, "Currently can only handle bounds dims with ndims=2; bounds var %s has ndims=%d.  Ignoring!\n", 
				bounds_dimvarname, dimvar_ndims );
		free( bounds_dimvarname );
		return( -1 );
		}

	/* Get the dim ids of the bounds var so we can get the length of the trailing
	 * one, which is the number of vertices 
	 */
	err = nc_inq_vardimid( fileid, bounds_dimvar_id, dimids );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error reading bounds info for boundary variable %s.  Ignoring!\n", bounds_dimvarname );
		free( bounds_dimvarname );
		return( -1 );
		}
	err = nc_inq_dimlen( fileid, dimids[1], &st_nvertices );
	if( err != NC_NOERR ) {
		fprintf( stderr, "Error reading nvertices info for boundary variable %s.  Ignoring!\n", bounds_dimvarname );
		free( bounds_dimvarname );
		return( -1 );
		}
	*nvertices = (int)st_nvertices;

	free( bounds_dimvarname );

	return( bounds_dimvar_id );
}

