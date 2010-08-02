#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "commands.h"
#include "completion.h"
#include "util.h"

/* dwb_simple_command(keyMap *km) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  gboolean (*func)(void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;

  if (dwb.state.mode & AutoComplete) {
    dwb_clean_autocompletion();
  }

  if (func(arg)) {
    if (!km->map->hide) {
      gchar *message = g_strconcat(km->map->n.second, ":", NULL);
      dwb_set_normal_message(dwb.state.fview, message, false);
      g_free(message);
    }
    else if (km->map->hide == AlwaysSM) {
      CLEAR_COMMAND_TEXT(dwb.state.fview);
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
  }
  dwb.state.nummod = 0;
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
}/*}}}*/

/* COMMANDS {{{*/

/* dwb_set_setting {{{*/
gboolean 
dwb_set_setting(Arg *arg) {
  dwb.state.mode = SettingsMode;
  dwb.state.setting_apply = arg->n;
  dwb_focus_entry();
  
  //dwb_settings_completion(0);

  return true;
}/*}}}*/

/* dwb_set_key {{{*/
gboolean 
dwb_set_key(Arg *arg) {
  dwb.state.mode = KeyMode;
  dwb_focus_entry();

  return true;
}/*}}}*/

/* dwb_toggle_custom_encoding {{{*/
gboolean 
dwb_toggle_custom_encoding(Arg *arg) {
  WebKitWebView *web = CURRENT_WEBVIEW();

  const gchar *encoding= webkit_web_view_get_custom_encoding(web);
  const gchar *custom_encoding = GET_CHAR("custom-encoding");

  if (encoding && !strcmp(custom_encoding, encoding) ) {
    webkit_web_view_set_custom_encoding(web, NULL);
    const gchar *default_encoding = webkit_web_view_get_encoding(web);
    gchar *message = g_strdup_printf("Using default encoding: %s", default_encoding);
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
  }
  else {
    webkit_web_view_set_custom_encoding(web, custom_encoding);
    gchar *message = g_strdup_printf("Using encoding: %s", custom_encoding);
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
  }
  return true;
}/*}}}*/

/* dwb_focus_input {{{*/
gboolean
dwb_focus_input(Arg *a) {
  gchar *value;
  value = dwb_execute_script("focus_input()");
  if (!strcmp(value, "_dwb_no_input_")) {
    return false;
  }
  return true;
}/*}}}*/

/* dwb_add_search_field(Arg *) {{{*/
gboolean
dwb_add_search_field(Arg *a) {
  gchar *value;
  gboolean ret = true;
  value = dwb_execute_script("add_searchengine()");
  if (value) {
    if (!strcmp(value, "_dwb_no_hints_")) {
      return false;
    }
  }
  dwb.state.mode = SearchFieldMode;
  dwb_focus_entry();
  g_free(value);
  return ret;

}/*}}}*/

/* dwb_toggle_property {{{*/
gboolean 
dwb_toggle_property(Arg *a) {
  gchar *prop = a->p;
  gboolean value;
  WebKitWebSettings *settings = webkit_web_view_get_settings(CURRENT_WEBVIEW());
  g_object_get(settings, prop, &value, NULL);
  g_object_set(settings, prop, !value, NULL);
  gchar *message = g_strdup_printf("Setting %s set: %s", prop, !value ? "true" : "false");
  dwb_set_normal_message(dwb.state.fview, message, true);
  g_free(message);
  return true;
}/*}}}*/

/* dwb_toggle_proxy {{{*/
gboolean
dwb_toggle_proxy(Arg *a) {
  SoupURI *uri = NULL;

  WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
  s->arg.b = !s->arg.b;

  if (s->arg.b) { 
    uri = dwb.misc.proxyuri;
  }
  g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", uri, NULL);
  return true;
}/*}}}*/

/* dwb_focus_entry() {{{*/
void 
dwb_focus_entry() {
  gtk_widget_grab_focus(dwb.gui.entry);
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  //gtk_widget_show(dwb.gui.entry);
}/*}}}*/

/*dwb_find {{{*/
gboolean  
dwb_find(Arg *arg) { 
  dwb.state.mode = FindMode;
  dwb.state.forward_search = arg->b;
  //g_free(CURRENT_VIEW()->status->search_string);
  dwb_focus_entry();
  return true;
}/*}}}*/

/*dwb_resize_master {{{*/
gboolean  
dwb_resize_master(Arg *arg) { 
  gboolean ret = true;
  gint inc = dwb.state.nummod == 0 ? arg->n : dwb.state.nummod * arg->n;
  gint size = dwb.state.size + inc;
  if (size > 90 || size < 10) {
    size = size > 90 ? 90 : 10;
    ret = false;
  }
  dwb_resize(size);
  return ret;
}/*}}}*/

/* dwb_show_hints {{{*/
gboolean
dwb_show_hints(Arg *arg) {
  dwb.state.nv = arg->n;
  if (dwb.state.mode != HintMode) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
    dwb_execute_script("show_hints()");
    dwb.state.mode = HintMode;
    dwb_focus_entry();
  }
  return true;

}/*}}}*/

/* dwb_show_keys(Arg *arg){{{*/
gboolean 
dwb_show_keys(Arg *arg) {
  View *v = dwb.state.fview->data;
  GString *buffer = g_string_new(NULL);
  g_string_append_printf(buffer, SETTINGS_VIEW, dwb.color.settings_bg_color, dwb.color.settings_fg_color, dwb.misc.settings_border);
  g_string_append_printf(buffer, HTML_H2, "Keyboard Configuration", dwb.misc.profile);

  g_string_append(buffer, HTML_BODY_START);
  g_string_append(buffer, HTML_FORM_START);
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    Navigation n = m->map->n;
    g_string_append(buffer, HTML_DIV_START);
    g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, n.first);
    g_string_append_printf(buffer, HTML_DIV_KEYS_VALUE, n.first, dwb_modmask_to_string(m->mod), m->key ? : "");
    g_string_append(buffer, HTML_DIV_END);
  }
  g_string_append(buffer, HTML_FORM_END);
  g_string_append(buffer, HTML_BODY_END);
  dwb_web_view_add_history_item(dwb.state.fview);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(v->web), buffer->str, "text/html", NULL, KEY_SETTINGS);
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_show_settings(Arg *a) {{{*/
gboolean
dwb_show_settings(Arg *arg) {
  View *v = dwb.state.fview->data;
  GString *buffer = g_string_new(NULL);
  GHashTable *t;
  const gchar *setting_string;

  dwb.state.setting_apply = arg->n;
  if ( dwb.state.setting_apply == Global ) {
    t = dwb.settings;
    setting_string = "Global Settings";
  }
  else {
    t = v->setting;
    setting_string = "Settings";
  }

  GList *l = g_hash_table_get_values(t);
  l = g_list_sort(l, (GCompareFunc)dwb_web_settings_sort_second);

  g_string_append_printf(buffer, SETTINGS_VIEW, dwb.color.settings_bg_color, dwb.color.settings_fg_color, dwb.misc.settings_border);
  g_string_append_printf(buffer, HTML_H2, setting_string, dwb.misc.profile);

  g_string_append(buffer, HTML_BODY_START);
  g_string_append(buffer, HTML_FORM_START);
  for (; l; l=l->next) {
    WebSettings *m = l->data;
    if (!m->global || (m->global && dwb.state.setting_apply == Global)) {
      g_string_append(buffer, HTML_DIV_START);
      g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, m->n.second);
      if (m->type == Boolean) {
        const gchar *value = m->arg.b ? "checked" : "";
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_CHECKBOX, m->n.first, value);
      }
      else {
        gchar *value = dwb_arg_to_char(&m->arg, m->type);
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_VALUE, m->n.first, value ? value : "");
      }
      g_string_append(buffer, HTML_DIV_END);
    }
  }
  g_list_free(l);
  g_string_append(buffer, HTML_FORM_END);
  g_string_append(buffer, HTML_BODY_END);
  dwb_web_view_add_history_item(dwb.state.fview);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(v->web), buffer->str, "text/html", NULL, SETTINGS);
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_allow_cookie {{{*/
gboolean
dwb_allow_cookie(Arg *arg) {
  if (dwb.state.last_cookie) {
    dwb.fc.cookies_allow = g_list_append(dwb.fc.cookies_allow, dwb.state.last_cookie->domain);
    soup_cookie_jar_add_cookie(dwb.state.cookiejar, dwb.state.last_cookie);
    gchar *message = g_strdup_printf("Saved cookie and allowed domain: %s", dwb.state.last_cookie->domain);
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_bookmark {{{*/
gboolean 
dwb_bookmark(Arg *arg) {
  gboolean noerror;
  if ( (noerror = dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks)) ) {
    gchar *message = g_strdup_printf("Saved bookmark: %s", webkit_web_view_get_uri(CURRENT_WEBVIEW()));
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
  }

    
  return dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks);
}/*}}}*/

/* dwb_quickmark(Arg *arg) {{{*/
gboolean
dwb_quickmark(Arg *arg) {
  dwb.state.nv = arg->i;
  dwb.state.mode = arg->n;
  return true;
}/*}}}*/

/* dwb_reload(Arg *){{{*/
gboolean
dwb_reload(Arg *arg) {
  WebKitWebView *web = WEBVIEW_FROM_ARG(arg);
  webkit_web_view_reload(web);
  return true;
}/*}}}*/

/* dwb_view_source(Arg) {{{*/
gboolean
dwb_view_source(Arg *arg) {
  WebKitWebView *web = WEBVIEW_FROM_ARG(arg);
  webkit_web_view_set_view_source_mode(web, !webkit_web_view_get_view_source_mode(web));
  dwb_reload(arg);
  return true;
}/*}}}*/

/* dwb_zoom_in(void *arg) {{{*/
gboolean
dwb_zoom_in(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (gint i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_in(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_zoom_out(void *arg) {{{*/
gboolean
dwb_zoom_out(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_out(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_scroll {{{*/
gboolean 
dwb_scroll(Arg *arg) {
  gboolean ret = true;
  gdouble scroll;

  View *v = CURRENT_VIEW();

  GtkAdjustment *a = arg->n == Left || arg->n == Right 
    ? gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(v->scroll)) 
    : gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  gint sign = arg->n == Up || arg->n == PageUp || arg->n == Left ? -1 : 1;

  gdouble value = gtk_adjustment_get_value(a);

  gdouble inc = arg->n == PageUp || arg->n == PageDown 
    ? gtk_adjustment_get_page_increment(a) 
    : gtk_adjustment_get_step_increment(a);

  gdouble lower  = gtk_adjustment_get_lower(a);
  gdouble upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  switch (arg->n) {
    case  Top:      scroll = lower; break;
    case  Bottom:   scroll = upper; break;
    default:        scroll = value + sign * inc * NN(dwb.state.nummod); break;
  }

  scroll = scroll < lower ? lower : scroll > upper ? upper : scroll;
  if (scroll == value) 
    ret = false;
  else {
    gtk_adjustment_set_value(a, scroll);
    dwb_update_status_text(dwb.state.fview);
  }
  return ret;
}/*}}}*/

/* dwb_set_zoom_level(Arg *arg) {{{*/
void 
dwb_set_zoom_level(Arg *arg) {
  GList *gl = arg->p ? arg->p : dwb.state.fview;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(((View*)gl->data)->web), arg->d);

}/*}}}*/

/* dwb_set_orientation(Arg *arg) {{{*/
gboolean 
dwb_set_orientation(Arg *arg) {
  Layout l;
  if (arg->n) {
    l = arg->n;
  }
  else {
    dwb.state.layout ^= BottomStack;
    l = dwb.state.layout;
  }
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.paned), l & BottomStack );
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.right), (l & BottomStack) ^ 1);
  dwb_resize(dwb.state.size);
  return true;
}/*}}}*/

/* History {{{*/
gboolean 
dwb_history_back(Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = CURRENT_WEBVIEW();
  if (webkit_web_view_can_go_back(w)) {
    webkit_web_view_go_back(w);
    ret = true;
  }
  return ret;
}
gboolean 
dwb_history_forward(Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = CURRENT_WEBVIEW();
  if (webkit_web_view_can_go_forward(w)) {
    webkit_web_view_go_forward(w);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_open(Arg *arg) {{{*/
gboolean  
dwb_open(Arg *arg) {
  dwb.state.nv = arg->n;
  //gtk_widget_grab_focus(dwb.gui.entry);

  if (arg && arg->p) {
    dwb_load_uri(arg);
    dwb_normal_mode(true);
    
  }
  else {
    dwb_focus_entry();
    dwb.state.mode = OpenMode;
  }
  return true;
} /*}}}*/

/* dwb_get_search_engine_uri(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine_uri(const gchar *uri, const gchar *text) {
  gchar **token = g_strsplit(uri, HINT_SEARCH_SUBMIT, 2);
  gchar *ret = g_strconcat(token[0], text, token[1], NULL);
  g_strfreev(token);
  return ret;
}/* }}} */

/* dwb_get_search_engine(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine(const gchar *uri) {
  gchar *ret = NULL;
  if ( (!strstr(uri, ".") || strstr(uri, " ")) && !strstr(uri, "localhost:")) {
    gchar **token = g_strsplit(uri, " ", 2);
    for (GList *l = dwb.fc.searchengines; l; l=l->next) {
      Navigation *n = l->data;
      if (!strcmp(token[0], n->first)) {
        ret = dwb_get_search_engine_uri(n->second, token[1]);
        break;
      }
    }
    if (!ret) {
      ret = dwb_get_search_engine_uri(dwb.misc.default_search, uri);
    }
  }
  return ret;
}/*}}}*/

/* dwb_toggle_maximized {{{*/
void 
dwb_maximized_hide_zoom(View *v, View *no) {
  if (dwb.misc.factor != 1.0) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  }
  if (v != dwb.state.fview->data) {
    gtk_widget_hide(v->vbox);
  }
}
void 
dwb_maximized_show_zoom(View *v) {
  if (dwb.misc.factor != 1.0 && v != dwb.state.views->data) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), dwb.misc.factor);
  }
  gtk_widget_show(v->vbox);
}

void 
dwb_toggle_maximized(Arg *arg) {
  dwb.state.layout ^= Maximized;
  if (dwb.state.layout & Maximized) {
    g_list_foreach(dwb.state.views,  (GFunc)dwb_maximized_hide_zoom, NULL);
    if  (dwb.state.views == dwb.state.fview) {
      gtk_widget_hide(dwb.gui.right);
    }
    else if (dwb.state.views->next) {
      gtk_widget_hide(dwb.gui.left);
    }
  }
  else {
    if (dwb.state.views->next) {
      gtk_widget_show(dwb.gui.right);
    }
    gtk_widget_show(dwb.gui.left);
    g_list_foreach(dwb.state.views,  (GFunc)dwb_maximized_show_zoom, NULL);
  }
  dwb_resize(dwb.state.size);
}/*}}}*/

/* dwb_remove_view(Arg *arg) {{{*/
void 
dwb_remove_view(Arg *arg) {
  GList *gl;
  if (!dwb.state.views->next) {
    dwb_exit();
    return;
  }
  if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
  }
  else {
    gl = dwb.state.fview;
    if ( !(dwb.state.fview = dwb.state.fview->next) ) {
      dwb.state.fview = dwb.state.views;
      gtk_widget_show_all(dwb.gui.topbox);
    }
  }
  View *v = gl->data;
  if (gl == dwb.state.views) {
    if (dwb.state.views->next) {
      View *newfirst = dwb.state.views->next->data;
      gtk_widget_reparent(newfirst->vbox, dwb.gui.left);
    }
  }
  gtk_widget_destroy(v->vbox);
  dwb.gui.entry = NULL;
  dwb_grab_focus(dwb.state.fview);
  gtk_container_remove(GTK_CONTAINER(dwb.gui.topbox), v->tabevent);

  // clean up
  dwb_source_remove(gl);
  g_free(v->status);
  g_free(v);

  dwb.state.views = g_list_delete_link(dwb.state.views, gl);

  if (dwb.state.layout & Maximized) {
    gtk_widget_show(CURRENT_VIEW()->vbox);
    if (dwb.state.fview == dwb.state.views) {
      gtk_widget_hide(dwb.gui.right);
      gtk_widget_show(dwb.gui.left);
    }
    else {
      gtk_widget_show(dwb.gui.right);
      gtk_widget_hide(dwb.gui.left);
    }
  }
  dwb_update_layout();
}/*}}}*/

/* dwb_push_master {{{*/
gboolean 
dwb_push_master(Arg *arg) {
  GList *gl, *l;
  View *old = NULL, *new;
  if (!dwb.state.views->next) {
    return false;
  }
  if (arg && arg->p) {
    gl = arg->p;
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
    if (!gl) {
      return false;
    }
    CLEAR_COMMAND_TEXT(dwb.state.views);
    dwb_set_normal_style(dwb.state.fview);
  }
  else {
    gl = dwb.state.fview;
  }
  if (gl == dwb.state.views) {
    old = gl->data;
    l = dwb.state.views->next;
    new = l->data;
    gtk_widget_reparent(old->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), old->vbox, 0);
    gtk_widget_reparent(new->vbox, dwb.gui.left);
    dwb.state.views = g_list_remove_link(dwb.state.views, l);
    dwb.state.views = g_list_concat(l, dwb.state.views);
    dwb_focus(l);
  }
  else {
    old = dwb.state.views->data;
    new = gl->data;
    gtk_widget_reparent(new->vbox, dwb.gui.left);
    gtk_widget_reparent(old->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), old->vbox, 0);
    dwb.state.views = g_list_remove_link(dwb.state.views, gl);
    dwb.state.views = g_list_concat(gl, dwb.state.views);
    dwb_grab_focus(dwb.state.views);
  }
  if (dwb.state.layout & Maximized) {
    gtk_widget_show(dwb.gui.left);
    gtk_widget_hide(dwb.gui.right);
    gtk_widget_show(new->vbox);
    gtk_widget_hide(old->vbox);
  }
  gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), new->tabevent, -1);
  dwb_update_layout();
  return true;
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  GList *tmp = NULL;

  if (dwb.state.fview) {
    tmp = dwb.state.fview;
  }
  if (tmp) {
    dwb_set_normal_style(tmp);
    dwb_source_remove(tmp);
    CLEAR_COMMAND_TEXT(tmp);
  }
  dwb_grab_focus(gl);
} /*}}}*/

/* dwb_focus_next(Arg *arg) {{{*/
gboolean 
dwb_focus_next(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl->next) {
    if (dwb.state.layout & Maximized) {
      if (gl == dwb.state.views) {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      gtk_widget_show(((View *)gl->next->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->next);
  }
  else {
    if (dwb.state.layout & Maximized) {
      gtk_widget_hide(dwb.gui.right);
      gtk_widget_show(dwb.gui.left);
      gtk_widget_show(((View *)dwb.state.views->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(g_list_first(dwb.state.views));
  }
  return true;
}/*}}}*/

/* dwb_focus_prev(Arg *arg) {{{*/
gboolean 
dwb_focus_prev(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl == dwb.state.views) {
    GList *last = g_list_last(dwb.state.views);
    if (dwb.state.layout & Maximized) {
      gtk_widget_hide(dwb.gui.left);
      gtk_widget_show(dwb.gui.right);
      gtk_widget_show(((View *)last->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(last);
  }
  else {
    if (dwb.state.layout & Maximized) {
      if (gl == dwb.state.views->next) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
      gtk_widget_show(((View *)gl->prev->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->prev);
  }
  return true;
}/*}}}*/

gboolean
dwb_yank(Arg *arg) {
  WebKitWebView *w = CURRENT_WEBVIEW();
  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *old = gtk_clipboard_get(atom);
  gchar *text = NULL;
  gboolean ret = false;

  if (webkit_web_view_has_selection(w)) {
    webkit_web_view_copy_clipboard(w);
    GtkClipboard *new = gtk_clipboard_get(GDK_NONE);
    text = gtk_clipboard_wait_for_text(new);
  }
  else {
    text = g_strdup(webkit_web_view_get_uri(w));
  }
  if (text) {
    if (strlen(text)) {
      gtk_clipboard_set_text(old, text, -1);
      gchar *message = g_strdup_printf("Yanked: %s", text);
      dwb_set_normal_message(dwb.state.fview, message, true);
      g_free(message);
      ret = true;
    }
    g_free(text);
  }
  return ret;
}
gboolean
dwb_paste(Arg *arg) {
  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  gchar *text = NULL;

  if ( (text = gtk_clipboard_wait_for_text(clipboard)) ) {
    dwb.state.nv = arg->n;
    Arg a = { .p = text };
    dwb_load_uri(&a);
    g_free(text);
    return true;
  }
  return false;
}


/*}}}*/

