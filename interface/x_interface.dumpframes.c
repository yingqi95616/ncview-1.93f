/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993-2008 David W. Pierce
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
 * San Diego, CA  92122
 * pierce@cirrus.ucsd.edu
 */

/*****************************************************************************
 *
 *	Elements which implement the X Windows user interface to ncview
 *
 *****************************************************************************/

#include "../ncview.includes.h"
#include "../ncview.defines.h"
#include "../ncview.protos.h"
#include "../ncview.bitmaps.h"
#include <X11/CoreP.h>

#include "ppm.h"

#define DEFAULT_BUTTON_WIDTH	55
#define DEFAULT_LABEL_WIDTH	400
#define DEFAULT_DIMLABEL_WIDTH	95
#define DEFAULT_VARLABEL_WIDTH	95
#define DEFAULT_N_VARS_PER_ROW	4
#define DEFAULT_BLOWUP_SIZE	300
#define N_EXTRA_COLORS		10

#define XtNlabelWidth 		"labelWidth"
#define XtCLabelWidth		"LabelWidth"
#define XtNblowupDefaultSize 	"blowupDefaultSize"
#define XtCBlowupDefaultSize	"BlowupDefaultSize"
#define XtNdimLabelWidth 	"dimLabelWidth"
#define XtCDimLabelWidth 	"DimLabelWidth"
#define XtNvarLabelWidth 	"varLabelWidth"
#define XtCVarLabelWidth 	"VarLabelWidth"
#define XtNbuttonWidth 		"buttonWidth"
#define XtCButtonWidth		"ButtonWidth"
#define XtNnVarsPerRow		"nVarsPerRow"
#define XtCNVarsPerRow		"NVarsPerRow"
#define XtNdeltaStep		"deltaStep"
#define XtCDeltaStep		"DeltaStep"
#define XtNversion 		"version"
#define XtCVersion		"Version"

#define	ORDER_RGB	1 	/* R mask > B mask */
#define	ORDER_BGR	2	/* B mask > R mask */

/* This is used to hold key info about the X server
 * that we use when we try to create an image.
 */
typedef struct {
	int	bits_per_pixel, 
		byte_order,
		bytes_per_pixel, 
		bitmap_unit,
		bitmap_pad,
		rgb_order,
		shift_red,
		shift_green_upper, shift_green_lower,
		shift_blue,
		depth;
	unsigned long	mask_red,
		mask_green_upper, mask_green_lower,
		mask_blue;
} Server_Info;

/**************************/
extern 	NCVar	  *variables;
extern  Options	  options;
extern  ncv_pixel *pixel_transform;
/**************************/

typedef struct {
	int	label_width;		/* width of the informational labels */
	int	dimlabel_width;		/* as above, but for dimension labls */
	int	varlabel_width;		/* as above, but for variable labls */
	int	blowup_default_size;	/* default size, in pixels, of newly opened windows */
	int	button_width;		/* width of the control buttons */
	int	n_vars_per_row;		/* how many vars in one row before
					 * we start another.
					 */
	int	delta_step;		/* Becomes options.delta_step */
	float	version;		/* Must match compiled in version */
} AppData, *AppDataPtr;

typedef struct {
	XColor		*color_list;
	void		*next, *prev;
	char		*name;
	ncv_pixel	*pixel_transform;
} Cmaplist;

/* These are "global" to this directory, in the sense that other files
 * in this directory use them.
 */
Widget 		topLevel;
XtAppContext 	x_app_context;

static Cmaplist		*colormap_list   = NULL, *current_colormap_list = NULL;
static Colormap		current_colormap = (Colormap)NULL;

static AppData		app_data;
static Server_Info	server;
static XtIntervalId	timer;

static int		timer_enabled      = FALSE,
			ccontour_popped_up = FALSE,
			valid_display;

static float		default_version_number = 0.0;

static Pixmap		reverse_pixmap,
			backwards_pixmap,
			pause_pixmap,
			forward_pixmap,
			fastforward_pixmap;

static XEvent	event;

static Widget
	error_popup_widget = NULL,
		error_popupcanvas_widget,
			error_popupdialog_widget,
	dimsel_popup_widget,
		dimsel_popupcanvas_widget,
			dimsel_ok_button_widget,
			dimsel_cancel_button_widget,
	ccontourpanel_widget,
		ccontour_form_widget,
			ccontour_info1_widget,
                		ccontour_widget,
			ccontour_info2_widget,
	commandcanvas_widget,
                buttonbox_widget,
			label1_widget,
			label2_widget,
			label3_widget,
			label4_widget,
			label5_widget,
                        quit_button_widget,
			restart_button_widget,
                        reverse_button_widget,
                        backwards_button_widget,
                        pause_button_widget,
                        forward_button_widget,
                        fastforward_button_widget,
			edit_button_widget,
			info_button_widget,
			scrollspeed_label_widget,
			scrollspeed_widget,
			options_button_widget,
		optionbox_widget,
                        cmap_button_widget,
                        invert_button_widget,
                        invert_color_button_widget,
                        blowup_widget,
			transform_widget,
			dimset_widget,
			range_widget,
			blowup_type_widget,
			print_button_widget,
		varsel_form_widget,
			*var_selection_widget,	/* the boxes with N vars per box */
			varlist_label_widget,
			*varlist_widget,	/* The buttons that select a var */
			varsel_menu_widget,	/* Only if using menu-style var selection */
			var_menu,		/* ditto */
		labels_row_widget,
			lr_dim_widget,
			lr_name_widget,
			lr_min_widget,
			lr_cur_widget,
			lr_max_widget,
			lr_units_widget,
		*diminfo_row_widget = NULL,
			*diminfo_dim_widget = NULL,
			*diminfo_name_widget = NULL,
			*diminfo_min_widget = NULL,
			*diminfo_cur_widget = NULL,
			*diminfo_max_widget = NULL,
			*diminfo_units_widget = NULL,

		xdim_selection_widget,
			xdimlist_label_widget,
			*xdimlist_widget = NULL,

		ydim_selection_widget,
			ydimlist_label_widget,
			*ydimlist_widget = NULL;
		
static XtResource resources[] = {
    {
	XtNlabelWidth, 
	XtCLabelWidth,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, label_width ),
	XtRImmediate,
	(XtPointer)DEFAULT_LABEL_WIDTH,
    },
    {
	XtNblowupDefaultSize, 
	XtCBlowupDefaultSize,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, blowup_default_size ),
	XtRImmediate,
	(XtPointer)DEFAULT_BLOWUP_SIZE,
    },
    {
	XtNdimLabelWidth, 
	XtCDimLabelWidth,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, dimlabel_width ),
	XtRImmediate,
	(XtPointer)DEFAULT_DIMLABEL_WIDTH,
    },
    {
	XtNvarLabelWidth, 
	XtCVarLabelWidth,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, varlabel_width ),
	XtRImmediate,
	(XtPointer)DEFAULT_VARLABEL_WIDTH,
    },
    {
	XtNbuttonWidth, 
	XtCButtonWidth,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, button_width ),
	XtRImmediate,
	(XtPointer)DEFAULT_BUTTON_WIDTH,
    },
    {
	XtNnVarsPerRow, 
	XtCNVarsPerRow,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, n_vars_per_row ),
	XtRImmediate,
	(XtPointer)DEFAULT_N_VARS_PER_ROW,
    },
    {
	XtNdeltaStep,
	XtCDeltaStep,
	XtRInt,
	sizeof( int ),
	XtOffset( AppDataPtr, delta_step ),
	XtRImmediate,
	(XtPointer)DEFAULT_DELTA_STEP,	/* see file do_buttons.c for interpretation of this */
    },
    {
	XtNversion,
	XtCVersion,
	XtRFloat,
	sizeof( float ),
	XtOffset( AppDataPtr, version ),
	XtRFloat,
	(XtPointer)&default_version_number
    },
};

static int	error_popup_done    = FALSE, error_popup_result    = 0;
static int	dimsel_popup_done   = FALSE, dimsel_popup_result   = 0;
static Cursor	busy_cursor;

/******************************************************************************
 * These are only used in this file
 */
void 	new_cmaplist( Cmaplist **cml );
unsigned char interp( int i, int range_i, unsigned char *mat, int n_entries );
void	x_init_widgets		( Widget top );
void	x_init_pixmaps		( Widget top );
void 	create_pixmap 		( Widget shell_widget, Pixmap *pixmap, int id );
void	x_set_lab     		( Widget w, char *s, int width );
void 	x_add_to_cmap_list	( char *name, Colormap new_colormap );
void	colormap_back		( Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	x_make_dim_button_util  ( int dimension, Stringlist *dim_list, char *prefix, 
					Widget **dimlist_widget, 
					Widget parent_widget, 
					char *selected_name );
void	x_popup			( char *message );
int	x_dialog                ( char *message, char *ret_string, int want_cancel_button );
void	track_pointer		( void );

/* the button callbacks and actions in x_interface.c */
/* _mod1 is a standard callback; _mod2 is an accelerated action, and _mod3
 * is a backwards action */
void 	print_button_callback(Widget w, XtPointer client_data, XtPointer call_data );
void 	blowup_type_mod1(Widget w, XtPointer client_data, XtPointer call_data );
void 	varsel_menu_select(Widget w, XtPointer client_data, XtPointer call_data );
void 	range_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	options_mod1	(Widget w, XtPointer client_data, XtPointer call_data);
void 	range_mod3	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	dimset_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	quit_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	cmap_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	cmap_mod3	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	restart_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	reverse_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	reverse_mod2   	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	back_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	back_mod2    	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	pause_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	forward_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	forward_mod2 	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	ff_mod1		(Widget w, XtPointer client_data, XtPointer call_data );
void 	edit_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	info_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	fastforward_mod2 (Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	invert_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	invert_color_mod1 (Widget w, XtPointer client_data, XtPointer call_data );
void 	blowup_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	blowup_mod2	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	blowup_mod3	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	blowup_mod4	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	varlist_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	xdimlist_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	ydimlist_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	transform_mod1	(Widget w, XtPointer client_data, XtPointer call_data );
void 	dimsel_callback	(Widget w, XtPointer client_data, XtPointer call_data );
void 	error_popup_callback(Widget w, XtPointer client_data, XtPointer call_data );
void 	diminfo_cur_mod1(Widget w, XtPointer client_data, XtPointer call_data);
void 	diminfo_cur_mod2(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	diminfo_cur_mod3(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	diminfo_cur_mod4(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	diminfo_min_mod1(Widget w, XtPointer client_data, XtPointer call_data);
void 	do_plot_xy	(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	do_set_dataedit_place  (Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	do_set_min_from_curdata(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	do_set_max_from_curdata(Widget w, XButtonEvent *e, String *p, Cardinal *n );
void 	expose_ccontour();

void 	testf(Widget w, XButtonEvent *e, String *p, Cardinal *n );

static void 	add_callbacks( void );
static void 	make_tc_data( unsigned char *data, long width, long height, 
		unsigned char *tc_data );
static void 	make_tc_data_24( unsigned char *data, long width, long height, 
		unsigned char *tc_data );
static void 	make_tc_data_16( unsigned char *data, long width, long height, 
		unsigned char *tc_data );
static void 	make_tc_data_32( unsigned char *data, long width, long height, 
		unsigned char *tc_data );
static void 	dump_to_ppm( unsigned char *data, size_t width, size_t height );

/*************************************************************************/

	void
x_parse_args( int *p_argc, char **argv )
{
	Visual	*visual;
	char	program_title[132];
	static XImage	*ximage;
	long	data;

	topLevel =  XtVaAppInitialize(
		&x_app_context,	 /* Application context           */
		"Ncview",	 /* Application class             */
		NULL, 0,	 /* command line option list      */
		p_argc, argv,	 /* command line args             */
		NULL,		 /* for missing app-defaults file */
		NULL );		 /* terminate varargs list        */
	
	sprintf( program_title, "Ncview %.2f\n", PROGRAM_VERSION );
	XtVaSetValues( topLevel, XtNtitle, program_title, NULL );

	/* Have to determine the visual class as soon as possible!
	 * In particular, must be done BEFORE x_initialize, because
	 * the colormap initialization is called before x_initialize,
	 * and we have to know the display class to init the colormaps.
	 */
	visual = XDefaultVisualOfScreen( XtScreen( topLevel ) );
	switch ( visual->class ) {
		case PseudoColor:	
			options.display_type = PseudoColor;
			valid_display = TRUE;
			break;

		case TrueColor:
			options.display_type = TrueColor;
			valid_display = TRUE;
			break;

		case StaticColor:
			fprintf( stderr, "Sorry, StaticColor displays ");
			fprintf( stderr, "are not supported.\n" );
			options.display_type = StaticColor;
			valid_display = FALSE;
			break;

		case DirectColor:
			fprintf( stderr, "Sorry, DirectColor displays ");
			fprintf( stderr, "are not supported.\n" );
			valid_display = FALSE;
			break;

		case GrayScale:
			fprintf( stderr, "Sorry, GrayScale displays ");
			fprintf( stderr, "are not supported.\n" );
			valid_display = FALSE;
			break;

		case StaticGray:
			fprintf( stderr, "Sorry, StaticGray displays ");
			fprintf( stderr, "are not supported.\n" );
			valid_display = FALSE;
			break;
		
		default:
			fprintf( stderr, "ERROR!! Unknown visual class %d!!\n",
				visual->class );
			exit( -1 );
		}

	/* Make a test image to get our server parameters
	*/
	data = 0;       /* fake test data */
	if( options.display_type == TrueColor ) {
		ximage  = XCreateImage(
			XtDisplay( topLevel ),
			XDefaultVisualOfScreen( XtScreen(topLevel) ),
			XDefaultDepthOfScreen ( XtScreen(topLevel) ),
			ZPixmap,
			0,
			(char *)data, 
			1, 1, 32, 0 );
		}
	else /* display_type == PseudoColor */
		{
		ximage  = XCreateImage(
			XtDisplay( topLevel ),
			XDefaultVisualOfScreen( XtScreen(topLevel) ),
			XDefaultDepthOfScreen ( XtScreen(topLevel) ),
			ZPixmap,
			0,
			(char *)data,
			1, 1, 8, 0 );
		}

	if( options.debug ) {
		printf( "byte order: %s\n", 
			(ximage->byte_order == LSBFirst) ? "LSBFirst" : "MSBFirst" );
		printf( "bitmap unit: %d\n", ximage->bitmap_unit );
		printf( "bit order: %s\n", 
			(ximage->bitmap_bit_order == LSBFirst) ? "LSBFirst" : "MSBFirst" );
		printf( "bitmap pad: %d\n", ximage->bitmap_pad );
		printf( "depth: %d\n", ximage->depth );
		printf( "bytes per line: %d\n", ximage->bytes_per_line );
		printf( "bits per pixel: %d\n", ximage->bits_per_pixel );
		printf( "r, g, b masks: %0lx, %0lx, %0lx\n", ximage->red_mask,
				ximage->green_mask, ximage->blue_mask );
		}

	server.byte_order	= ximage->byte_order;
	server.bits_per_pixel  	= ximage->bits_per_pixel;
	server.bytes_per_pixel 	= server.bits_per_pixel/8;
	server.bitmap_unit     	= ximage->bitmap_unit;
	server.bitmap_pad      	= ximage->bitmap_pad;
	server.depth           	= ximage->depth;
	if( server.byte_order == MSBFirst ) {
		if( ximage->red_mask > ximage->blue_mask )
			server.rgb_order = ORDER_BGR;
		else
			server.rgb_order = ORDER_RGB;
		}
	else
		{
		if( ximage->red_mask > ximage->blue_mask )
			server.rgb_order = ORDER_RGB;
		else
			server.rgb_order = ORDER_BGR;
		}

	/* These are for 16-bit displays.  These numbers are surely
	 * wrong for other displays than the one 16-bit display I
	 * had to test them on, which was an Intel Linux box.
	 */
	server.shift_blue = 11;
	server.shift_red  = 8;
	server.shift_green_upper = 13;
	server.shift_green_lower = 5;

	server.mask_red = 0x00f8;
	server.mask_green_upper = 0x0007; /* goes with red */
	server.mask_green_lower = 0x00e0; /* goes with blue */
	server.mask_blue = 0x001f;
}

	void
x_initialize()
{
	void	  check_app_res();
	void 	  track_pointer();

	static XtActionsRec new_actions[] = {
		{"cmap_mod3",    	(XtActionProc)cmap_mod3        	},
/*		{"redraw_ccontour",  	(XtActionProc)redraw_ccontour  	}, */
		{"reverse_mod2",     	(XtActionProc)reverse_mod2     	},
		{"back_mod2",     	(XtActionProc)back_mod2        	},
		{"forward_mod2",	(XtActionProc)forward_mod2	},
		{"fastforward_mod2",	(XtActionProc)fastforward_mod2	},
		{"diminfo_cur_mod2",	(XtActionProc)diminfo_cur_mod2  },
		{"diminfo_cur_mod3",	(XtActionProc)diminfo_cur_mod3  },
		{"diminfo_cur_mod4",	(XtActionProc)diminfo_cur_mod4  },
		{"range_mod3",		(XtActionProc)range_mod3	},
		{"blowup_mod2",		(XtActionProc)blowup_mod2	},
		{"blowup_mod3",		(XtActionProc)blowup_mod3	},
		{"blowup_mod4",		(XtActionProc)blowup_mod4	},
		{"do_plot_xy",		(XtActionProc)do_plot_xy	},
		{"testf",		(XtActionProc)testf		},
		{"do_set_dataedit_place",  (XtActionProc)do_set_dataedit_place   },
		{"do_set_min_from_curdata",(XtActionProc)do_set_min_from_curdata },
		{"do_set_max_from_curdata",(XtActionProc)do_set_max_from_curdata },
	};

	XtVaGetApplicationResources( topLevel,
		&app_data,
		resources,
		XtNumber(resources),
		NULL );
	check_app_res( &app_data );
	/* This shouldn't be in this file, really, but X can get it from
	 * the resource file, and there is no point in having ncview read
	 * in two different configuration files.
	 */
	options.delta_step          = app_data.delta_step;
	options.blowup_default_size = app_data.blowup_default_size;

	XtAppAddActions( x_app_context, new_actions, XtNumber( new_actions ));
	x_init_pixmaps( topLevel );
	x_init_widgets( topLevel );
	add_callbacks ();

	x_range_init();
	x_plot_range_init();

	XtRealizeWidget( topLevel );

	busy_cursor = XCreateFontCursor( 
		XtDisplay( commandcanvas_widget ), XC_watch );
	if( busy_cursor == BadValue )
		busy_cursor = (Cursor)NULL;

	
	if( options.display_type == PseudoColor ) {
		XSetWindowColormap( XtDisplay(topLevel), XtWindow(topLevel), 
				current_colormap );
		}

	/* Add the event handler which tracks the cursor in
	 * the data window.
	 */
	XtAddEventHandler( ccontour_widget, 
		PointerMotionMask | ButtonPressMask,
		False,
		track_pointer,
		NULL );

	plot_xy_init();

	x_display_info_init();

	set_options_init();
}

	void
x_query_pointer_position( int *ret_x, int *ret_y )
{
	Window root_return, child_window_return;
	int	root_x, root_y, win_x, win_y;
	unsigned int keys_and_buttons;

	if( XQueryPointer( XtDisplay(topLevel),
			XtWindow( ccontour_widget ),
			&root_return,
			&child_window_return,
			&root_x,
			&root_y,
			&win_x,
			&win_y,
			&keys_and_buttons ) ) {
		*ret_x = win_x;
		*ret_y = win_y;
		}
	else
		{
		*ret_x = -1;
		*ret_y = -1;
		}
}

	void
track_pointer()
{
	Window root_return, child_window_return;
	int	root_x, root_y, win_x, win_y;
	unsigned int keys_and_buttons;

	if( XQueryPointer( XtDisplay(topLevel),
			XtWindow( ccontour_widget ),
			&root_return,
			&child_window_return,
			&root_x,
			&root_y,
			&win_x,
			&win_y,
			&keys_and_buttons ) )
		report_position( win_x, win_y, keys_and_buttons );
}


	void
x_init_widgets_ccontourpanel( Widget top )
{
	if( options.display_type == PseudoColor ) {
		ccontourpanel_widget = XtVaCreatePopupShell(
			options.window_title,
			transientShellWidgetClass,
			top,
			XtNcolormap, current_colormap,
			NULL );

		ccontour_form_widget = XtVaCreateManagedWidget(
			"ccontourform",
			boxWidgetClass,
			ccontourpanel_widget,
			XtNorientation, XtorientVertical,
			XtNhSpace, 0,
			XtNvSpace, 0,
			XtNcolormap, current_colormap,
			NULL);
		}
	else
		{
		ccontourpanel_widget = XtVaCreatePopupShell(
			options.window_title,
			transientShellWidgetClass,
			top,
			NULL );

		ccontour_form_widget = XtVaCreateManagedWidget(
			"ccontourform",
			boxWidgetClass,
			ccontourpanel_widget,
			XtNorientation, XtorientVertical,
			XtNhSpace, 0,
			XtNvSpace, 0,
			NULL);
		}

		if( options.want_extra_info )
			ccontour_info1_widget = XtVaCreateManagedWidget(
				"ccontourinfo1",
				labelWidgetClass,
				ccontour_form_widget,
				XtNlabel, "no variable selected - 1",
				XtNborderWidth, 0,
				XtNwidth, 300,
				NULL );

		ccontour_widget = XtVaCreateManagedWidget(
			"ccontour",
			simpleWidgetClass,
			ccontour_form_widget,
/* cursorName is only present in R5 release */
#ifdef XtNcursorName
/*			XtNcursorName, "dotbox", */
#endif
			XtNwidth,  24,
			XtNheight, 24,
/*			XtNdepth,  server.depth, */
			NULL);

		ccontour_widget->core.widget_class->core_class.compress_exposure = TRUE;
		XtAddEventHandler( ccontour_widget, ExposureMask, FALSE,
			(XtEventHandler)expose_ccontour, NULL );

		if( options.want_extra_info )
			ccontour_info2_widget = XtVaCreateManagedWidget(
				"ccontourinfo2",
				labelWidgetClass,
				ccontour_form_widget,
				XtNlabel, "no variable selected - 2",
				XtNwidth, 300,
				XtNfromVert, ccontour_widget,
				XtNborderWidth, 0,
				NULL );
}

	void
x_init_widgets_labels( Widget parent )
{
	/* Title */
	label1_widget = XtVaCreateManagedWidget(
		"label1",
		labelWidgetClass,
		parent,
		XtNlabel, "no variable selected",
		XtNwidth, app_data.label_width,
		NULL );

	/* Variable name */
	label2_widget = XtVaCreateManagedWidget(
		"label2",
		labelWidgetClass,
		parent,
		XtNlabel, PROGRAM_ID,
		XtNwidth, app_data.label_width,
		XtNfromVert, label1_widget,
		NULL);

	/* Frame number */
	label3_widget = XtVaCreateManagedWidget(
		"label3",
		labelWidgetClass,
		parent,
		XtNwidth, app_data.label_width,
		XtNfromVert, label2_widget,
		XtNlabel, "*** SELECT A VARIABLE TO START ***",
		NULL);

	/* Displayed range */
	label4_widget = XtVaCreateManagedWidget(
		"label4",
		labelWidgetClass,
		parent,
		XtNlabel, "",
		XtNfromVert, label3_widget,
		XtNwidth, app_data.label_width,
		NULL);

	/* Current location and value */
	label5_widget = XtVaCreateManagedWidget(
		"label5",
		labelWidgetClass,
		parent,
		XtNlabel, "",
		XtNfromVert, label4_widget,
		XtNwidth, app_data.label_width,
		NULL);
}

	void
x_init_widgets_buttonbox( Widget parent )
{
	buttonbox_widget = XtVaCreateManagedWidget(
		"buttonbox",
		boxWidgetClass,
		parent,
		XtNorientation, XtorientHorizontal,
		XtNfromVert, label5_widget,
		NULL);

	quit_button_widget = XtVaCreateManagedWidget(
		"quit",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNwidth, app_data.button_width,
		NULL);

	restart_button_widget = XtVaCreateManagedWidget(
		"restart",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNsensitive, False,
		XtNlabel, "->1",
		XtNwidth, app_data.button_width,
		NULL);

	reverse_button_widget = XtVaCreateManagedWidget(
		"reverse",
		toggleWidgetClass,
		buttonbox_widget,
		XtNbitmap, reverse_pixmap,
		XtNsensitive, False,
		NULL);

	backwards_button_widget = XtVaCreateManagedWidget(
		"back",
		commandWidgetClass,
		buttonbox_widget,
		XtNbitmap, backwards_pixmap,
		XtNsensitive, False,
		NULL);

	pause_button_widget = XtVaCreateManagedWidget(
		"pause",
		toggleWidgetClass,
		buttonbox_widget,
		XtNbitmap, pause_pixmap,
		XtNradioGroup, reverse_button_widget,
		XtNstate, True,
		XtNsensitive, False,
		NULL);

	forward_button_widget = XtVaCreateManagedWidget(
		"forward",
		commandWidgetClass,
		buttonbox_widget,
		XtNbitmap, forward_pixmap,
		XtNradioGroup, reverse_button_widget,
		XtNsensitive, False,
		NULL);

	fastforward_button_widget = XtVaCreateManagedWidget(
		"fastforward",
		toggleWidgetClass,
		buttonbox_widget,
		XtNbitmap, fastforward_pixmap,
		XtNradioGroup, reverse_button_widget,
		XtNsensitive, False,
		NULL);

	edit_button_widget = XtVaCreateManagedWidget(
		"Edit",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNsensitive, False,
		NULL);

	info_button_widget = XtVaCreateManagedWidget(
		"?",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNsensitive, False,
		NULL);

	scrollspeed_label_widget = XtVaCreateManagedWidget(
		"Delay:",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNwidth, app_data.button_width,
		XtNborderWidth, 0,
		NULL);

	scrollspeed_widget = XtVaCreateManagedWidget(
		"scrollspeed",
		scrollbarWidgetClass,
		buttonbox_widget,
		XtNorientation, XtorientHorizontal,
		/* XtNshown, 0.1, */
		XtNthumb, None,
		XtNlength, 75,
		XtNthickness, 24,
		NULL);

	options_button_widget = XtVaCreateManagedWidget(
		"Opts",
		commandWidgetClass,
		buttonbox_widget,
		XtNheight, 24,
		XtNsensitive, True,
		NULL);
}

	void
x_init_widgets_optionbox( Widget parent )
{
	optionbox_widget = XtVaCreateManagedWidget(
		"optionbox",
		boxWidgetClass,
		parent,
		XtNorientation, XtorientHorizontal,
		XtNfromVert, buttonbox_widget,
		NULL);

	cmap_button_widget = XtVaCreateManagedWidget(
		"cmap",
		commandWidgetClass,
		optionbox_widget,
		XtNlabel, current_colormap_list->name,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);

	invert_button_widget = XtVaCreateManagedWidget(
		"Inv P",
		toggleWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);

	invert_color_button_widget = XtVaCreateManagedWidget(
		"Inv C",
		toggleWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);

	blowup_widget = XtVaCreateManagedWidget(
		"blowup",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		XtNlabel, "Mag X1",
		NULL);

	transform_widget = XtVaCreateManagedWidget(
		"transform",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, True,
		XtNlabel, "Linear",
		XtNwidth, app_data.button_width,
		XtNsensitive, False,
		NULL);

	dimset_widget = XtVaCreateManagedWidget(
		"dimset",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNlabel, "Axes",
		XtNwidth, app_data.button_width,
		NULL);

	range_widget = XtVaCreateManagedWidget(
		"Range",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);

	blowup_type_widget = XtVaCreateManagedWidget(
		"blowup_type",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);

	print_button_widget = XtVaCreateManagedWidget(
		"Print",
		commandWidgetClass,
		optionbox_widget,
		XtNsensitive, False,
		XtNwidth, app_data.button_width,
		NULL);
}

	void
x_init_widgets_varsel_menu( Widget parent )
{
	int	i, n_vars;
	NCVar	*var;
	char	widget_name[128];
	Widget	entry;

	varsel_menu_widget = XtVaCreateManagedWidget(
		"Var to display",
		menuButtonWidgetClass,
		parent,
		XtNmenuName, "var_menu",
		NULL );

	var_menu = XtVaCreatePopupShell( 
		"var_menu", 
		simpleMenuWidgetClass,
		varsel_menu_widget,
		NULL );

	n_vars = n_vars_in_list( variables );
	var       = variables;
	for( i=0; i<n_vars; i++ ) {
		sprintf( widget_name, "%s", var->name );
		entry = XtVaCreateManagedWidget(
			widget_name,
			smeBSBObjectClass,
			var_menu,
			NULL );
		XtAddCallback( entry, XtNcallback, varsel_menu_select, (XtPointer)i );
		var = var->next;
		}
}

	void
x_init_widgets_varsel_list( Widget parent )
{
	int	n_vars, n_varsel_boxes, which_box, i, state;
	NCVar	*var;
	char	widget_name[128];
	Widget	w;

	/* Arrange the variables in boxes, n_vars_per_row variables to a box */
	n_vars               = n_vars_in_list( variables );
	n_varsel_boxes       = n_vars / app_data.n_vars_per_row + 5;
	var_selection_widget = (Widget *)malloc( n_varsel_boxes*sizeof( Widget ));

	/* Make an array of widgets for the variables; indicate the end of the 
	 * array by a NULL value.
	 */
	varlist_widget = (Widget *)malloc( (n_vars+1)*sizeof(Widget));
	if( varlist_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_widgets: malloc ");
		fprintf( stderr, "failed on varlist_widget initializeation\n" );
		exit( -1 );
		}
	*(varlist_widget+n_vars) = NULL;

	var       = variables;
	which_box = 0;
	for( i=0; i<n_vars; i++ ) {
		if( var == NULL ) 
			{
			fprintf( stderr, "ncview: x_init_widgets: internal ");
			fprintf( stderr, "inconsistancy -- empty variable list\n" );
			exit( -1 );
			}
		if( i == 0 ) {
			/* The very first button box! */
			sprintf( widget_name, "varselbox_%1d", which_box+1 );
			*(var_selection_widget+which_box) = XtVaCreateManagedWidget(
				widget_name,
				boxWidgetClass,
				parent,
				XtNorientation, XtorientHorizontal,
				XtNborderWidth, 0,
				NULL);
			sprintf( widget_name, "varlist_label_%1d", which_box+1 );
			varlist_label_widget = XtVaCreateManagedWidget(
				widget_name,
				labelWidgetClass,
				*(var_selection_widget+which_box),
				XtNwidth, app_data.button_width,
				XtNlabel, "Var:",
				XtNborderWidth, 0,
				NULL );
			which_box++;
			}
		else if( (i % app_data.n_vars_per_row) == 0 ) {
			/* A new button box! */
			sprintf( widget_name, "box_%1d", which_box+1 );
			*(var_selection_widget+which_box) = XtVaCreateManagedWidget(
				widget_name,
				boxWidgetClass,
				parent,
				XtNorientation, XtorientHorizontal,
				XtNborderWidth, 0,
				XtNfromVert, *(var_selection_widget + which_box - 1),
				NULL);
			sprintf( widget_name, "varlist_label_%1d", which_box+1 );
			varlist_label_widget = XtVaCreateManagedWidget(
				widget_name,
				labelWidgetClass,
				*(var_selection_widget+which_box),
				XtNwidth, app_data.button_width,
				XtNborderWidth, 0,
				XtNlabel, "",
				NULL );
			which_box++;
			}

		sprintf( widget_name, "varsel_%s", var->name );
		state = False;
		if( i == 0 )  /* first variable button */
			*(varlist_widget+i) = XtVaCreateManagedWidget(
				widget_name,
				toggleWidgetClass,
				*(var_selection_widget+which_box-1),
				XtNstate, state,
				XtNlabel, var->name,
				XtNsensitive, True,
				XtNwidth, app_data.varlabel_width,
				NULL );
		else
			*(varlist_widget+i) = XtVaCreateManagedWidget(
				widget_name,
				toggleWidgetClass,
				*(var_selection_widget+which_box-1),
				XtNradioGroup, *varlist_widget,
				XtNstate, state,
				XtNlabel, var->name,
				XtNsensitive, True,
				XtNwidth, app_data.varlabel_width,
				NULL );
		var = var->next;
		}

	/* In the degenerate case of only one variable, must make it a radio
	 * group by itself; can't do it above, because it needs to know its
	 * own widget number before it can be done.
	 */
	if( n_vars == 1 )
		XtVaSetValues( *varlist_widget, XtNradioGroup, 
				*varlist_widget, NULL );

	i = 0;
	while( (w = *(varlist_widget + i++)) != NULL )
		XtAddCallback( w, XtNcallback, varlist_mod1, NULL );
}

	void
x_init_widgets_varsel( Widget parent )
{
	if( options.varsel_style == VARSEL_LIST )
		x_init_widgets_varsel_list( parent );
	else
		x_init_widgets_varsel_menu( parent );
}

	void
x_init_widgets_dimlabels( Widget parent )
{
	labels_row_widget = XtVaCreateManagedWidget(
		"label_row",
		boxWidgetClass,
		commandcanvas_widget,
		XtNfromVert, varsel_form_widget,
		XtNorientation, XtorientHorizontal,
		NULL);

	lr_dim_widget = XtVaCreateManagedWidget(
		"label_dimension",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Dim:",
		XtNjustify, XtJustifyRight,
		XtNwidth, 50,
		NULL);

	lr_name_widget = XtVaCreateManagedWidget(
		"label_name",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Name:",
		XtNwidth, app_data.dimlabel_width,
		NULL);

	lr_min_widget = XtVaCreateManagedWidget(
		"label_min",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Min:",
		XtNwidth, app_data.dimlabel_width,
		NULL);

	lr_cur_widget = XtVaCreateManagedWidget(
		"label_cur",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Current:",
		XtNwidth, app_data.dimlabel_width,
		NULL);

	lr_max_widget = XtVaCreateManagedWidget(
		"label_max",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Max:",
		XtNwidth, app_data.dimlabel_width,
		NULL);

	lr_units_widget = XtVaCreateManagedWidget(
		"label_units",
		labelWidgetClass,
		labels_row_widget,
		XtNlabel, "Units:",
		XtNwidth, app_data.dimlabel_width,
		NULL);
}

	void
x_init_widgets( Widget top )
{
	int	buttonbox_width, n_dims;
	int	max_dims;
	Stringlist *dim_list, *max_dim_list;
	NCVar	*var;

	x_init_widgets_ccontourpanel( top );

	if( options.display_type == PseudoColor )
		commandcanvas_widget = XtVaCreateManagedWidget(
			"commandcanvas",
			formWidgetClass,
			top,
			XtNcolormap, current_colormap,
			NULL);
	else
		commandcanvas_widget = XtVaCreateManagedWidget(
			"commandcanvas",
			formWidgetClass,
			top,
			NULL);

	/* The labels that hold name of displayed variable, the
	 * variable's range, current location and value, and
	 * the displayed frame number.
	 */
	x_init_widgets_labels( commandcanvas_widget );

	/* First row of buttons, including "Quit" and the
	 * tape-recorder style movement buttons
	 */
	x_init_widgets_buttonbox( commandcanvas_widget );

	/* Second row of options buttons, including colormap selection,
	 * whether or not to invert, mag, transformation, axes,
	 * range, interpolation, and printing
	 */
	x_init_widgets_optionbox( commandcanvas_widget );

	XtVaGetValues( buttonbox_widget, XtNwidth, &buttonbox_width, NULL );

	varsel_form_widget = XtVaCreateManagedWidget(
		"varselectform",
		formWidgetClass,
		commandcanvas_widget,
		XtNfromVert, optionbox_widget,
		NULL);

	/* the widgets that allow the user to select the variable to display */
	x_init_widgets_varsel( varsel_form_widget );

	/* Description lables for the following dimension entries */
	x_init_widgets_dimlabels( commandcanvas_widget );

	/* Construct an aggregate list which has the most possible scannable
	 * dimensions for all variables in the file.
	 */
	max_dims = -1;
	var      = variables;
	while( var != NULL ) {
		dim_list = fi_scannable_dims( var->first_file->id, var->name );
		n_dims   = n_strings_in_list( dim_list );
		if( n_dims > max_dims ) {
			max_dims     = n_dims;
			max_dim_list = dim_list;
			}
		var = var->next;
		}
	x_init_dim_info( max_dim_list );
}	

	void
x_init_pixmaps( Widget top )
{
        create_pixmap( top, &reverse_pixmap,     BUTTON_REWIND      );
	create_pixmap( top, &backwards_pixmap,   BUTTON_BACKWARDS   );
	create_pixmap( top, &pause_pixmap,       BUTTON_PAUSE       );
	create_pixmap( top, &forward_pixmap,     BUTTON_FORWARD     );
	create_pixmap( top, &fastforward_pixmap, BUTTON_FASTFORWARD );
}	

	void
create_pixmap( Widget shell_widget, Pixmap *pixmap, int id )
{
	switch( id ) {
		
		case BUTTON_REWIND:
        		*pixmap = XCreateBitmapFromData( XtDisplay(shell_widget),
				RootWindowOfScreen(XtScreen(shell_widget)),
				(char *)reversebitmap_bits,
				reversebitmap_width,
				reversebitmap_height );
			break;

		case BUTTON_BACKWARDS:
        		*pixmap = XCreateBitmapFromData( XtDisplay(shell_widget),
				RootWindowOfScreen(XtScreen(shell_widget)),
				(char *)backbitmap_bits,
				backbitmap_width,
				backbitmap_height );
			break;

		case BUTTON_PAUSE:
        		*pixmap = XCreateBitmapFromData( XtDisplay(shell_widget),
				RootWindowOfScreen(XtScreen(shell_widget)),
				(char *)pausebitmap_bits,
				pausebitmap_width,
				pausebitmap_height );
			break;

		case BUTTON_FORWARD:
        		*pixmap = XCreateBitmapFromData( XtDisplay(shell_widget),
				RootWindowOfScreen(XtScreen(shell_widget)),
				(char *)forwardbitmap_bits,
				forwardbitmap_width,
				forwardbitmap_height );
			break;

		case BUTTON_FASTFORWARD:
        		*pixmap = XCreateBitmapFromData( XtDisplay(shell_widget),
				RootWindowOfScreen(XtScreen(shell_widget)),
				(char *)ffbitmap_bits,
				ffbitmap_width,
	       			ffbitmap_height );
			break;

		default:
			fprintf( stderr, "err: internal error in ncview,\n" );
			fprintf( stderr, "routine create_pixmap: no recognized value\n" );
			fprintf( stderr, ">%d<\n", id );
			exit( 1 );
			break;
		}
}

	void
x_create_colormap( char *name, unsigned char r[256], unsigned char g[256], unsigned char b[256] )
{
	Colormap	orig_colormap, new_colormap;
	Display		*display;
	int		i, status;
	XColor		*color;
	unsigned long	plane_masks[1], pixels[1];
	Cmaplist	*cmaplist, *cml;
	static int	first_time_through = FALSE;

	display = XtDisplay( topLevel );

	if( colormap_list == NULL )
		first_time_through = TRUE;
	else
		first_time_through = FALSE;

	/* Make a new Cmaplist structure, and link it into the list */
	new_cmaplist( &cmaplist );
	cmaplist->name = (char *)malloc( (strlen(name)+1)*sizeof(char) );
	strcpy( cmaplist->name, name );
	if( first_time_through )
		colormap_list = cmaplist;
	else
		{
		cml = colormap_list;
		while( cml->next != NULL )
			cml = cml->next;
		cml->next      = cmaplist;
		cmaplist->prev = cml;
		}

	if( first_time_through ) {

	/* First time through, allocate the read/write entries in the colormap.
	 * We always do ten more than the number of colors requested so that
	 * black can indicate 'fill_value' entries, and the other color entries
	 * (which are the window elements) can be colored as desired.
	 */
		current_colormap_list = cmaplist;

		/* TrueColor device */
		if( options.display_type == TrueColor ) {
			for( i=0; i<(options.n_colors+N_EXTRA_COLORS); i++ ) {
				(cmaplist->color_list+i)->pixel = (long)i;
				*(cmaplist->pixel_transform+i)  = (ncv_pixel)i;
				}
			}

		/* Shared colormap, PseudoColor device */
		if( (!options.private_colormap) && (options.display_type == PseudoColor) ) {
			/* Try the standard colormap, and see if 
			 * it has enough available colorcells.
			 */
			orig_colormap    = DefaultColormap( display, 0 );
			current_colormap = orig_colormap;
			i      = 0;
			status = 1;
			while( (i < (options.n_colors+N_EXTRA_COLORS)) && (status != 0) ) {
				status = XAllocColorCells( display, orig_colormap, True, 
					plane_masks, 0, pixels, 1 );
				(cmaplist->color_list+i)->pixel = *pixels;
				*(cmaplist->pixel_transform+i)   = (ncv_pixel)*pixels;
				i++;
				}
			}

		/* standard colormap failed for PseudoColor, allocate a private colormap */
		if( (options.display_type == PseudoColor) && 
				((status == 0) || (options.private_colormap)) ) {	
			printf( "Using private colormap...\n" );
			new_colormap  = XCreateColormap(
				display,
				RootWindow( display, DefaultScreen( display ) ),
				XDefaultVisualOfScreen( XtScreen( topLevel ) ),
				AllocNone ); 
			current_colormap = new_colormap;
			for( i=0; i<(options.n_colors+N_EXTRA_COLORS); i++ )
				{
				status = XAllocColorCells( display, new_colormap, 
						True, plane_masks,
						0, pixels, 1 );
				if( status == 0 ) {
					fprintf( stderr, "ncview: x_create_colormap: couldn't allocate \n" );
					fprintf( stderr, "requested number of colorcells in a private colormap\n" );
					fprintf( stderr, "Try requesting fewer colors with the -nc option\n" );
					exit( -1 );
					}
				(cmaplist->color_list+i)->pixel = *pixels;
				*(cmaplist->pixel_transform+i)  = (ncv_pixel)*pixels;
				}
			}
		pixel_transform = cmaplist->pixel_transform;
		}
	else
		{
		/* if NOT the first time through, just set the pixel values */
		for( i=0; i<options.n_colors+N_EXTRA_COLORS; i++ ) {
			(cmaplist->color_list+i)->pixel =
				(current_colormap_list->color_list+i)->pixel;
			*(cmaplist->pixel_transform+i)   = 
				(ncv_pixel)(current_colormap_list->color_list+i)->pixel;
			}
		}

	/* Set the first ten colors including black, the color used for "Fill_Value" entries */
	for( i=0; i<N_EXTRA_COLORS; i++ ) {
		color        = cmaplist->color_list+i;
		color->flags = DoRed | DoGreen | DoBlue;
		color->red   = 256*(unsigned int)255;
		color->green = 256*(unsigned int)255;
		color->blue  = 256*(unsigned int)255;
		}
	color        = cmaplist->color_list+1;
	color->flags = DoRed | DoGreen | DoBlue;
	color->red   = 256*(unsigned int)0;
	color->green = 256*(unsigned int)0;
	color->blue  = 256*(unsigned int)0;

	for( i=N_EXTRA_COLORS; i<options.n_colors+N_EXTRA_COLORS; i++ )
		{
		color        = cmaplist->color_list+i;
		color->flags = DoRed | DoGreen | DoBlue;
		color->red   = (unsigned int)
				(256*interp( i-N_EXTRA_COLORS, options.n_colors, r, 256 ));
		color->green = (unsigned int)
				(256*interp( i-N_EXTRA_COLORS, options.n_colors, g, 256 ));
		color->blue  = (unsigned int)
				(256*interp( i-N_EXTRA_COLORS, options.n_colors, b, 256 ));
		}

	if( (options.display_type == PseudoColor) && first_time_through )
		XStoreColors( XtDisplay(topLevel), current_colormap, current_colormap_list->color_list,
			options.n_colors+N_EXTRA_COLORS );
}

/* Go forward or backwards in the colormap list, and install the new one.
 * Returns the name of the newly installed colormap.  If do_widgets_flag
 * is false, don't install colormaps on the widgets, just on the topLevel.
 */
	char *
x_change_colormap( int delta, int do_widgets_flag )
{
	if( delta > 0 )
		{
		if( current_colormap_list->next != NULL )
			current_colormap_list = current_colormap_list->next;
		else
			current_colormap_list = colormap_list;
		}
	else
		{
		if( current_colormap_list->prev != NULL )
			current_colormap_list = current_colormap_list->prev;
		else /* go to last colormap */
			while( current_colormap_list->next != NULL )
				current_colormap_list = current_colormap_list->next;
		}

	if( options.display_type == PseudoColor )
		XStoreColors( XtDisplay(topLevel), current_colormap, 
			current_colormap_list->color_list,
			options.n_colors+1 );
	pixel_transform = current_colormap_list->pixel_transform;

	return( current_colormap_list->name );
}

	void
x_draw_2d_field( unsigned char *data, size_t width, size_t height )
{
	Display	*display;
	Screen	*screen;
	static XImage	*ximage;
	XGCValues values;
	GC	gc;
	static	size_t last_width=0L, last_height=0L;
	static 	unsigned char *tc_data=NULL;

	dump_to_ppm( data, width, height );

	display = XtDisplay( ccontour_widget );
	screen  = XtScreen ( ccontour_widget );

	if( options.display_type == TrueColor ) {
		/* If the TrueColor data array does not yet exist, 
		 * or is the wrong size, then allocate it.
		 */
		if( tc_data == NULL ) {
			tc_data=(unsigned char *)malloc( server.bitmap_unit*width*height );
			last_width  = width;
			last_height = height;
			}
		else if( (width!=last_width) || (height!=last_height)) {
			free( tc_data );
			last_width  = width;
			last_height = height;
			tc_data=(unsigned char *)malloc( server.bitmap_unit*width*height );
			}
		/* Convert data to TrueColor representation, with
		 * the proper number of bytes per pixel
		 */
		make_tc_data( data, width, height, tc_data );

		ximage  = XCreateImage(
			display,
			XDefaultVisualOfScreen( screen ),
			XDefaultDepthOfScreen ( screen ),
			ZPixmap,
			0,
			(char *)tc_data, 
			(unsigned int)width, (unsigned int)height,
			32, 0 );
		}
	else /* display_type == PseudoColor */
		{
		ximage  = XCreateImage(
			display,
			XDefaultVisualOfScreen( screen ),
			XDefaultDepthOfScreen ( screen ),
			ZPixmap,
			0,
			(char *)data,
			(unsigned int)width, (unsigned int)height,
			8, 0 );
		}

	gc = XtGetGC( ccontour_widget, (XtGCMask)0, &values );

	if( !valid_display )
		return;

	XPutImage(
		display,
		XtWindow( ccontour_widget ),
		gc,
		ximage,
		0, 0, 0, 0,
		(unsigned int)width, (unsigned int)height );
}

/* Converts the byte-scaled data to truecolor representation,
 * using the current color map.  
 */
	static void
make_tc_data( unsigned char *data, long width, long height, 
	unsigned char *tc_data )
{
	switch (server.bytes_per_pixel) {
		case 4: make_tc_data_32( data, width, height, tc_data );
			break;

		case 3:
			make_tc_data_24( data, width, height, tc_data );
			break;

		case 2:
			make_tc_data_16( data, width, height, tc_data );
			break;

		default:
			fprintf( stderr, "Sorry, I am not set up to produce ");
			fprintf( stderr, "images of %d bytes per pixel.\n", 
					server.bytes_per_pixel );
			exit( -1 );
			break;
		}
}

	static void
make_tc_data_16( unsigned char *data, long width, long height, 
		unsigned char *tc_data )
{
	int	i, j, pix;

	/****************************************
	 *    Least significant bit first
	 ****************************************/
	for( i=0; i<width; i++ )
	for( j=0; j<height; j++ ) {

		pix = *(data+i+j*width);

		*(tc_data+i*2+j*(width*2)) = 
			(unsigned char)((current_colormap_list->color_list+pix)->blue>>server.shift_blue & server.mask_blue);

		*(tc_data+i*2+1+j*(width*2)) = 
			(unsigned char)((current_colormap_list->color_list+pix)->green>>server.shift_green_upper & server.mask_green_upper);


		*(tc_data+i*2+j*(width*2)) = *(tc_data+i*2+j*(width*2)) +
			(unsigned char)((current_colormap_list->color_list+pix)->green>>server.shift_green_lower & server.mask_green_lower );

		*(tc_data+i*2+1+j*(width*2)) = *(tc_data+i*2+1+j*(width*2)) +
			(unsigned char)((current_colormap_list->color_list+pix)->red>>server.shift_red & server.mask_red );
		}


}

	static void
make_tc_data_24( unsigned char *data, long width, long height, 
		unsigned char *tc_data )
{
	int	i, j, pix, o_r, o_g, o_b;
	long	pad_offset, po_val;

	if( server.rgb_order == ORDER_RGB ) {
		o_r = 2;
		o_g = 1;
		o_b = 0;
		}
	else
		{
		o_r = 0;
		o_g = 1;
		o_b = 2;
		}

	/* pad to server.bitmap_pad bits if required */
	pad_offset = 0L;
	po_val     = 0L;
	if( (((width*3)%4) != 0) && (server.bits_per_pixel != server.bitmap_pad) ) 
		po_val = (server.bitmap_pad/8) - (width*3)%4;

	for( j=0; j<height; j++ ) {
		for( i=0; i<width; i++ ) {

			pix = *(data+i+j*width);
			*(tc_data+i*3+o_b+j*(width*3)+pad_offset) = 
				(char)((current_colormap_list->color_list+pix)->blue>>8);
			*(tc_data+i*3+o_g+j*(width*3)+pad_offset) = 
				(char)((current_colormap_list->color_list+pix)->green>>8);
			*(tc_data+i*3+o_r+j*(width*3)+pad_offset) = 
				(char)((current_colormap_list->color_list+pix)->red>>8);
			}
			pad_offset += po_val;
		}
}

	static void
make_tc_data_32( unsigned char *data, long width, long height, 
		unsigned char *tc_data )
{
	int	i, j, pix, o_r, o_g, o_b;

	if( server.rgb_order == ORDER_RGB ) {
		o_r = 2;
		o_g = 1;
		o_b = 0;
		}
	else
		{
		o_r = 0;
		o_g = 1;
		o_b = 2;
		}
	if( server.byte_order == MSBFirst ) {
		o_r++;
		o_g++;
		o_b++;
		}

	for( i=0; i<width; i++ )
	for( j=0; j<height; j++ ) {

/*
*(tc_data+i*4+0 +j*(width*4)) = 0;
*(tc_data+i*4+1 +j*(width*4)) = 0;
*(tc_data+i*4+2 +j*(width*4)) = 128;
*(tc_data+i*4+3 +j*(width*4)) = 0;
printf( "%d ", pix);
*/
		pix = *(data+i+j*width);
		*(tc_data+i*4+o_b+j*(width*4)) = 
			(char)((current_colormap_list->color_list+pix)->blue>>8);
		*(tc_data+i*4+o_g+j*(width*4)) = 
			(char)((current_colormap_list->color_list+pix)->green>>8);
		*(tc_data+i*4+o_r+j*(width*4)) = 
			(char)((current_colormap_list->color_list+pix)->red>>8);
		}
}

	void
x_set_speed_proc( Widget scrollbar, XtPointer client_data, XtPointer position )
{
	float	f_pos;

	f_pos = *(float *)position;
	options.frame_delay = f_pos;
}

	void
x_set_2d_size( size_t width, size_t height )
{
	size_t		new_width, new_height;
	Dimension 	widget_height;

	new_width = width;
	new_height = height;
	if( options.want_extra_info ) {
		XtVaGetValues( ccontour_info1_widget, 
					XtNheight, &widget_height, NULL );
		new_height += widget_height+4L;
		XtVaGetValues( ccontour_info2_widget, 
					XtNheight, &widget_height, NULL );
		new_height += widget_height+4L;
		}

	XtResizeWidget( ccontourpanel_widget, (Dimension)(new_width+2L), (Dimension)(new_height+2L), 0 );
	XtResizeWidget( ccontour_form_widget, (Dimension)(new_width+2L), (Dimension)(new_height+2L), 0 );
	XtResizeWidget( ccontour_widget,      (Dimension)width, (Dimension)height, 1 );
}

	void
x_set_label( int id, char *string )
{
	switch( id ) {
		case LABEL_1:
			x_set_lab( label1_widget, string, app_data.label_width );
			break;

		case LABEL_2:
			x_set_lab( label2_widget, string, app_data.label_width );
			break;

		case LABEL_3:
			x_set_lab( label3_widget, string, app_data.label_width );
			break;

		case LABEL_4:
			x_set_lab( label4_widget, string, app_data.label_width );
			break;

		case LABEL_5:
			x_set_lab( label5_widget, string, app_data.label_width );
			break;

		case LABEL_COLORMAP_NAME:
			x_set_lab( cmap_button_widget, string,
							app_data.button_width );
			break;

		case LABEL_BLOWUP:
			x_set_lab( blowup_widget, string,
							app_data.button_width );
			break;

		case LABEL_TRANSFORM:
			x_set_lab( transform_widget, string,
							app_data.button_width );
			break;

		case LABEL_CCINFO_1:
			x_set_lab( ccontour_info1_widget, string, 0 );
			break;

		case LABEL_CCINFO_2:
			x_set_lab( ccontour_info2_widget, string, 0 );
			break;

		case LABEL_BLOWUP_TYPE:
			x_set_lab( blowup_type_widget, string, app_data.button_width );
			break;

		default:
			fprintf( stderr, "ncview: x_set_label: internal error,\n" );
			fprintf( stderr, "unknown label id %d\n", id );
			exit( -1 );
		}
}

	void
x_set_lab( Widget w, char *s, int width )
{
	XtVaSetValues( w, XtNlabel, s,     NULL );
	if( width != 0 )
		XtVaSetValues( w, XtNwidth, width, NULL );
}

/*************************** X interface button callbacks ************************/
	static void
add_callbacks()
{
        XtAddCallback( quit_button_widget,         XtNcallback, quit_mod1,         NULL);
        XtAddCallback( cmap_button_widget,         XtNcallback, cmap_mod1,         NULL );
        XtAddCallback( restart_button_widget,      XtNcallback, restart_mod1,      NULL );
        XtAddCallback( reverse_button_widget,      XtNcallback, reverse_mod1,      NULL );
        XtAddCallback( backwards_button_widget,    XtNcallback, back_mod1,         NULL );
        XtAddCallback( pause_button_widget,        XtNcallback, pause_mod1,        NULL );
        XtAddCallback( forward_button_widget,      XtNcallback, forward_mod1,      NULL );
        XtAddCallback( fastforward_button_widget,  XtNcallback, ff_mod1,           NULL );
        XtAddCallback( edit_button_widget,         XtNcallback, edit_mod1,         NULL );
        XtAddCallback( info_button_widget,         XtNcallback, info_mod1,         NULL );
        XtAddCallback( invert_button_widget,       XtNcallback, invert_mod1,       NULL );
        XtAddCallback( invert_color_button_widget, XtNcallback, invert_color_mod1, NULL );
        XtAddCallback( blowup_widget,              XtNcallback, blowup_mod1,       NULL );
        XtAddCallback( transform_widget,           XtNcallback, transform_mod1,    NULL );
        XtAddCallback( dimset_widget,              XtNcallback, dimset_mod1,       NULL );
        XtAddCallback( range_widget,               XtNcallback, range_mod1,        NULL );
	XtAddCallback( scrollspeed_widget,         XtNjumpProc, x_set_speed_proc,  NULL );
	XtAddCallback( options_button_widget,      XtNcallback, options_mod1,      NULL );

        XtAddCallback( blowup_type_widget,         XtNcallback, blowup_type_mod1,        NULL );
        XtAddCallback( print_button_widget,        XtNcallback, print_button_callback,        NULL );
}

        void
options_mod1( Widget widget, XtPointer client_data, XtPointer call_data)
{
	in_button_pressed( BUTTON_OPTIONS, MOD_1 );
}

        void
range_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_RANGE, MOD_1 );
}

        void
print_button_callback(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_PRINT, MOD_1 );
}

        void
blowup_type_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_BLOWUP_TYPE, MOD_1 );
}

	void
range_mod3( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_RANGE, MOD_3 );
}

        void
dimset_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_DIMSET, MOD_1 );
}

        void
quit_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_QUIT, MOD_1 );
}

        void
cmap_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_COLORMAP_SELECT, MOD_1 );
}

	void
cmap_mod3( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_COLORMAP_SELECT, MOD_3 );
}

	void
do_plot_xy( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	plot_XY();
}

	void
do_set_dataedit_place( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	set_dataedit_place();
}

	void
do_set_min_from_curdata( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	set_min_from_curdata();
}

	void
do_set_max_from_curdata( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	set_max_from_curdata();
}

        void
diminfo_cur_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	int	i = 0;
	Widget	*w;
	String	label;

	while( (w = diminfo_cur_widget+i) != NULL ) {
		if( *w == widget ) {
			XtVaGetValues( *(diminfo_name_widget+i), 
						XtNlabel, &label, NULL );
			in_change_current( label, MOD_1 );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: diminfo_cur_mod1 callback: can't find " );
	fprintf( stderr, "widget for the pressed button\n" );
	exit( -1 );
}

	void
diminfo_cur_mod2( Widget widget, XButtonEvent *event, String *params, 
							Cardinal *num_params )
{
	int	i = 0;
	Widget	*w;
	String	label;

	while( (w = diminfo_cur_widget+i) != NULL ) {
		if( *w == widget ) {
			XtVaGetValues( *(diminfo_name_widget+i), 
						XtNlabel, &label, NULL );
			in_change_current( label, MOD_2 );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: diminfo_cur_mod2 callback: can't find " );
	fprintf( stderr, "widget for the pressed button\n" );
	exit( -1 );
}

	void
diminfo_cur_mod3( Widget widget, XButtonEvent *event, String *params, 
							Cardinal *num_params )
{
	int	i = 0;
	Widget	*w;
	String	label;

	while( (w = diminfo_cur_widget+i) != NULL ) {
		if( *w == widget ) {
			XtVaGetValues( *(diminfo_name_widget+i), 
						XtNlabel, &label, NULL );
			in_change_current( label, MOD_3 );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: diminfo_cur_mod3 callback: can't find " );
	fprintf( stderr, "widget for the pressed button\n" );
	exit( -1 );
}

	void
diminfo_cur_mod4( Widget widget, XButtonEvent *event, String *params, 
							Cardinal *num_params )
{
	int	i = 0;
	Widget	*w;
	String	label;

	while( (w = diminfo_cur_widget+i) != NULL ) {
		if( *w == widget ) {
			XtVaGetValues( *(diminfo_name_widget+i), 
						XtNlabel, &label, NULL );
			in_change_current( label, MOD_4 );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: diminfo_cur_mod4 callback: can't find " );
	fprintf( stderr, "widget for the pressed button\n" );
	exit( -1 );
}

        void
restart_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_RESTART, MOD_1 );
}

        void
reverse_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_REWIND, MOD_1 );
}

	void
reverse_mod2( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_REWIND, MOD_2 );
}

	void
back_mod2( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_BACKWARDS, MOD_2 );
}

        void
back_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_BACKWARDS, MOD_1 );
}

        void
pause_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_PAUSE, MOD_1 );
}

        void
forward_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_FORWARD, MOD_1 );
}

	void
forward_mod2( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_FORWARD, MOD_2 );
}

        void
edit_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_EDIT, MOD_1 );
}

        void
info_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_INFO, MOD_1 );
}

        void
ff_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_FASTFORWARD, MOD_1 );
}

	void
fastforward_mod2( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_FASTFORWARD, MOD_2 );
}

        void
invert_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_INVERT_PHYSICAL, MOD_1 );
}

        void
invert_color_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_INVERT_COLORMAP, MOD_1 );
}

        void
blowup_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_BLOWUP, MOD_1 );
}

	void
blowup_mod4( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_BLOWUP, MOD_4 );
}

	void
blowup_mod3( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_BLOWUP, MOD_3 );
}

	void
blowup_mod2( Widget w, XButtonEvent *event, String *params, Cardinal *num_params )
{
	in_button_pressed( BUTTON_BLOWUP, MOD_2 );
}

        void
transform_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	in_button_pressed( BUTTON_TRANSFORM, MOD_1 );
}

        void
varlist_mod1(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	char	*widget_name;

	/* Only respond to 'set' calls, not 'unset' calls... */
	if( call_data == 0 )
		return;

	widget_name = XawToggleGetCurrent( *varlist_widget );
	if( widget_name == NULL )	/* happens because first call to */
		return;			/* ToggleSet unsets the previous */
					/* selection, which is NULL      */
	/* advance past the leading 'varsel_' */
	widget_name += strlen( "varsel_" );
	in_variable_selected( widget_name );
}

	void
x_set_var_sensitivity( char *varname, int sens )
{
	int	i = 0;
	String	label;

	while( (varlist_widget+i) != NULL ) {
		XtVaGetValues( *(varlist_widget+i), XtNlabel, &label, NULL );
		if( strcmp( varname, label ) == 0 ) {
			XtVaSetValues(  *(varlist_widget+i),
					XtNsensitive, sens, NULL );
			if( sens == FALSE ) 
				XtVaSetValues(  *(varlist_widget+i),
					XtNstate, FALSE, NULL );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: x_set_var_sensitivity: can't find " );
	fprintf( stderr, "widget for variable %s\n", varname );
	exit( -1 );
}

	void
x_process_user_input()
{
	for(;;)
		{
		XtAppNextEvent( x_app_context, &event );
		XtDispatchEvent( &event );
		}
}

	void
x_timer_set( XtTimerCallbackProc procedure, XtPointer client_arg )
{
	unsigned long	delay_millisec;

	delay_millisec = (long)(350.0 * options.frame_delay) + 10L;

	timer = XtAppAddTimeOut( 
		x_app_context,
		delay_millisec,
		procedure,
		client_arg );
	timer_enabled = TRUE;
}

	void
x_timer_clear()
{
	if( timer_enabled ) {
		XtRemoveTimeOut( timer );
		timer_enabled = FALSE;
		}
}

	void
x_indicate_active_var( char *var_name )
{
	Widget	*w;
	String	label;
	int	i = 0;

	w = varlist_widget;
	while( *w != NULL ) {
		XtVaGetValues( *w, XtNlabel, &label, NULL );
		if( strcmp( label, var_name ) == 0 ) {
			XtVaGetValues( *(varlist_widget+i), XtNradioData, &label, NULL );
			XawToggleSetCurrent( *varlist_widget, (XtPointer)label );
			return;
			}
		w = (varlist_widget + ++i );
		}

	fprintf( stderr, "ncview: x_indicate_active_var: cannot find " );
	fprintf( stderr, "widget for variable named >%s<\n", var_name );
	exit( -1 );
}

	void
x_indicate_active_dim( int dimension, char *dim_name )
{
	Widget	*w;
	String	label;
	int	i = 0;
	char	new_label[ 132 ];

	if( dimension == DIMENSION_X )
		sprintf( new_label, "X:" );
	else if( dimension == DIMENSION_Y )
		sprintf( new_label, "Y:" );
	else if( dimension == DIMENSION_SCAN )
		sprintf( new_label, "Scan:" );
	else if( dimension == DIMENSION_NONE )
		sprintf( new_label, " " );
	else
		{
		fprintf( stderr, "ncview: x_indicate_active_dim: unknown " );
		fprintf( stderr, "dimension received: %d\n", dimension     );
		exit( -1 );
		}

	i = 0;
	w = diminfo_name_widget;
	while( *w != NULL ) {
		XtVaGetValues( *w, XtNlabel, &label, NULL );
		if( strcmp( label, dim_name ) == 0 ) {
			XtVaSetValues( *(diminfo_dim_widget+i), 
					XtNlabel, new_label,
					XtNwidth, app_data.dimlabel_width, NULL);
			return;
			}
		w = (diminfo_name_widget + ++i );
		}

	fprintf( stderr, "ncview: x_indicate_active_dim: cannot find " );
	fprintf( stderr, "widget for dimension %d, named %s\n", 
							dimension, dim_name );
	exit( -1 );
}

	void
x_set_sensitive( int button_id, int state )
{
	switch( button_id ) {
		case BUTTON_BLOWUP_TYPE:	
			XtVaSetValues( blowup_type_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_RANGE:	
			XtVaSetValues( range_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_DIMSET:	
			XtVaSetValues( dimset_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_RESTART:
			XtVaSetValues( restart_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_REWIND:	
			XtVaSetValues( reverse_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_BACKWARDS:
			XtVaSetValues( backwards_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_PAUSE:
			XtVaSetValues( pause_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_FORWARD:
			XtVaSetValues( forward_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_FASTFORWARD:
			XtVaSetValues( fastforward_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_COLORMAP_SELECT:
			XtVaSetValues( cmap_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_INVERT_PHYSICAL:
			XtVaSetValues( invert_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_INVERT_COLORMAP:
			XtVaSetValues( invert_color_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_BLOWUP:
			XtVaSetValues( blowup_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_TRANSFORM:
			XtVaSetValues( transform_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_EDIT:
			XtVaSetValues( edit_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_INFO:
			XtVaSetValues( info_button_widget, XtNsensitive, state, NULL );
			break;

		case BUTTON_PRINT:
			XtVaSetValues( print_button_widget, XtNsensitive, state, NULL );
			break;
		}
}

	void
x_force_set_invert_state( int state )
{
	XtVaSetValues( invert_button_widget, XtNstate, state, NULL );
}

	void
x_clear_dim_buttons()
{
	Widget	w;
	int	i;

	if( xdimlist_widget == NULL )
		return;

	i=0;
	while( (w = *(xdimlist_widget + i++)) != NULL )
		XtDestroyWidget( w );

	i=0;
	while( (w = *(ydimlist_widget + i++)) != NULL )
		XtDestroyWidget( w );
}

	int
x_set_scan_dims( Stringlist *dim_list, char *x_axis_name, char *y_axis_name,
	Stringlist **new_dim_list )
{
	Position	width, height, root_x, root_y;
	char		*new_xdim_name, *new_ydim_name;
	char		*x_prefix = "xsel_", *y_prefix = "ysel_"; 
	int		i;

	if( options.display_type == TrueColor )
		dimsel_popup_widget = XtVaCreatePopupShell(
			"Dimension Select",
			transientShellWidgetClass,
			topLevel,
			NULL );
	else if( valid_display ) 
		dimsel_popup_widget = XtVaCreatePopupShell(
			"Dimension Select",
			transientShellWidgetClass,
			topLevel,
			XtNcolormap, current_colormap,
			NULL );
	else
		dimsel_popup_widget = XtVaCreatePopupShell(
			"Dimension Select",
			transientShellWidgetClass,
			topLevel,
			NULL );

	dimsel_popupcanvas_widget = XtVaCreateManagedWidget(
		"dimsel_popupcanvas",
		formWidgetClass,
		dimsel_popup_widget,
		XtNborderWidth, 0,
		NULL);

	ydim_selection_widget = XtVaCreateManagedWidget(
		"ydimselectbox",
		boxWidgetClass,
		dimsel_popupcanvas_widget,
		XtNorientation, XtorientHorizontal,
		NULL);

	ydimlist_label_widget = XtVaCreateManagedWidget(
		"ydimlist_label",
		labelWidgetClass,
		ydim_selection_widget,
		XtNwidth, app_data.button_width,
		XtNjustify, XtJustifyRight,
		XtNlabel, "Y Dim:",
		NULL );

	xdim_selection_widget = XtVaCreateManagedWidget(
		"xdimselectbox",
		boxWidgetClass, 
		dimsel_popupcanvas_widget,
		XtNfromVert, ydim_selection_widget,
		XtNorientation, XtorientHorizontal,
		NULL);

	xdimlist_label_widget = XtVaCreateManagedWidget(
		"xdimlist_label",
		labelWidgetClass,
		xdim_selection_widget,
		XtNwidth, app_data.button_width,
		XtNjustify, XtJustifyRight,
		XtNlabel, "X Dim:",
		NULL );

	x_make_dim_button_util( DIMENSION_Y,    dim_list, y_prefix,
			&ydimlist_widget, ydim_selection_widget, y_axis_name );
	x_make_dim_button_util( DIMENSION_X,    dim_list, x_prefix,    
			&xdimlist_widget, xdim_selection_widget, x_axis_name );

	dimsel_ok_button_widget = XtVaCreateManagedWidget(
		"OK",
		commandWidgetClass,
		dimsel_popupcanvas_widget,
		XtNfromVert, xdim_selection_widget,
		NULL);

	dimsel_cancel_button_widget = XtVaCreateManagedWidget(
		"Cancel",
		commandWidgetClass,
		dimsel_popupcanvas_widget,
		XtNfromVert, xdim_selection_widget,
		XtNfromHoriz, dimsel_ok_button_widget,
		NULL);

        XtAddCallback( dimsel_ok_button_widget, XtNcallback, 
			dimsel_callback, (XtPointer)MESSAGE_OK );
        XtAddCallback( dimsel_cancel_button_widget, XtNcallback, 
			dimsel_callback, (XtPointer)MESSAGE_CANCEL );

	XtVaGetValues    ( commandcanvas_widget, XtNwidth,  &width, 
					XtNheight, &height, NULL );
	XtTranslateCoords( commandcanvas_widget, (Position)width,
					(Position)height, &root_x, &root_y );
	XtVaSetValues    ( dimsel_popup_widget, XtNx, root_x/2, XtNy, root_y/2, NULL );
	XtPopup          ( dimsel_popup_widget, XtGrabExclusive );

	if( options.display_type == PseudoColor )
		XSetWindowColormap( XtDisplay(topLevel), XtWindow(dimsel_popup_widget), 
				current_colormap );

	/* This mini main event loop just handles the dimension selection popup */
	dimsel_popup_done = FALSE;
	while( ! dimsel_popup_done ) {
		XtAppNextEvent( x_app_context, &event );
		XtDispatchEvent( &event );
		}

	if( dimsel_popup_result == MESSAGE_OK ) {
		new_ydim_name = XawToggleGetCurrent( *ydimlist_widget );
		new_xdim_name = XawToggleGetCurrent( *xdimlist_widget );

		/* advance past the leading prefixes */
		for( i=strlen(y_prefix); *(new_ydim_name+i) != '\0'; i++ )
			*(new_ydim_name+i-strlen(y_prefix)) = *(new_ydim_name+i);
		*(new_ydim_name+i-strlen(y_prefix)) = '\0';
		for( i=strlen(x_prefix); *(new_xdim_name+i) != '\0'; i++ )
			*(new_xdim_name+i-strlen(x_prefix)) = *(new_xdim_name+i);
		*(new_xdim_name+i-strlen(x_prefix)) = '\0';
		add_to_stringlist( new_dim_list, new_ydim_name, NULL );
		add_to_stringlist( new_dim_list, new_xdim_name, NULL );
		}

	XtPopdown( dimsel_popup_widget );

	XtDestroyWidget( dimsel_cancel_button_widget );
	XtDestroyWidget( dimsel_ok_button_widget   );
	XtDestroyWidget( dimsel_popupcanvas_widget );
	XtDestroyWidget( dimsel_popup_widget       );

	dimsel_popup_widget = NULL;

	return( dimsel_popup_result );
}

        void
dimsel_callback(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	dimsel_popup_result = (int)client_data;
	dimsel_popup_done   = TRUE;
}

	void
x_make_dim_button_util( int dimension, Stringlist *dim_list, char *prefix, 
	Widget **dimlist_widget, Widget parent_widget, char *selected_name )
{
	Widget		*w;
	int	   	i, n_dims;
	Stringlist 	*s;
	char		widget_name[ 64 ];
	String		label;

	/* Make the widget array; set the last element to NULL so that 
	 * we can find it later.
	 */
	n_dims          = n_strings_in_list( dim_list );
	*dimlist_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( *dimlist_widget == NULL ) {
		fprintf( stderr, "ncview: x_make_dim_buttons: malloc ");
		fprintf( stderr, "failed on dimlist_widget initialization\n" );
		exit( -1 );
		}
	*(*dimlist_widget+n_dims) = NULL;

	s = dim_list;
	i = 0;
	while( s != NULL )
		{
		sprintf( widget_name, "%s%s", prefix, s->string );
		if( i == 0 )
			*(*dimlist_widget + i) = XtVaCreateManagedWidget(
				widget_name,
				toggleWidgetClass,
				parent_widget,
				XtNwidth, app_data.button_width,
				XtNlabel, s->string,
				NULL);
		else
			*(*dimlist_widget + i) = XtVaCreateManagedWidget(
				widget_name,
				toggleWidgetClass,
				parent_widget,
				XtNlabel, s->string,
				XtNwidth, app_data.button_width,
				XtNradioGroup, **dimlist_widget,
				NULL);
		s = s->next;
		i++;
		}

	/* Now, set the initial value on the appropriate widget */
	i = 0;
	w = *dimlist_widget;
	while( *w != NULL ) {
		XtVaGetValues( *w, XtNlabel, &label, NULL );
		if( strcmp( label, selected_name ) == 0 ) {
			XtVaGetValues( *(*dimlist_widget+i), XtNradioData, &label, NULL );
			XawToggleSetCurrent( **dimlist_widget, (XtPointer)label );
			return;
			}
		w = (*dimlist_widget + ++i );
		}
	fprintf( stderr, "?didn't find a match for name >%s<\n",
				selected_name );
}

	void
check_app_res( AppDataPtr ad )
{
	if( ad->delta_step > 100 ) {
		fprintf( stderr, "ncview: check_app_data: error in resource " );
		fprintf( stderr, "file entry for deltaStep.  Syntax is:\n" );
		fprintf( stderr, "if deltaStep > 0, then it indicates the " );
		fprintf( stderr, "integer *percent* (from 1 to 100) to step \n");
		fprintf( stderr, "on each press of the forward or backward ");
		fprintf( stderr, "buttons while holding down the Ctrl key.\n");
		fprintf( stderr, "If deltaStep < 0, it indicates the number ");
		fprintf( stderr, "of frames to step in such events.\n" );
		exit( -1 );
		}
	
	if( (ad->button_width > 500) ||
	    (ad->button_width < 10)   ) {
		fprintf( stderr, "ncview: check_app_data: error in resource " );
		fprintf( stderr, "file entry for buttonWidth.  Acceptable\n" );
		fprintf( stderr, "range is 10 to 500\n" );
		exit( -1 );
		}

	if( (ad->label_width > 2000) ||
	    (ad->label_width < 100)   ) {
		fprintf( stderr, "ncview: check_app_data: error in resource " );
		fprintf( stderr, "file entry for labelWidth.  Acceptable\n" );
		fprintf( stderr, "range is 100 to 2000\n" );
		exit( -1 );
		}

	if( (ad->n_vars_per_row > 20) || 
	    (ad->n_vars_per_row < 1 ) ) {
		fprintf( stderr, "ncview: check_app_data: error in resource " );
		fprintf( stderr, "file entry for NVarsPerRow.  Acceptable\n" );
		fprintf( stderr, "range is 1 to 20\n" );
		exit( -1 );
		}

	if( fabs(ad->version - APP_RES_VERSION) > 0.001 ) {
fprintf(stderr, "Error: I cannot find the proper app-defaults file, which\n");
fprintf(stderr, "should be named `Ncview'.  I need an app-defaults file\n" );
fprintf(stderr, "for ncview version %.2f, but have found version %.2f.\n", 
		APP_RES_VERSION, ad->version );
if( fabs(ad->version) < 0.01 )  {
fprintf(stderr, "(Note: a found version of 0.00 means either no\n" ); 
fprintf(stderr, "app-defaults file was found at all, or a pre-version 1.60\n");
fprintf(stderr, "file was found.)\n" );
				}
fprintf(stderr, "To fix this, you must put file `Ncview'\n");
fprintf(stderr, "(NOT the executable, which is named `ncview') either in \n");
fprintf(stderr, "your local $XAPPLRESDIR directory, in the global directory\n");
fprintf(stderr, "/usr/local/lib/X11/app-defaults, or add it to your own\n" );
fprintf(stderr, ".Xdefaults file.\n" );
		exit( -1 );
		}

}

	void
x_error( char *message )
{
	x_dialog( message, NULL, FALSE );
}

	int
x_dialog( char *message, char *ret_string, int want_cancel_button )
{
	Position	width, height, root_x, root_y;
	static Widget	ok_button_widget, cancel_button_widget;

	if( options.display_type == TrueColor )
		error_popup_widget = XtVaCreatePopupShell(
			"popup",
			transientShellWidgetClass,
			topLevel,
			NULL );
	else if( valid_display ) 
		error_popup_widget = XtVaCreatePopupShell(
			"popup",
			transientShellWidgetClass,
			topLevel,
			XtNcolormap, current_colormap,
			NULL );
	else
		error_popup_widget = XtVaCreatePopupShell(
			"popup",
			transientShellWidgetClass,
			topLevel,
			NULL );

	error_popupcanvas_widget = XtVaCreateManagedWidget(
		"error_popupcanvas",
		formWidgetClass,
		error_popup_widget,
		XtNborderWidth, 0,
		NULL);

	if( ret_string != NULL )
		error_popupdialog_widget = XtVaCreateManagedWidget(
			"error_popupdialog",	
			dialogWidgetClass,	
			error_popupcanvas_widget,
			XtNlabel, message,
			XtNborderWidth, 0,
			XtNvalue, ret_string,
			NULL );
	else
		error_popupdialog_widget = XtVaCreateManagedWidget(
			"error_popupdialog",
			dialogWidgetClass,
			error_popupcanvas_widget,
			XtNborderWidth, 0,
			XtNlabel, message,
			NULL );

	ok_button_widget = XtVaCreateManagedWidget(
		"OK",
		commandWidgetClass,
		error_popupdialog_widget,
		NULL);

        XtAddCallback( ok_button_widget,     XtNcallback, 
				error_popup_callback, (XtPointer)MESSAGE_OK);

	if( want_cancel_button ) {
		cancel_button_widget = XtVaCreateManagedWidget(
			"Cancel",
			commandWidgetClass,
			error_popupdialog_widget,
			XtNfromHoriz, ok_button_widget,
			NULL);
	        XtAddCallback( cancel_button_widget, XtNcallback,
				error_popup_callback, (XtPointer)MESSAGE_CANCEL);
		}

	/* Move the dialog to a reasonable location and pop it up */
	XtVaGetValues    ( commandcanvas_widget, XtNwidth,  &width, 
					XtNheight, &height, NULL );
	XtTranslateCoords( commandcanvas_widget, (Position)width,
					(Position)height, &root_x, &root_y );
	XtVaSetValues    ( error_popup_widget, XtNx, root_x/2, XtNy, root_y/2, NULL );
	XtPopup          ( error_popup_widget, XtGrabExclusive );

	if( options.display_type == PseudoColor )
		XSetWindowColormap( XtDisplay(topLevel), XtWindow(error_popup_widget), 
				current_colormap );

	/*
	XtInstallAccelerators( error_popupdialog_widget, ok_button_widget );
	*/
	while( ! error_popup_done ) {
		XtAppNextEvent( x_app_context, &event );
		XtDispatchEvent( &event );
		}
	error_popup_done = FALSE;

	if( ret_string != NULL )
		strcpy( ret_string, XawDialogGetValueString( error_popupdialog_widget ));
	XtPopdown( error_popup_widget );

	if( want_cancel_button )
		XtDestroyWidget( cancel_button_widget );
	XtDestroyWidget( ok_button_widget         );
	XtDestroyWidget( error_popupdialog_widget );
	XtDestroyWidget( error_popupcanvas_widget );
	XtDestroyWidget( error_popup_widget       );
	error_popup_widget = NULL;

	return( error_popup_result );
}

        void
error_popup_callback(widget, client_data, call_data)
Widget widget;
XtPointer client_data;
XtPointer call_data;
{
	error_popup_result = (int)client_data;
	error_popup_done   = TRUE;
}

	void
x_var_set_sensitive( char *var_name, int sensitivity )
{
	Widget 	*w;
	String 	label;
	int	i = 0;

	w = varlist_widget;
	while( w != NULL ) {
		XtVaGetValues( *w, XtNlabel, &label, NULL );
		if( strcmp( label, var_name ) == 0 ) {
			XtVaSetValues( *w, XtNsensitive, sensitivity, NULL );
			return;
			}
		w = (varlist_widget + ++i);
		}

	fprintf( stderr, "ncview: x_var_set_sensitive: could not find " );
	fprintf( stderr, "variable widget corrosponding to variable %s\n",
					var_name );
	exit( -1 );
}

	void
x_fill_dim_info( NCDim *d, int please_flip )
{
	Widget	*w;
	String	widget_name;
	int	i;
	char	temp_label[132];

	/* first, find the row we want */
	i = 0;
	while( (w = diminfo_name_widget + i) != NULL ) {
		XtVaGetValues( *w, XtNlabel, &widget_name, NULL );
		if( strcmp( widget_name, d->name ) == 0 ) {

			if( please_flip )
				sprintf( temp_label, "%g", d->max );
			else
				sprintf( temp_label, "%g", d->min );
			XtVaSetValues( *(diminfo_min_widget+i),
				XtNlabel, temp_label,
				XtNwidth, app_data.dimlabel_width,
				NULL );

			if( please_flip )
				sprintf( temp_label, "%g", d->min );
			else
				sprintf( temp_label, "%g", d->max );
			XtVaSetValues( *(diminfo_max_widget+i),
				XtNlabel, temp_label,
				XtNwidth, app_data.dimlabel_width,
				NULL );

			if( d->units == NULL )
				XtVaSetValues( *(diminfo_units_widget+i),
					XtNlabel, "-",
					XtNwidth, app_data.dimlabel_width,
					NULL );
			else
				XtVaSetValues( *(diminfo_units_widget+i),
					XtNlabel, limit_string(d->units),
					XtNwidth, app_data.dimlabel_width,
					NULL );

			return;
			}
		i++;
		}

	fprintf( stderr, "ncview: x_fill_dim_info: error, can't find " );
	fprintf( stderr, "dim info widget named \"%s\"\n", d->name );
	exit( -1 );
}

	void
x_set_cur_dim_value( char *dim_name, char *string )
{
	int	i;
	Widget	*w;
	String	label;

	i = 0;
	while( (w = diminfo_name_widget+i) != NULL ) {
		XtVaGetValues( *w, XtNlabel, &label, NULL );
		if( strcmp( label, dim_name ) == 0 ) {
			XtVaSetValues( *(diminfo_cur_widget+i), 
				XtNlabel, string, NULL );
			return;
			}
		i++;
		}
	fprintf( stderr, "ncview: x_set_cur_dim: error; widget for dimension ");
	fprintf( stderr, "named \"%s\" not found.\n", dim_name );
	exit( -1 );
}

	void
x_init_dim_info( Stringlist *dims )
{
	int	i, n_dims;
	char	widget_name[128];
	Dimension bb_width, bb_height;

	XtVaGetValues( labels_row_widget, XtNwidth,  &bb_width,
					  XtNheight, &bb_height, NULL );

	x_clear_dim_info();
	n_dims = n_strings_in_list( dims );
	diminfo_row_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_row_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_row widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_dim_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_dim_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_dim widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_name_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_name_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_name widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_min_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_min_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_min widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_cur_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_cur_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_cur widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_max_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_max_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_max widgets",
							n_dims );
		exit( -1 );
		}
	diminfo_units_widget = (Widget *)malloc( (n_dims+1)*sizeof(Widget));
	if( diminfo_units_widget == NULL ) {
		fprintf( stderr, "ncview: x_init_dim_info: malloc failed ");
		fprintf( stderr, "initializing %d diminfo_units widgets",
							n_dims );
		exit( -1 );
		}
	/* Mark the end of the arrays by a 'NULL' */
	*(diminfo_row_widget   + n_dims) = NULL;
	*(diminfo_dim_widget   + n_dims) = NULL;
	*(diminfo_name_widget  + n_dims) = NULL;
	*(diminfo_min_widget   + n_dims) = NULL;
	*(diminfo_cur_widget   + n_dims) = NULL;
	*(diminfo_max_widget   + n_dims) = NULL;
	*(diminfo_units_widget + n_dims) = NULL;

	for( i=0; i<n_dims; i++ )
		{
                sprintf( widget_name, "diminfo_row_%1d", i );
                if( i == 0 )
			*(diminfo_row_widget+i) = XtVaCreateManagedWidget(
				widget_name,
				boxWidgetClass,
				commandcanvas_widget,
				XtNorientation, XtorientHorizontal,
				XtNfromVert, labels_row_widget,
				XtNheight, bb_height,
				XtNwidth,  bb_width,
				NULL);
		else
			*(diminfo_row_widget+i) = XtVaCreateManagedWidget(
				widget_name,
				boxWidgetClass,
				commandcanvas_widget,
				XtNorientation, XtorientHorizontal,
				XtNfromVert, *(diminfo_row_widget + (i-1)),
				XtNheight, bb_height,
				XtNwidth,  bb_width,
				NULL);

		sprintf( widget_name, "diminfo_dim_%1d", i );
		*(diminfo_dim_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			labelWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, "",
			XtNjustify, XtJustifyRight,
			XtNwidth, 50,
			XtNborderWidth, 0,
			NULL);

		sprintf( widget_name, "diminfo_name_%1d", i );
		*(diminfo_name_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			labelWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, dims->string,
			XtNwidth, app_data.dimlabel_width,
			XtNborderWidth, 0,
			NULL);

		sprintf( widget_name, "diminfo_min_%1d", i );
		*(diminfo_min_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			labelWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, "Min:",
			XtNwidth, app_data.dimlabel_width,
			XtNborderWidth, 0,
			NULL);

		sprintf( widget_name, "diminfo_cur_%1d", i );
		*(diminfo_cur_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			commandWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, "Current:",
			XtNwidth, app_data.dimlabel_width,
			NULL);
        	XtAddCallback( *(diminfo_cur_widget+i), XtNcallback, 
      					diminfo_cur_mod1, (XtPointer)i );

		/* Add the modifications for the created curr_dimension button;
		 * Button-3 gives mod3 and holding down control gives mod2.
		 */
		XtAugmentTranslations( *(diminfo_cur_widget+i), 
			XtParseTranslationTable( 
				"<Btn3Down>,<Btn3Up>: diminfo_cur_mod3()" ));
		/* XtAugmentTranslations( *(diminfo_cur_widget+i),  */
		XtOverrideTranslations( *(diminfo_cur_widget+i), 
			XtParseTranslationTable( 
				"Ctrl<Btn1Down>,<Btn1Up>: diminfo_cur_mod2()" ));
		XtOverrideTranslations( *(diminfo_cur_widget+i), 
			XtParseTranslationTable( 
				"Ctrl<Btn3Down>,<Btn3Up>: diminfo_cur_mod4()" ));

		sprintf( widget_name, "diminfo_max_%1d", i );
		*(diminfo_max_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			labelWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, "Max:",
			XtNwidth, app_data.dimlabel_width,
			XtNborderWidth, 0,
			NULL);

		sprintf( widget_name, "diminfo_units_%1d", i );
		*(diminfo_units_widget+i) = XtVaCreateManagedWidget(
			widget_name,
			labelWidgetClass,
			*(diminfo_row_widget+i),
			XtNlabel, "Units:",
			XtNwidth, app_data.dimlabel_width,
			XtNborderWidth, 0,
			NULL);

		dims = dims->next;
		}
}	
	void
x_clear_dim_info()
{
	Widget	w;
	int	i;

	if( diminfo_row_widget == NULL )
		return;

	i=0;
	while( (w = *(diminfo_row_widget + i++)) != NULL )
		XtDestroyWidget( w );
}

	void
x_set_cursor_busy()
{
	if( busy_cursor == (Cursor)NULL )
		return;

	XDefineCursor( XtDisplay( commandcanvas_widget ),
	   	    XtWindow( commandcanvas_widget ), busy_cursor );

	
	if( ccontour_popped_up )
		XDefineCursor( XtDisplay( ccontour_widget ),
	   	    XtWindow( ccontour_widget ), busy_cursor );

	if( dimsel_popup_widget != NULL )
		XDefineCursor( XtDisplay( ccontour_widget ),
	   	    XtWindow( dimsel_popup_widget ), busy_cursor );

	XFlush( XtDisplay( commandcanvas_widget ));
}

	void
x_set_cursor_normal()
{
	if( busy_cursor == (Cursor)NULL )
		return;

	XUndefineCursor( XtDisplay( commandcanvas_widget ),
	   	    XtWindow( commandcanvas_widget ) );

	if( ccontour_popped_up )
		XUndefineCursor( XtDisplay( ccontour_widget ),
	   	    XtWindow( ccontour_widget ) ); 

	if( dimsel_popup_widget != NULL )
		XUndefineCursor( XtDisplay( ccontour_widget ),
	   	    XtWindow( dimsel_popup_widget ) );

}

	void
new_cmaplist( Cmaplist **cml )
{
	(*cml) = (Cmaplist *)malloc( sizeof( Cmaplist ) );
	(*cml)->color_list = (XColor *)malloc( (options.n_colors+N_EXTRA_COLORS)*sizeof(XColor) );
	(*cml)->next       = NULL;
	(*cml)->prev       = NULL;
	(*cml)->name       = NULL;
	(*cml)->pixel_transform = (ncv_pixel *)malloc( (options.n_colors+N_EXTRA_COLORS)*sizeof(ncv_pixel) );
}

	void
x_set_windows_colormap_to_current( Widget w )
{
	XSetWindowColormap( XtDisplay(topLevel), XtWindow(w), current_colormap );
}

	void
x_get_window_position( int *llx, int *lly, int *urx, int *ury )
{
	Dimension	width, height;
	Position	x, y;

	XtVaGetValues    ( commandcanvas_widget, XtNwidth,  &width, 
					XtNheight, &height, NULL );
	XtTranslateCoords( commandcanvas_widget, (Position)width,
					(Position)height, &x, &y );
	*urx = (int)x;
	*ury = (int)y;
	XtTranslateCoords( commandcanvas_widget, (Position)0,
					(Position)0, &x, &y );
	*llx = (int)x;
	*lly = (int)y;
}

	void
pix_to_rgb( ncv_pixel pix,  int *r, int *g, int *b )
{
	*r = (current_colormap_list->color_list+pix)->red;
	*g = (current_colormap_list->color_list+pix)->green;
	*b = (current_colormap_list->color_list+pix)->blue;
}

	void
x_popup_2d_window()
{
	Position  root_x, root_y;
	Dimension width, height;
	static int 	first_time = 1;

	if( first_time ) {
		/* Move the ccontourpanel to a reasonable location */
		XtVaGetValues    ( commandcanvas_widget, XtNwidth,  &width, XtNheight, &height, NULL );
		XtTranslateCoords( commandcanvas_widget, (Position)1, (Position)height, &root_x, &root_y );
		XtVaSetValues    ( ccontourpanel_widget, XtNx, root_x, XtNy, root_y, NULL );
		first_time = 0;
		}

	XtPopup( ccontourpanel_widget, XtGrabNone );

	ccontour_popped_up = TRUE;
}

	void
x_popdown_2d_window()
{
	XtPopdown( ccontourpanel_widget );
	ccontour_popped_up = FALSE;
}

        void
expose_ccontour( Widget w, XtPointer client_data, XExposeEvent *event, Boolean *continue_to_dispatch )
{
	if( event->type != Expose ) {
		fprintf( stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" );
		fprintf( stderr, "Got a non-expose event to expose_ccontour!\n" );
		fprintf( stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" );
		return;
		}

	if( (event->count == 0) && (event->width > 1) && (event->height > 1))
		view_draw( TRUE );
}

	void 	
varsel_menu_select(Widget w, XtPointer client_data, XtPointer call_data )
{
	NCVar 	*var;
	int 	i, menu_item;

	menu_item = (int)client_data;
	printf( "picked menu item %d\n", menu_item );

	var = variables;
	for( i=0; i<menu_item; i++ )
		var = var->next;

	printf( "picked variable %s\n", var->name );
	in_variable_selected( var->name );
}

void 	testf(Widget w, XButtonEvent *e, String *p, Cardinal *n )
{
	printf( "in testf!\n" );
}

	void
dump_to_ppm( unsigned char *data, size_t width, size_t height )
{
	pixel   **temp_array, ppm_pixel;
	pixval	r, g, b, max;
	int	i, j, pix;
	char	filename[2048];
	static  int frameno=0;
	FILE	*out_file;

	sprintf( filename, "frame.%06d.ppm", frameno );
	frameno++;

	if( (out_file = fopen( filename, "w" )) == NULL ) {
		fprintf( stderr, "can't open file %s for writing\n", filename );
		return;
		}

	temp_array = ppm_allocarray(width, height );
	for( j=0; j<height; j++ )
	for( i=0; i<width;  i++ ) {
		pix = *(data+i+j*width);
		r = (pixval)((current_colormap_list->color_list+pix)->red >> 8);
		g = (pixval)((current_colormap_list->color_list+pix)->green >> 8);
		b = (pixval)((current_colormap_list->color_list+pix)->blue >> 8);
/*
printf( "%d ", pix);
printf( "(%d %d %d %d) ", (int)pix, (int)(current_colormap_list->color_list+pix)->red, (int)(current_colormap_list->color_list+pix)->green, (int)(current_colormap_list->color_list+pix)->blue );
*/
		PPM_ASSIGN( ppm_pixel, r, g, b );
		*(temp_array[j]+i) = ppm_pixel;
		}	

	max = PPM_MAXMAXVAL;
	ppm_writeppm( out_file, temp_array, width, height, max, FALSE );

	fclose(out_file);
	ppm_freearray( temp_array, height );
}

