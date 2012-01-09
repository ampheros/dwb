/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
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

#include <string.h>
#include "dwb.h"
#include "html.h"
#include "util.h"

#define HTML_REMOVE_BUTTON "<div style='float:right;cursor:pointer;' navigation='%s %s' onclick='location.reload();'>&times</div>"
typedef struct _HtmlTable HtmlTable;

struct _HtmlTable {
  const char *uri;
  const char *title;
  const char *file;
  int type;
  DwbStatus (*func)(GList *, HtmlTable *);
};


DwbStatus html_bookmarks(GList *, HtmlTable *);
DwbStatus html_history(GList *, HtmlTable *);
DwbStatus html_quickmarks(GList *, HtmlTable *);
DwbStatus html_downloads(GList *, HtmlTable *);
DwbStatus html_settings(GList *, HtmlTable *);
DwbStatus html_startpage(GList *, HtmlTable *);
DwbStatus html_keys(GList *, HtmlTable *);


static HtmlTable table[] = {
  { "dwb://bookmarks",  "Bookmarks",    INFO_FILE,      0, html_bookmarks,  },
  { "dwb://quickmarks", "Quickmarks",   INFO_FILE,      0, html_quickmarks, },
  { "dwb://history",    "History",      INFO_FILE,      0, html_history,    },
  { "dwb://downloads",  "Downloads",    INFO_FILE,      0, html_downloads, },
  { "dwb://keys",       "Keys",         INFO_FILE,      0, html_keys },
  { "dwb://settings",   "Settings",     INFO_FILE,      0, html_settings },
  { "dwb://startpage",   NULL,           NULL,           0, html_startpage },
};

static char current_uri[BUFFER_LENGTH];
DwbStatus
html_load_page(WebKitWebView *wv, HtmlTable *t, char *panel) {
  char *filecontent;
  GString *content = g_string_new(NULL);
  char *path = util_get_data_file(t->file);
  char *headpath = util_get_data_file(HEAD_FILE);
  DwbStatus ret = STATUS_ERROR;

  if (path && headpath) {
    /* load head */
    g_file_get_contents(headpath, &filecontent, NULL, NULL);
    g_string_append_printf(content, filecontent, t->title);
    g_free(filecontent);
    /* load content */
    g_file_get_contents(path, &filecontent, NULL, NULL);
    if (panel) 
      g_string_append_printf(content, filecontent, panel);
    webkit_web_frame_load_alternate_string(webkit_web_view_get_main_frame(wv), content->str, current_uri, current_uri);
    g_string_free(content, true);
    g_free(filecontent);
    ret = STATUS_OK;
  }
  FREE(headpath);
  FREE(path);
  return ret;
}

gboolean
html_remove_item_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, GList *gl) {
  WebKitDOMEventTarget *target = webkit_dom_event_get_target(ev);
  if (webkit_dom_element_has_attribute((void*)target, "navigation")) {
    char *navigation = webkit_dom_element_get_attribute(WEBKIT_DOM_ELEMENT(target), "navigation");
    const char *uri = webkit_web_view_get_uri(WEBVIEW(gl));
    if (!g_strcmp0(uri, "dwb://history")) {
      dwb_remove_history(navigation);
    }
    else if (!g_strcmp0(uri, "dwb://bookmarks")) {
      dwb_remove_bookmark(navigation);
    }
    else if (!g_strcmp0(uri, "dwb://quickmarks")) {
      dwb_remove_quickmark(navigation);
    }
    else if (!g_strcmp0(uri, "dwb://downloads")) {
      dwb_remove_download(navigation);
    }
  }
  return false;
}
void
html_load_status_cb(WebKitWebView *web, GParamSpec *p, GList *gl) {
  WebKitLoadStatus s = webkit_web_view_get_load_status(web);
  if (s == WEBKIT_LOAD_FINISHED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
    WebKitDOMHTMLElement *body = webkit_dom_document_get_body(doc);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(body), "click", G_CALLBACK(html_remove_item_cb), true, gl);
    g_signal_handlers_disconnect_by_func(web, html_load_status_cb, gl);
  }
  else if (s == WEBKIT_LOAD_FAILED) {
    g_signal_handlers_disconnect_by_func(web, html_load_status_cb, gl);
  }

}
DwbStatus
html_navigation(GList *gl, GList *data, HtmlTable *table) {
  int i=0;
  WebKitWebView *wv = WEBVIEW(gl);
  DwbStatus ret;

  GString *panels = g_string_new(NULL);
  for (GList *l = data; l; l=l->next, i++, i%=2) {
    Navigation *n = l->data;
    g_string_append_printf(panels, "<tr class='dwb_table_row'>\
        <td class=dwb_table_cell_left>\
          <a href='%s'>%s</a>\
        </td>\
        <td class='dwb_table_cell_middle'></td>\
        <td class='dwb_table_cell_right' style='cursor:pointer;' navigation='%s %s' onclick='location.reload()'>&times</td>\
        <tr>",  n->first, n->second, n->first, n->second);
  }
  ret = html_load_page(wv, table, panels->str);

  g_signal_connect(wv, "notify::load-status", G_CALLBACK(html_load_status_cb), gl); 
  g_string_free(panels, true);
  return ret;
}
DwbStatus
html_bookmarks(GList *gl, HtmlTable *table) {
  dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)util_navigation_compare_second);
  return html_navigation(gl, dwb.fc.bookmarks, table);
}
DwbStatus
html_history(GList *gl, HtmlTable *table) {
  return html_navigation(gl, dwb.fc.history, table);
}
gboolean
html_settings_changed_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, WebKitWebView *wv) {
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(el));
  char *value = NULL;
  char *type;
  if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(el)) {
    type = webkit_dom_element_get_attribute(el, "type");
    if (!g_strcmp0(type, "checkbox")) {
      /* We need to dup "true" and "false", otherwise g_strstrip in
       * util_char_to_arg will cause a segfault */
      if (webkit_dom_html_input_element_get_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(el)) ) 
        value = g_strdup("true");
      else 
        value = g_strdup("false");
    }
    else  
      value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(el));
  }
  else if (WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(el)) {
    value = webkit_dom_html_select_element_get_value(WEBKIT_DOM_HTML_SELECT_ELEMENT(el));
  }
  dwb_set_setting(id, value);
  dwb_change_mode(NORMAL_MODE, false);
  return true;
}
gboolean
html_settings_really_changed_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, WebKitWebView *wv) {
  webkit_dom_element_blur(el);
  return true;
}
gboolean
html_keydown_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, WebKitWebView *wv) {
  glong val = webkit_dom_ui_event_get_key_code(WEBKIT_DOM_UI_EVENT(ev));
  if (val == 13) {
    WebKitDOMEventTarget *target = webkit_dom_event_get_target(ev);
    if (target != NULL) {
      char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(target));
      if (g_strcmp0(id, "dwb_custom_keys_area")) {
        webkit_dom_element_blur(WEBKIT_DOM_ELEMENT(target));
        return true;
      }
    }
  }
  return false;
}
gboolean
html_focus_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, WebKitWebView *wv) {
  char *type = webkit_dom_element_get_attribute(el, "type");
  gboolean ret = false;
  if (!g_strcmp0(type, "text")) {
    dwb_change_mode(INSERT_MODE);
    ret = true;
  }
  return ret;
}
void
html_settings_fill(char *key, WebSettings *s, WebKitWebView *wv) {
  char *value = util_arg_to_char(&s->arg, s->type);
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
  WebKitDOMElement *e = webkit_dom_document_get_element_by_id(doc, key);
  if (s->type == BOOLEAN) {
    webkit_dom_html_input_element_set_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), s->arg.b);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(html_settings_changed_cb), false, wv);
  }
  else {
    if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(e)) {
      webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), value ? value : "");
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(html_settings_really_changed_cb), false, wv);
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "blur", G_CALLBACK(html_settings_changed_cb), false, wv);
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "focus", G_CALLBACK(html_focus_cb), false, wv);
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "keydown", G_CALLBACK(html_keydown_cb), true, wv);
    }
    else if (WEBKIT_DOM_IS_HTML_SELECT_ELEMENT(e)) {
      char *lower = g_utf8_strdown(value, -1);
      webkit_dom_html_select_element_set_value(WEBKIT_DOM_HTML_SELECT_ELEMENT(e), lower);
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(html_settings_changed_cb), false, wv);
      g_free(lower);
    }
  }
  FREE(value);
}
void 
html_settings_load_cb(WebKitWebView *wv, GParamSpec *p, HtmlTable *table) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_FINISHED) {
    g_hash_table_foreach(dwb.settings, (GHFunc)html_settings_fill, wv);
    g_signal_handlers_disconnect_by_func(wv, html_settings_load_cb, table);
  }
}
DwbStatus
html_settings(GList *gl, HtmlTable *table) {
  char *content = NULL;
  DwbStatus ret = STATUS_ERROR;
  WebKitWebView *wv = WEBVIEW(gl);

  char *path = util_get_data_file(SETTINGS_FILE);
  if  (path != NULL) {
    g_file_get_contents(path, &content, NULL, NULL);
    ret = html_load_page(wv, table, content);
    g_free(path);
    g_signal_connect(wv, "notify::load-status", G_CALLBACK(html_settings_load_cb), table);
  }
  FREE(content);
  return ret;
}

static gboolean
html_custom_keys_changed_cb(WebKitDOMElement *target, WebKitDOMEvent *e, gpointer data) {
  WebKitDOMDocument *doc = webkit_dom_node_get_owner_document(WEBKIT_DOM_NODE(target));
  WebKitDOMElement *text_area = webkit_dom_document_get_element_by_id(doc, "dwb_custom_keys_area");
  char *value = webkit_dom_html_text_area_element_get_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(text_area));
  util_set_file_content(dwb.files.custom_keys, value);
  g_free(value);
  dwb_init_custom_keys(true);
  dwb_set_normal_message(dwb.state.fview, true, "Custom keys saved");
  return true;
}
static gboolean
html_key_changed_cb(WebKitDOMElement *target, WebKitDOMEvent *e, gpointer data) {
  char *value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(target));
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(target));
  dwb_set_key(id, value);
  return true;
}
static gboolean
html_key_really_changed_cb(WebKitDOMElement *target, WebKitDOMEvent *e, gpointer data) {
  webkit_dom_element_blur(target);
  return true;
}
void 
html_keys_load_cb(WebKitWebView *wv, GParamSpec *p, HtmlTable *table) {
  KeyMap *km;
  Navigation n;
  WebKitDOMElement *input;
  char *mod, *value;

  if (webkit_web_view_get_load_status(wv) == WEBKIT_LOAD_FINISHED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
    for (GList *l = dwb.keymap; l; l=l->next) {
      km = l->data; 
      n = km->map->n;
      input = webkit_dom_document_get_element_by_id(doc, n.first);
      if (input != NULL) {
        mod = dwb_modmask_to_string(km->mod);
        value = g_strdup_printf("%s%s%s", mod, strlen(mod) > 0 ? " " : "", km->key ? km->key : "");
        webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(input), value);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(input), "change", G_CALLBACK(html_key_really_changed_cb), false, wv);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(input), "blur", G_CALLBACK(html_key_changed_cb), false, wv);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(input), "focus", G_CALLBACK(html_focus_cb), false, wv);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "keydown", G_CALLBACK(html_keydown_cb), true, wv);
        FREE(mod);
        g_free(value);
      }
    }
    WebKitDOMElement *textarea = webkit_dom_document_get_element_by_id(doc, "dwb_custom_keys_area");
    WebKitDOMElement *button = webkit_dom_document_get_element_by_id(doc, "dwb_custom_keys_submit");
    char *content = util_get_file_content(dwb.files.custom_keys);
    if (content != NULL && *content != '\0') {
      webkit_dom_html_text_area_element_set_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(textarea), content);
      webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(button), "click", G_CALLBACK(html_custom_keys_changed_cb), false, wv);
    }
    FREE0(content);
    g_signal_handlers_disconnect_by_func(wv, html_keys_load_cb, table);
  }
}
DwbStatus
html_keys(GList *gl, HtmlTable *table) {
  DwbStatus ret = STATUS_ERROR;
  char *content = NULL;
  WebKitWebView *wv = WEBVIEW(gl);
  char *path = util_get_data_file(KEY_FILE);
  if (path != NULL) {
    g_signal_connect(wv, "notify::load-status", G_CALLBACK(html_keys_load_cb), table);
    g_file_get_contents(path, &content, NULL, NULL);
    ret = html_load_page(wv, table, content);
    g_free(path);
  }
  FREE(content);
  return ret;
}
DwbStatus
html_quickmarks(GList *gl, HtmlTable *table) {
  DwbStatus ret;
  WebKitWebView *wv = WEBVIEW(gl);
  GString *panels = g_string_new(NULL);

  for (GList *gl = dwb.fc.quickmarks; gl; gl=gl->next) {
    Quickmark *q = gl->data;
    g_string_append_printf(panels, "<tr class='dwb_table_row'>\
        <td class='dwb_table_cell_left'><div><div class='dwb_qm'>%s</div><a href='%s'>%s</a></div></td>\
        <td class='dwb_table_cell_middle'></td>\
        <td class='dwb_table_cell_right' style='cursor:pointer;' navigation='%s %s %s' onclick='location.reload()'>&times</td>\
        </tr>", q->key, q->nav->first, q->nav->second, q->key, q->nav->first, q->nav->second);
  }
  if ( (ret = html_load_page(wv, table, panels->str)) == STATUS_OK) 
    g_signal_connect(wv, "notify::load-status", G_CALLBACK(html_load_status_cb), gl); 
  g_string_free(panels, true);
  return ret;
}
DwbStatus
html_downloads(GList *gl, HtmlTable *table) {
  DwbStatus ret;
  WebKitWebView *wv = WEBVIEW(gl);
  GString *panels = g_string_new(NULL);
  for (GList *gl = dwb.fc.downloads; gl; gl=gl->next) {
    Navigation *n = gl->data;
    g_string_append_printf(panels, "<tr class='dwb_table_row'>\
        <td class='dwb_table_cell_left'>%s</td>\
        <td class='dwb_table_cell_middle'><a href='%s'>restart</a></td>\
        <td class='dwb_table_cell_right' style='cursor:pointer;' navigation='%s %s' onclick='location.reload()'>&times</td>\
        </tr>", n->second, n->first, n->first, n->second);
  }
  if ( (ret = html_load_page(wv, table, panels->str)) == STATUS_OK) 
    g_signal_connect(wv, "notify::load-status", G_CALLBACK(html_load_status_cb), gl); 
  g_string_free(panels, true);
  return ret;
}
DwbStatus
html_startpage(GList *gl, HtmlTable *table) {
  return dwb_open_startpage(gl);
}

gboolean 
html_load(GList *gl, const char *uri) {
  gboolean ret = false;
  for (int i=0; i<LENGTH(table); i++) {
    if (!strncmp(table[i].uri, uri, strlen(table[i].uri))) {
      g_strlcpy(current_uri, uri, BUFFER_LENGTH - 1);
      if (table[i].func(gl, &table[i]) == STATUS_OK)  {
        ret = true;
        break;
      }
    }
  }
  return ret;
}
