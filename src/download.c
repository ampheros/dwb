/*
 * Copyright (c) 2010-2011 Stefan Bolte <portix@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "download.h"

typedef struct _DwbDownloadStatus {
  guint blue;
  gint64 time;
} DwbDownloadStatus;

typedef struct _DwbDownload {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  WebKitDownload *download;
  DownloadAction action;
  char *path;
} DwbDownload;

static GList *downloads = NULL;
static char *lastdir = NULL;

/*  dwb_get_command_from_mimetype(char *mimetype){{{*/
static char *
download_get_command_from_mimetype(char *mimetype) {
  char *command = NULL;
  for (GList *l = dwb.fc.mimetypes; l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(n->first, mimetype)) {
      command = n->second;
      break;
    }
  }
  return command;
}/*}}}*/

/* dwb_get_download_command {{{*/
static char *
download_get_command(const char *uri, const char *output) {
  char *command = g_strdup(GET_CHAR("download-external-command"));
  char *newcommand = NULL;

  if ( (newcommand = util_string_replace(command, "dwb_uri", uri)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( (newcommand = util_string_replace(command, "dwb_cookies", dwb.files.cookies)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( (newcommand = util_string_replace(command, "dwb_output", output)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( GET_BOOL("use-fifo") && (newcommand = util_string_replace(command, "dwb_fifo", dwb.files.fifo)) ) {
    FREE(command);
    command = newcommand;
  }
  return command;
}/*}}}*/

/* download_get_download_label(WebKitDownload *) {{{*/
static GList * 
download_get_download_label(WebKitDownload *download) {
  for (GList *l = downloads; l; l=l->next) {
    DwbDownload *label = l->data;
    if (label->download == download) {
      return l;
    }
  }
  return NULL;
}/*}}}*/

/* download_progress_cb(WebKitDownload *) {{{*/
static void
download_progress_cb(WebKitDownload *download, GParamSpec *p, DwbDownloadStatus *status) {
  GList *l = download_get_download_label(download); 
  DwbDownload *label = l->data;
  /* Update at most four times a second */
  gint64 time = g_get_monotonic_time()/250000;
  if (time != status->time) {
    double elapsed = webkit_download_get_elapsed_time(download);
    double progress = webkit_download_get_progress(download);
    double total_size = (double)webkit_download_get_total_size(download) / 0x100000;


    double current_size = (double)webkit_download_get_current_size(download) / 0x100000;
    guint remaining = (guint)(elapsed / progress - elapsed);
    char *message = g_strdup_printf("[%d:%02d][%d%%][%.3f/%.3f]", remaining/60, remaining%60,  (int)(progress*100), current_size,  total_size);
    gtk_label_set_text(GTK_LABEL(label->rlabel), message);
    FREE(message);

    guint blue = ((1 - progress) * 0xaa);
    if (blue != status->blue) {
      guint green = progress * 0xaa;
      char *colorstring = g_strdup_printf("#%02x%02x%02x", 0x00, green, blue);

      DwbColor color; 

      DWB_COLOR_PARSE(&color, colorstring);
      DWB_WIDGET_OVERRIDE_BACKGROUND(label->event, GTK_STATE_NORMAL, &color);

      FREE(colorstring);
    }
    status->blue = blue;
  }
  status->time = time;
}/*}}}*/

/* download_set_mimetype(const char *) {{{*/
static void
download_set_mimetype(const char *command) {
  GList *list = NULL;
  if (dwb.state.mimetype_request) {
    Navigation *n = dwb_navigation_new(dwb.state.mimetype_request, command);
    if ( (list = g_list_find_custom(dwb.fc.mimetypes, n, (GCompareFunc)util_navigation_compare_first))) {
      g_free(list->data);
      dwb.fc.mimetypes = g_list_delete_link(dwb.fc.mimetypes, list);
    }
    dwb.fc.mimetypes = g_list_prepend(dwb.fc.mimetypes, n);
    util_file_add_navigation(dwb.files.mimetypes, n, true, -1);
  }
}/*}}}*/

/* download_spawn(DwbDownload *) {{{*/
static void 
download_spawn(DwbDownload *dl) {
  GError *error = NULL;
  const char *filename = webkit_download_get_destination_uri(dl->download);
  char *command = g_strconcat(dl->path, " ", filename + 7, NULL);

  char **argv = g_strsplit(command, " ", -1);
  if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) ) {
    fprintf(stderr, "Couldn't open %s with %s : %s\n", filename + 7, dl->path, error->message);
    g_clear_error(&error);
  }
  download_set_mimetype(dl->path);
  FREE(command);
  g_strfreev(argv);
}/*}}}*/

/* download_status_cb(WebKitDownload *) {{{*/
static void
download_status_cb(WebKitDownload *download, GParamSpec *p, DwbDownloadStatus *dstatus) {
  WebKitDownloadStatus status = webkit_download_get_status(download);

  if (status == WEBKIT_DOWNLOAD_STATUS_FINISHED || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED) {
    GList *list = download_get_download_label(download);
    if (list) {
      DwbDownload *label = list->data;
      if (label->action == DL_ACTION_EXECUTE && status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
        download_spawn(label);
      }
      gtk_widget_destroy(label->event);
      FREE(label->path);
      downloads = g_list_delete_link(downloads, list);
    }
    if (!downloads) {
      gtk_widget_hide(dwb.gui.downloadbar);
    }
    if (dwb.state.mimetype_request) {
      g_free(dwb.state.mimetype_request);
      dwb.state.mimetype_request = NULL;
    }
    g_free(dstatus);
  }
}/*}}}*/

/* download_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *) {{{*/
static gboolean 
download_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *gl) {
  if (e->button == 3) {
    DwbDownload *label = gl->data;
    webkit_download_cancel(label->download);
  }
  return false;
}/*}}}*/

/* download_add_progress_label (GList *gl, const char *filename) {{{*/
static DwbDownload *
download_add_progress_label(GList *gl, const char *filename) {
  DwbDownload *l = g_malloc(sizeof(DwbDownload));

  GtkWidget *hbox = gtk_hbox_new(false, 5);
  l->event = gtk_event_box_new();
  l->rlabel = gtk_label_new("???");
  char *escaped  = g_markup_escape_text(filename, -1);
  l->llabel = gtk_label_new(escaped);

  gtk_box_pack_start(GTK_BOX(hbox), l->llabel, false, false, 1);
  gtk_box_pack_start(GTK_BOX(hbox), l->rlabel, false, false, 1);
  gtk_container_add(GTK_CONTAINER(l->event), hbox);
  gtk_box_pack_start(GTK_BOX(dwb.gui.downloadbar), l->event, false, false, 2);

  gtk_label_set_use_markup(GTK_LABEL(l->llabel), true);
  gtk_label_set_use_markup(GTK_LABEL(l->rlabel), true);
  gtk_misc_set_alignment(GTK_MISC(l->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(l->rlabel), 1.0, 0.5);

  DWB_WIDGET_OVERRIDE_COLOR(l->llabel, GTK_STATE_NORMAL, &dwb.color.download_fg);
  DWB_WIDGET_OVERRIDE_COLOR(l->rlabel, GTK_STATE_NORMAL, &dwb.color.download_fg);

  DWB_WIDGET_OVERRIDE_BACKGROUND(l->event, GTK_STATE_NORMAL, &dwb.color.download_bg);

  DWB_WIDGET_OVERRIDE_FONT(l->llabel, dwb.font.fd_active);
  DWB_WIDGET_OVERRIDE_FONT(l->rlabel, dwb.font.fd_active);

  l->download = dwb.state.download;

  gtk_widget_show_all(dwb.gui.downloadbar);
  return l;
}/*}}}*/

/* download_start {{{*/
void 
download_start() {
  const char *path = GET_TEXT();
  char *fullpath;
  const char *filename = webkit_download_get_suggested_filename(dwb.state.download);
  const char *uri = webkit_download_get_uri(dwb.state.download);
  char *command = NULL;
  gboolean external = GET_BOOL("download-use-external-program");
  
  if (g_str_has_prefix(uri, "file://") && g_file_test(uri + 7, G_FILE_TEST_EXISTS)) {
    GError *error = NULL;
    char *command = g_strconcat(path, " ", uri + 7, NULL);
    char **argv = g_strsplit(command, " ", -1);
    if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) ) {
      fprintf(stderr, "Couldn't open %s with %s : %s\n", uri + 7, path, error->message);
      g_clear_error(&error);
    }
    download_set_mimetype(path);
    g_strfreev(argv);
    g_free(command);

    return;
  }

  if (!filename || !strlen(filename)) {
    filename = "dwb_download";
  }

  if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
    fullpath = g_build_filename("file:///tmp", filename, NULL);
  }
  else {
    if (!path || !strlen(path)) {
      path = g_get_current_dir();
    }
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
      fullpath = external ? g_build_filename(path, filename, NULL) : g_build_filename("file://", path, filename, NULL);
    }
    else {
      filename = strrchr(path, '/')+1;
      fullpath = external ? g_strdup(path) : g_build_filename("file://", path, NULL);
    }
  }

  if (external && dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
    command = download_get_command(uri, fullpath);
    if (!g_spawn_command_line_async(command, NULL)) {
      dwb_set_error_message(dwb.state.fview, "Cannot spawn download program");
    }
  }
  else {
    webkit_download_set_destination_uri(dwb.state.download, fullpath);
    DwbDownload *active = download_add_progress_label(dwb.state.fview, filename);
    active->action = dwb.state.dl_action;
    active->path = g_strdup(path);
    gtk_widget_show_all(dwb.gui.downloadbar);
    downloads = g_list_prepend(downloads, active);
    DwbDownloadStatus *s = dwb_malloc(sizeof(DwbDownloadStatus));
    s->blue = s->time = 0;
    g_signal_connect(active->event, "button-press-event", G_CALLBACK(download_button_press_cb), downloads);
    g_signal_connect(dwb.state.download, "notify::current-size", G_CALLBACK(download_progress_cb), s);
    g_signal_connect(dwb.state.download, "notify::status", G_CALLBACK(download_status_cb), s);
    webkit_download_start(dwb.state.download);
  }
  FREE(lastdir);
  if (dwb.state.dl_action != DL_ACTION_EXECUTE) {
    lastdir = g_strdup(path);
  }

  dwb_normal_mode(true);
  dwb.state.download = NULL;
  FREE(fullpath);
}/*}}}*/

/* download_entry_set_directory() {{{*/
static void
download_entry_set_directory() {
  dwb_set_normal_message(dwb.state.fview, false, "Downloadpath:");
  char *current_dir = lastdir ? g_strdup(lastdir) : g_get_current_dir();
  char *newdir = current_dir[strlen(current_dir) - 1] != '/' ? g_strdup_printf("%s/", current_dir) : g_strdup(current_dir);

  dwb_entry_set_text(newdir);

  FREE(current_dir);
  FREE(newdir);
}/*}}}*/

/* download_entry_set_spawn_command{{{*/
void
download_entry_set_spawn_command(const char *command) {
  if (!command && dwb.state.mimetype_request) {
    command = download_get_command_from_mimetype(dwb.state.mimetype_request);
  }
  dwb_set_normal_message(dwb.state.fview, false, "Spawn (%s):", dwb.state.mimetype_request ? dwb.state.mimetype_request : "???");
  dwb_entry_set_text(command ? command : "");
}/*}}}*/

/* download_get_path {{{*/
void 
download_get_path(GList *gl, WebKitDownload *d) {
  const char *uri = webkit_download_get_uri(d) + 7;

  if (g_file_test(uri,  G_FILE_TEST_IS_EXECUTABLE)) {
    g_spawn_command_line_async(uri, NULL);
    return;
  }

  char *command = NULL;
  dwb_focus_entry();
  dwb.state.mode = DOWNLOAD_GET_PATH;
  dwb.state.download = d;
  if ( (dwb.state.mimetype_request && (command = download_get_command_from_mimetype(dwb.state.mimetype_request))) ||  g_file_test(uri, G_FILE_TEST_EXISTS)) {
    dwb.state.dl_action = DL_ACTION_EXECUTE;
    download_entry_set_spawn_command(command);
  }
  else {
    download_entry_set_directory();
  }
}/*}}}*/

/* download_set_execute {{{*/
/* TODO complete path should not be in download.c */
void 
download_set_execute(Arg *arg) {
  if (dwb.state.mode == DOWNLOAD_GET_PATH) {
    if (dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
      dwb.state.dl_action = DL_ACTION_EXECUTE;
      download_entry_set_spawn_command(NULL);
    }
    else {
      dwb.state.dl_action = DL_ACTION_DOWNLOAD;
      download_entry_set_directory();
    }
  }
}/*}}}*/
