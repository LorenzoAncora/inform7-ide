/*  Copyright 2006 P.F. Chimento
 *  This file is part of GNOME Inform 7.
 * 
 *  GNOME Inform 7 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Inform 7 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Inform 7; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include <gnome.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gtksourceview/gtksourcebuffer.h>

#include "story.h"
#include "interface.h"
#include "support.h"
#include "tabsource.h"
#include "windowlist.h"
#include "prefs.h"
#include "configfile.h"
#include "skein.h"
#include "tabgame.h"

/* This is the list of all story structures that are currently allocated */
static GSList *storylist = NULL;

/* Create and initialize a new story structure, with main window and source
buffer and skein, etc. */
struct story *new_story() {
    struct story *newstory = g_malloc(sizeof(struct story));

    newstory->filename = NULL;
    newstory->window = create_app_window();
    newstory->buffer = create_natural_inform_source_buffer();

    /* Connect the source buffer to both of our sourceviews */
    gtk_text_view_set_buffer(
      GTK_TEXT_VIEW(lookup_widget(newstory->window, "source_l")),
      GTK_TEXT_BUFFER(newstory->buffer));
    gtk_text_view_set_buffer(
      GTK_TEXT_VIEW(lookup_widget(newstory->window, "source_r")),
      GTK_TEXT_BUFFER(newstory->buffer));

    /* Do the default settings */
    newstory->story_format = FORMAT_Z5;
    newstory->make_blorb = FALSE;
    newstory->interp_running = FALSE;
    newstory->interp_process = 0;
    newstory->handler_child_exit = 0;
    newstory->handler_commit = 0;
    newstory->release = FALSE;
    newstory->run = FALSE;
    
    /* Create an empty skein */
    newstory->theskein = create_skein();
    newstory->skein_ptr = get_start_pointer(newstory->theskein);

    storylist = g_slist_append(storylist, (gpointer)newstory);
    update_window_list();
    
    return newstory;
}

/* Helper function to delete a file relative to the project path */
static void delete_from_project_dir(struct story *thestory, gchar *filename) {
    gchar *pathname = g_strconcat(thestory->filename, filename, NULL);
    g_remove(pathname);
    g_free(pathname);
}

/* Free all the resources from the story */
void delete_story(struct story *oldstory) {
    storylist = g_slist_remove(storylist, (gconstpointer)oldstory);

    stop_project(oldstory);
    
    if(oldstory->filename != NULL) {
        if(config_file_get_bool("Cleaning", "BuildFiles")) {
            delete_from_project_dir(oldstory, "/Metadata.iFiction");
            delete_from_project_dir(oldstory, "/Release.blurb");
            delete_from_project_dir(oldstory, "/Build/auto.inf");
            delete_from_project_dir(oldstory, "/Build/Debug log.txt");
            delete_from_project_dir(oldstory, "/Build/Map.eps");
            delete_from_project_dir(oldstory, "/Build/output.z5");
            delete_from_project_dir(oldstory, "/Build/output.z8");
            delete_from_project_dir(oldstory, "/Build/output.ulx");
            delete_from_project_dir(oldstory, "/Build/Problems.html");
            delete_from_project_dir(oldstory, "/Build/temporary file.inf");
            
            if(config_file_get_bool("Cleaning", "IndexFiles")) {
                delete_from_project_dir(oldstory, "/Index/Actions.html");
                delete_from_project_dir(oldstory, "/Index/Contents.html");
                delete_from_project_dir(oldstory, "/Index/Headings.xml");
                delete_from_project_dir(oldstory, "/Index/Kinds.html");
                delete_from_project_dir(oldstory, "/Index/Phrasebook.html");
                delete_from_project_dir(oldstory, "/Index/Rules.html");
                delete_from_project_dir(oldstory, "/Index/Scenes.html");
                delete_from_project_dir(oldstory, "/Index/World.html");
            }
        }
        free(oldstory->filename);
    }
    gtk_widget_destroy(oldstory->window);
    destroy_skein(oldstory->theskein);
    g_free(oldstory);
    
    update_window_list();
}

/* Returns the story struct associated with the main window that is the toplevel
of widget */
struct story *get_story(GtkWidget *widget) {
    GSList *iter = storylist;
    /* the following is because the menu items do not share the same toplevel
     as the other widgets? */
    GtkWidget *topwindow = lookup_widget(widget, "app_window");

    while(iter != NULL) {
        if(((struct story *)(iter->data))->window == topwindow)
            return (struct story *)(iter->data);
        iter = g_slist_next(iter);
    }
    return NULL;
}

/* Format and set the filename of a story struct */
void set_story_filename(struct story *thestory, gchar *filename) {
    if(thestory->filename)
        g_free(thestory->filename);
    thestory->filename = g_strdup(filename);
    gtk_window_set_title(GTK_WINDOW(thestory->window),
      strrchr(filename, '/') + 1);
    update_window_list();
}

/* Carry out func for each story window */
void for_each_story_window(void (*func)(GtkWidget *)) {
    GSList *iter;
    for(iter = storylist; iter != NULL; iter = g_slist_next(iter))
        func(((struct story *)(iter->data))->window);
}

/* Carry out func for each story buffer */
void for_each_story_buffer(void (*func)(GtkSourceBuffer *)) {
    GSList *iter;
    for(iter = storylist; iter != NULL; iter = g_slist_next(iter))
        func(((struct story *)(iter->data))->buffer);
}

/* Return the extension of the output file of this story */
gchar *get_story_extension(struct story *thestory) {
    switch(thestory->story_format) {
        case FORMAT_Z5:
            return "z5";
        case FORMAT_Z8:
            return "z8";
        case FORMAT_GLULX:
            return "ulx";
    }
    return "error";
}