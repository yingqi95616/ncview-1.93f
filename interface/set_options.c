/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993 through 2008 David W. Pierce
 *
 * This program  is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 3, as 
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


#include "../ncview.includes.h"
#include "../ncview.defines.h"
#include "../ncview.protos.h"

#include "RadioWidget.h"

#define OVERLAY_FILENAME_WIDGET_WIDTH	500

#define OVERLAY_FILENAME_LEN		1024

static char overlay_filename[ OVERLAY_FILENAME_LEN ];

extern Widget		topLevel;
extern XtAppContext	x_app_context;
extern Options		options;
extern Pixmap 		open_circle_pixmap, closed_circle_pixmap; /* set in x_interface.c */

static Widget
	opt_popup_widget,
		opt_popupcanvas_widget,
			opt_overlays_box_widget,
				opt_overlays_label_widget,
				opt_overlays_filename_box,
					opt_overlays_filename_spacer,
					opt_overlays_filename_widget,
			opt_shrink_box_widget,
				opt_shrink_label_widget,
			opt_action_box_widget,
				opt_ok_widget,
				opt_cancel_widget;

static RadioWidget overlay_widget, shrink_widget;

static int opt_popup_done = FALSE, opt_popup_result;

static XEvent	opt_event;

void opt_popup_callback		( Widget widget, XtPointer client_data, XtPointer call_data);
void overlays_filename_callback ( Widget widget, XtPointer client_data, XtPointer call_data);
void overlay_set_callback 	( Widget widget, XtPointer client_data, XtPointer call_data);
void overlay_unset_callback 	( Widget widget, XtPointer client_data, XtPointer call_data);

void shrink_set_callback 	( Widget widget, XtPointer client_data, XtPointer call_data);
void shrink_unset_callback 	( Widget widget, XtPointer client_data, XtPointer call_data);

/* Order of these must match defines of SHRINK_METHOD_MEAN and SHRINK_METHOD_MODE
 * in file ../ncview.defines.h!
 */
static char *shrink_labels[] = { "Average (Mean)", "Most common value (Mode)" };

/********************************************************************************************/
	void
set_options( void )
{
	int	llx, lly, urx, ury;
	int	current_overlay, new_overlay, current_shrink, new_shrink;

	current_overlay = overlay_current(); 
	RadioWidget_set_current( overlay_widget, current_overlay );

	current_shrink = options.shrink_method;
	RadioWidget_set_current( shrink_widget, current_shrink );

	/********** Pop up the window **********/
	x_get_window_position( &llx, &lly, &urx, &ury );
	XtVaSetValues( opt_popup_widget, XtNx, llx+10, XtNy, (ury+lly)/2, NULL );
	XtPopup      ( opt_popup_widget, XtGrabExclusive );

	if( options.display_type == PseudoColor )
		x_set_windows_colormap_to_current( opt_popup_widget );

	/**********************************************
	 * Main loop for the options popup widget
	 */
	while( ! opt_popup_done ) {
		/* An event will cause opt_popup_done to become TRUE */
		XtAppNextEvent( x_app_context, &opt_event );
		XtDispatchEvent( &opt_event );
		}
	opt_popup_done = FALSE;

	/**********************************************/

	XtPopdown( opt_popup_widget );


	/* Process our result, if user pressed 'OK'.  (Discard our results if
	 * user pressed 'Cancel'
	 */
	if( opt_popup_result == MESSAGE_OK ) {

		/* Do overlay */
		new_overlay = RadioWidget_query_current( overlay_widget );
		if( (new_overlay != current_overlay) || (new_overlay == overlay_custom_n()) ) {
			do_overlay( new_overlay, overlay_filename, FALSE );
			}
		else
			; /* printf( "no changes in overlay\n" ); */

		/* Do shrink */
		new_shrink = RadioWidget_query_current( shrink_widget );
		if( new_shrink != current_shrink ) {
			options.shrink_method = new_shrink;
			invalidate_all_saveframes();
			change_view( 0, FRAMES );
			}
		}
	else if( opt_popup_result == MESSAGE_CANCEL ) {
		;
		}
	else
		{
		fprintf( stderr, "Error, got unknown message from options widget!\n" );
		exit(-1);
		}
}

/********************************************************************************************/
	void
set_options_init()
{
	opt_popup_widget = XtVaCreatePopupShell(
		"Set Options",
		transientShellWidgetClass,
		topLevel,
		NULL );

	opt_popupcanvas_widget = XtVaCreateManagedWidget(
		"opt_popupcanvas",
		formWidgetClass,
		opt_popup_widget,
		NULL);

	opt_overlays_box_widget = XtVaCreateManagedWidget(
		"opt_overlays_box_widget",	
		formWidgetClass,	
		opt_popupcanvas_widget,
		XtNorientation, XtorientVertical,
		NULL );

		opt_overlays_label_widget = XtVaCreateManagedWidget(
			"opt_overlays_label_widget",	
			labelWidgetClass,	
			opt_overlays_box_widget,
			XtNlabel, "Overlays:",
			XtNborderWidth, 0, 
			NULL );
	
		/* REMEMBER THIS IS **NOT** A WIDGET!! */
		overlay_widget = RadioWidget_init( 
			opt_overlays_box_widget,	/* parent */
			opt_overlays_label_widget, 	/* widget to be fromVert */
			overlay_n_overlays(), 
			overlay_current(), 
			overlay_names(), 
			&open_circle_pixmap, &closed_circle_pixmap,
			overlay_set_callback, overlay_unset_callback );

		opt_overlays_filename_box = XtVaCreateManagedWidget(
			"opt_overlays_filename_box",	
			boxWidgetClass,	
			opt_overlays_box_widget,
			XtNorientation, XtorientHorizontal,
			XtNfromVert, overlay_widget->widget,
			XtNborderWidth, 0, 
			NULL );

			opt_overlays_filename_spacer = XtVaCreateManagedWidget(
				"opt_overlays_filename_spacer",	
				labelWidgetClass,	
				opt_overlays_filename_box,
				XtNlabel, "     ",
				XtNborderWidth, 0, 
				NULL );

			opt_overlays_filename_widget = XtVaCreateManagedWidget(
				"opt_overlays_filename_widget",	
				commandWidgetClass,	
				opt_overlays_filename_box,
				XtNwidth, OVERLAY_FILENAME_WIDGET_WIDTH,
				XtNjustify, XtJustifyLeft,
				XtNsensitive, FALSE,
				XtNlabel, "Click to select custom overlay file",
				NULL );

		 XtAddCallback( opt_overlays_filename_widget, XtNcallback,
			overlays_filename_callback, (XtPointer)NULL );
	
	opt_shrink_box_widget = XtVaCreateManagedWidget(
		"opt_shrink_box_widget",	
		formWidgetClass,	
		opt_popupcanvas_widget,
		XtNorientation, XtorientVertical,
		XtNfromVert, opt_overlays_box_widget,
		NULL );

		opt_shrink_label_widget = XtVaCreateManagedWidget(
			"opt_shrink_label_widget",	
			labelWidgetClass,	
			opt_shrink_box_widget,
			XtNlabel, "Method to use when shrinking data:",
			XtNborderWidth, 0, 
			NULL );

		shrink_widget = RadioWidget_init( 
			opt_shrink_box_widget,		/* parent */
			opt_shrink_label_widget, 	/* widget to be fromVert */
			2,
			0,
			shrink_labels,
			&open_circle_pixmap, &closed_circle_pixmap,
			NULL, NULL );

	opt_action_box_widget = XtVaCreateManagedWidget(
		"opt_action_box_widget",	
		boxWidgetClass,	
		opt_popupcanvas_widget,
		XtNorientation, XtorientHorizontal,
		XtNfromVert, opt_shrink_box_widget,
		NULL );
	
		opt_ok_widget = XtVaCreateManagedWidget(
			"OK",	
			commandWidgetClass,	
			opt_action_box_widget,
			NULL );
	
       	 	XtAddCallback( opt_ok_widget, XtNcallback, 
			opt_popup_callback, (XtPointer)MESSAGE_OK );
	
		opt_cancel_widget = XtVaCreateManagedWidget(
			"Cancel",	
			commandWidgetClass,	
			opt_action_box_widget,
			NULL );

       	 	XtAddCallback( opt_cancel_widget, XtNcallback, 
			opt_popup_callback, (XtPointer)MESSAGE_CANCEL );

}

/********************************************************************************************/
        void
opt_popup_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	opt_popup_result = (int)client_data;
	opt_popup_done   = TRUE;
}

/********************************************************************************************/
        void
overlays_filename_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	int 	message;
	char	*filename, overlay_base_dir[1024];

	determine_overlay_base_dir( overlay_base_dir, 1024 );
	message = file_select( &filename, overlay_base_dir );
	if( message == MESSAGE_OK ) {
		if( strlen(filename) >= OVERLAY_FILENAME_LEN ) {
			fprintf( stderr, "Error, length of overlay filename is too long.  Length=%ld, max=%d\n",
				strlen(filename), OVERLAY_FILENAME_LEN );
			exit(-1);
			}
		strcpy( overlay_filename, filename );
		options_set_overlay_filename( overlay_filename );
		}
	else
		overlay_filename[0] = '\0';
}

/********************************************************************************************/
	void
options_set_overlay_filename( char *fn )
{
        XtVaSetValues( opt_overlays_filename_widget, XtNlabel, fn, 
			XtNwidth, OVERLAY_FILENAME_WIDGET_WIDTH, NULL );
}

/********************************************************************************************/
        void
overlay_set_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	int butno;

	butno = (int)client_data;

	if( butno == 4 )
		XtVaSetValues( opt_overlays_filename_widget, XtNsensitive, TRUE, NULL );
}

/********************************************************************************************/
        void
overlay_unset_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	int butno;

	butno = (int)client_data;

	if( butno == 4 )
		XtVaSetValues( opt_overlays_filename_widget, XtNsensitive, FALSE, NULL );
}

/********************************************************************************************/
        void
shrink_set_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	printf("entering shrink_set callback with client_data=%d\n", (int)client_data );
}

/********************************************************************************************/
        void
shrink_unset_callback( Widget widget, XtPointer client_data, XtPointer call_data)
{
	printf("entering shrink_unset callback with client_data=%d\n", (int)client_data );
}

