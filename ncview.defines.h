/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993 through 2008 by David W. Pierce
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
 * 6259 Caminito Carrena 
 * San Diego, CA   92122
 * pierce@cirrus.ucsd.edu
 */

/*
	ncview.defines.h

	#defines, and structure definitions
*/

#ifdef INC_UDUNITS
#include <udunits.h>
#endif

#define PROGRAM_ID	"Ncview 1.93f David W. Pierce  12 September 2008"
#define PROGRAM_VERSION 1.93
#define APP_RES_VERSION 1.93

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

/******************** Buttons in the user interface **********************/
#define	BUTTON_REWIND			1
#define	BUTTON_BACKWARDS		2
#define	BUTTON_PAUSE			3
#define	BUTTON_FORWARD			4
#define	BUTTON_FASTFORWARD		5
#define	BUTTON_COLORMAP_SELECT		6	/* this is also a label */
#define	BUTTON_INVERT_PHYSICAL		7
#define	BUTTON_INVERT_COLORMAP		8
#define	BUTTON_MINIMUM			9
#define	BUTTON_MAXIMUM			10
#define	BUTTON_QUIT			11
#define BUTTON_BLOWUP			12	/* this is also a label */
#define	BUTTON_RESTART			13
#define	BUTTON_TRANSFORM		14	/* this is also a label */
#define BUTTON_DIMSET			15
#define BUTTON_RANGE			16
#define BUTTON_BLOWUP_TYPE		17	/* this is also a label */
#define BUTTON_SKIP			18	/* this is also a label */
#define BUTTON_EDIT			19
#define BUTTON_INFO			20
#define BUTTON_PRINT			21
#define BUTTON_OPTIONS			22

/***************************************************************************
 * These are the overlays we know about
 */
#define OVERLAY_NONE			0
#define OVERLAY_P8DEG			1
#define OVERLAY_P08DEG			2
#define OVERLAY_USA			3
#define OVERLAY_CUSTOM			4

#define OVERLAY_N_OVERLAYS		5


/***************************************************************************
 * General purpose writable labels in the user interface.  These are the 
 * 'real' names; to actually use them, associate a more easily 
 * remembered #define with them, as shown below.
 */
#define LABEL_1			1
#define LABEL_2			2
#define LABEL_3			3
#define LABEL_4			4
#define LABEL_5			11
/* Specific purpose writable labels in the user interface. */
#define LABEL_COLORMAP_NAME	5		/* this is also a button */
#define LABEL_BLOWUP		6		/* this is also a button */
#define LABEL_TRANSFORM		7
#define LABEL_CCINFO_1		8
#define LABEL_CCINFO_2		9
#define LABEL_BLOWUP_TYPE	10		/* this is also a button */
#define LABEL_SKIP		12		/* this is also a button */

#define LABEL_TITLE		LABEL_1
#define LABEL_SCANVAR_NAME	LABEL_2
#define LABEL_SCAN_PLACE	LABEL_3
#define LABEL_DATA_EXTREMA	LABEL_4
#define LABEL_DATA_VALUE	LABEL_5

/*****************************************************************************/
/* Transforming the data before turning it into pixels is supported */
#define N_TRANSFORMS		3
#define TRANSFORM_NONE		1
#define TRANSFORM_LOW		2
#define TRANSFORM_HI		3

/*****************************************************************************
 * Maximum number of X-Y plot windows which can pop up, and the max
 * number of lines on one plot.
 */
#define MAX_PLOT_XY		10
#define MAX_LINES_PER_PLOT	5

/*****************************************************************************
 * This is for the popup windows which show all of a variable's attributes.
 */
#define MAX_DISPLAY_POPUPS	10

/*****************************************************************************/
/* Types of file data formats supported */
#define FILE_TYPE_NETCDF	1

/*****************************************************************************/
/* Maximum name length of a variable */
#define MAX_VAR_NAME_LEN	132

/*****************************************************************************/
/* Maximum name length of a file */
#define MAX_FILE_NAME_LEN	1024

/*****************************************************************************/
/* Maximum name length of a recdim units */
#define MAX_RECDIM_UNITS_LEN	1024

/*****************************************************************************/
/* Possible interpretations for the change_view routine; either change
 * the specified number of FRAMES or the specified PERCENT.
 */
#define FRAMES	1
#define PERCENT	2

/*****************************************************************************/
/* Truncate displayed strings which are longer than this */
#define MAX_DISPLAYED_STRING_LENGTH 	250

/*****************************************************************************/
/* What dimension button sets we have */
#define DIMENSION_X	1
#define DIMENSION_Y	2
#define DIMENSION_SCAN	3
#define DIMENSION_NONE	4

/*****************************************************************************/
/* Button-press modification indicators */
#define	MOD_1		1
#define	MOD_2		2
#define	MOD_3		3
#define	MOD_4		4

/*****************************************************************************/
/* Messages which a dialog popup can return */
#define MESSAGE_OK	1
#define MESSAGE_CANCEL	2

/*****************************************************************************/
/* This is used in x_interface.c, even though it has nothing to do with
 * the X interface, because the X mechanism has a way of
 * reading in resource files, and there is no point in reading in TWO
 * different configuration files.  Sigh.  For use of this, see routine
 * 'check_app_res' in file x_interface.c
 */
#define DEFAULT_DELTA_STEP	10

/*****************************************************************************/
/* Ways in which the file's min and max can be calculated */
#define MIN_MAX_METHOD_FAST	1
#define MIN_MAX_METHOD_MED	2
#define MIN_MAX_METHOD_SLOW	3
#define MIN_MAX_METHOD_EXHAUST	4

/*****************************************************************************/
/* Data which has the fill_value is IGNORED.  It is assumed to represent 
 * out of domain or out of range data.  Netcdf has its own values for this
 * which replace this value, so in Netcdf implementations, this particular
 * value is not the one which is actually used.
 */
#define DEFAULT_FILL_VALUE	1.0e35

/*******************************************************************
 * Ways to expand a small pixmap into a large one.
 */
#define BLOWUP_REPLICATE	1
#define BLOWUP_BILINEAR		2

/*******************************************************************
 * Ways to contract a large pixmap into a small one.
 */
#define SHRINK_METHOD_MEAN	0
#define SHRINK_METHOD_MODE	1

/*********************************************************************
 * Possible states which the data inside the current buffer can be in
 */
#define VDS_VALID	1
#define VDS_INVALID	2
#define VDS_EDITED	3

/*******************************************************************
 * Where postscript output can go.
 */
#define DEVICE_PRINTER	1
#define DEVICE_FILE	2

/*******************************************************************
 * Ways of handling the variable-select area.  We can either list
 * all the variables, or make a pull-down menu for selecting them.
 */
#define VARSEL_LIST	1
#define VARSEL_MENU	2

/*******************************************************************
 * Recognized standards by which the time axis may be described
 */
#define TSTD_UDUNITS		1	/* Ex: units="days since 1900-01-01" */
#define TSTD_EPIC_0		2	/* Ex: units="True Julian Day" w/att epic_code=624 */
#define TSTD_MONTHS		3	/* Ex: units="months", Jan 1 AD = month 1 */

/*******************************************************************
 * Kinds of time-like granularity.
 */
#define TGRAN_SEC	1
#define TGRAN_MIN	2
#define TGRAN_HOUR	3
#define TGRAN_DAY	4
#define TGRAN_MONTH	5
#define TGRAN_YEAR	6

/*******************************************************************
 *
 * 	The main concept here is the 'variable'.  Variables are
 *	things which might possibly be displayed by ncview.  Variables
 *	live in one or more files, and within each of those files 
 *	have a size, and minimum and maximum values.  Different 
 *	variables can be in different files, but if you have the 
 *	same variable in different files it must have the EXACT SAME
 *	layout in all files, with the exception of the first index
 *	(which is the time index in netCDF files).  So, you can have
 *	20 time entries in the first file, then 7 in the second, and
 *	14 in the third; but you can't have the resolution of the 
 *	variable be different in the different files.
 *
 ********************************************************************/

/*****************************************************************************/
typedef unsigned char ncv_pixel;/* If you change this, make sure to change
				 * routine 'data_to_pixels' in util.c!  It
				 * assumes a size of one byte.  Some of the
				 * X routines do also.
				 */

/*****************************************************************************/
/* A general purpose list of character strings */
typedef struct {
	char	*string;
	void	*next, *prev;
	int	index;		/* initialized to position in list */
	void	*aux;		/* auxilliary data */
} Stringlist;

/*****************************************************************************/
/* This describes the file which the relevant variable lives in */
typedef struct {
	void	*next, *prev;
	int	id;		/* internally used ID number */
	char	*filename;
	void	*aux_data;	/* For specific datafile implementations */
	size_t	*var_size;	/* Multi-dimensional size of variables which live in this file */
	float	data_min, data_max; /* for a specific variable in the file */

	/* Following is an ugly hack for an ugly problem.  Basically, different files can have
	 * different units for the unlimited dimension, and some people actually do this.  So
	 * we must store the recdim units for each file.  In a way this is a property more of
	 * the dimensions, so maybe should be in the NCDim structure somehow, but the units live
	 * in each file and have a 1-1 association with each file, so I'm putting them here.
	 */
	char	*recdim_units;
#ifdef INC_UDUNITS
	utUnit	*udunits;	/* only non-null if utScan worked on these units */
#endif
} FDBlist;	

/*****************************************************************************
 * A specific set of data for netCDF-type files.  These won't necessarily 
 * be applicable to different types of data file formats.
 */
typedef struct {
	int	valid_range_set,
		valid_min_set,
		valid_max_set,
		scale_factor_set,
		add_offset_set;

	float	valid_range[2],
		valid_min,
		valid_max,
		scale_factor,
		add_offset;

} NetCDFOptions;
	
/*****************************************************************************/
/* The dimension structure.  This is more for convienence and efficiency
 * than because dimensions are so fundamental; actually, it's the variables
 * which are more important.
 */
typedef struct {
	char	*name, *long_name, *units;
	int	units_change;	/* if 1, then a virtully concatenated timelike dimension has different units in different input files */
	float	min, max, *values;
	int	have_calc_minmax;  /* 0 initially, 1 after min & max have been calculated */
	size_t	size;
	int	timelike;	/* 0 if NOT timelike, 1 if is.  If is, MUST */
				/* have an identified time standard (below). */
	int	time_std;	/* TSTD_UDUNITS, TSTD_EPIC_0, TSTD_MONTHS */
	char	*calendar;	/* ONLY applicable if time_std==TSTD_UDUNITS; can be any CF-1.0 value. Defaults to "standard" */
	int	tgran; 		/* time granularity; i.e., frequency of entries (daily, hourly, etc). Must be one of the TGRAN_* defined above */
	int	global_id;	/* Used internally, goes from 1..total number of dims we know about */
	int	is_lat, is_lon; /* Just a guess if these are lat/lon. Used to put on coastlines automatically */
} NCDim;

/*****************************************************************************/
/* Here it is: the variable structure.  Aspects of the variable which are
 * different from file to file are kept in the pointed-to file descriptor 
 * blocks (FDBs).
 */
typedef struct {
	char	*name;
	void	*next, *prev;			/* for global list of variables */
	float	fill_value;			/* Any data with this special
						 * value will be IGNORED. It
						 * is assumed to indicate 
						 * out-of-range or out-of-domain
						 * data.
						 */
	int	have_set_range;			/* boolean -- have we set the
						 * valid range for this var yet?
						 */
	int	n_dims;				/* how many dimensions this var has */
	FDBlist	*first_file, *last_file;	/* What files this variable lives in */
	float	global_min, global_max,		/* These are diffferent from the */
	        user_min, user_max;	 	/* min & max in the FDBs because these
					 	* are global, rather than local to
					 	* a file.
					 	*/
	size_t	*size;				/* The accumulated size of
						 * this variable, from all 
						 * the files which hold it.
						 */
	int  	effective_dimensionality;	/* # of entries in 'size' array > 1 */
	NCDim	**dim;				/* An array of 'n_dim' pointers to
	 					 * Dimension structures.  This
	 					 * is only filled out for 
	 					 * scannable dimensions!! If
	 					 * the dim is not scannable,
	 					 * a NULL is inserted instead.
	 					 */
	int	is_virtual;			/* Boolean -- true if this var lives
						 * in more than one input file, false
						 * otherwise.
						 */
} NCVar;

/*****************************************************************************/
/* Our current view--the view is the 2D field which is being color-contoured.
 */
typedef struct {
	NCVar	*variable;
	size_t	*var_place;	/* Where we currently are in that var's space, in that file */
	void	*data;		/* The actual 2-D data to colorcontour */
	int	data_status;	/* Either valid, invalid, or edited (changed) */
	unsigned char *pixels;	/* Scaled, replicated, byte array version of data */
	int	x_axis_id, 	/* which axes the 2-D data lies on.  'scan' */
		y_axis_id,	/* is the one accessed by the pushbuttons */
		scan_axis_id;
	int	skip;		/* Number of time entries to stride each time */
	int	plot_XY_axis,	/* Which axis to plot along in XY plots */
		plot_XY_nlines;	/* # of XY lines for this variable on current plot */
	size_t	plot_XY_position[MAX_LINES_PER_PLOT][10];
} View;

/*****************************************************************************
 * Place to store the frames in, if we want in-core displaying.
 */
typedef struct {
	int	valid;		/* Is anything in the frame store valid? */
	ncv_pixel *frame;	/* Actual store of the frames */
	int	*frame_valid;	/* Is this particular frame valid? */
} FrameStore;

/*****************************************************************************/
/* program options */

/* Options for the overlay feature */
typedef struct {
	int	doit;
	int	*overlay;
} OverlayOptions;

typedef struct {
	int	invert_physical,
		invert_colors,
		t_conv,
		debug,
		show_sel,
		no_autoflip,
		no_char_dims,
		private_colormap,
		want_extra_info,
		n_colors,
		small,
		dump_frames,
		no_1d_vars,
		min_max_method,
		delta_step,	/* if > 0, percent of total frames to step when pressing the 
				 * 'forward' or 'backward' button and holding down the Ctrl 
				 * key; if < 0, absolute number of frames to step.
				 */
		transform,
		varsel_style,	/* can be VARSEL_LIST or VARSEL_MENU */
		listsel_max,	/* if # of vars is more than this, auto switch from VARSEL_LIST to VARSEL_MENU */
		color_by_ndims,	/* if 1, then button is color coded by # of effective dims */
		beep_on_restart,
		auto_overlay,	/* if 1, then tries to figure out if coastlines should automatically be added */
		blowup,
		maxsize_pct,	/* -1 if a width/height pair specified instead */
		maxsize_width,	/* in pixels */
		maxsize_height,	/* in pixels */
		shrink_method,
		blowup_default_size,
		display_type;	/* This uses std 'X' defines; PseudoColor, DirectColor, etc */

	char	*ncview_base_dir,
		*window_title,
		*calendar;	/* This OVERRIDES any 'calendar' attribute in the data file */

	int	blowup_type;	/* can be BLOWUP_REPLICATE or BLOWUP_BILINEAR */

	int	save_frames;	/* If true, try to save frames in core for faster display */
	float	frame_delay;	/* Normalied to be between 0.0 and 1.0 */

	OverlayOptions *overlay;
} Options;

/* Postscript printer output options */
typedef struct {
	float	page_width, page_height,		/* In inches */
		page_x_margin, page_upper_y_margin,	/* In inches */
		page_lower_y_margin, ppi;		/* Points per inch */
	int	font_size,
		leading,
		header_font_size;		/* In points */
	char	font_name[132],			/* Postscript name */
		out_file_name[1024];
	int	output_device,
		include_outline, 
		include_id,
		include_title,
		include_axis_labels,
		include_extra_info,
		test_only;
} PrintOptions;

