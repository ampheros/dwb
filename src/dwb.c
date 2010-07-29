#define _POSIX_SOURCE
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <ctype.h>
#include <errno.h>
#include <JavaScriptCore/JavaScript.h>
#include <libsoup/soup.h>
#include <gdk/gdkkeysyms.h>
#include <locale.h>

#define NAME "dwb2";

/* SETTINGS MAKROS {{{*/
#define KEY_SETTINGS "Dwb Key Settings"
#define SETTINGS "Dwb Settings"

// SETTTINGS_VIEW %s: bg-color  %s: fg-color %s: border
#define SETTINGS_VIEW "<head>\n<style type=\"text/css\">\n \
  body { background-color: %s; color: %s; font: fantasy; font-size:16; font-weight: bold; text-align:center; }\n\
  .line { border: %s; vertical-align: middle; }\n \
  .text { float: left; font-variant: normal; font-size: 14;}\n \
  .key { text-align: right;  font-size: 12; }\n \
  .active { background-color: #660000; }\n \
  h2 { font-variant: small-caps; }\n \
  .alignCenter { margin-left: 25%%; width: 50%%; }\n \
  </style>\n \
  <script type=\"text/javascript\">\n  \
  function setting_submit() { e = document.activeElement; value = e.value ? e.id + \" \" + e.value : e.id; console.log(value); e.blur(); return false; } \
  function checkbox_click(id) { e = document.activeElement; value = e.value ? e.id + \" \" + e.value : e.id; console.log(value); e.blur(); } \
  </script>\n<noscript>Enable scripts to add settings!</noscript>\n</head>\n"
#define HTML_H2  "<h2>%s -- Profile: %s</h2>"

#define HTML_BODY_START "<body>\n"
#define HTML_BODY_END "</body>\n"
#define HTML_FORM_START "<div class=\"alignCenter\">\n <form onsubmit=\"return setting_submit()\">\n"
#define HTML_FORM_END "<input type=\"submit\" value=\"save\"/></form>\n</div>\n"
#define HTML_DIV_START "<div class=\"line\">\n"
#define HTML_DIV_KEYS_TEXT "<div class=\"text\">%s</div>\n "
#define HTML_DIV_KEYS_VALUE "<div class=\"key\">\n <input id=\"%s\" value=\"%s %s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_VALUE "<div class=\"key\">\n <input id=\"%s\" value=\"%s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_CHECKBOX "<div class=\"key\"\n <input id=\"%s\" type=\"checkbox\" onchange=\"checkbox_click();\" %s>\n</div>\n"
#define HTML_DIV_END "</div>\n"
/*}}}*/
#define INSERT_MODE "Insert Mode"

#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

/* PRE {{{*/

/* MAKROS {{{*/ 
#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X)) 
#define NN(X)       ( ((X) == 0) ? 1 : (X) )
#define NUMMOD      (dwb.state.nummod == 0 ? 1 : dwb.state.nummod)

#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))

#define GET_TEXT()                  (gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)))
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT(X)       dwb_set_status_bar_text(((View *)X->data)->lstatus, NULL, NULL, NULL)

#define DIGIT(X)   (X->keyval >= GDK_0 && X->keyval <= GDK_9)
#define ALPHA(X)    ((X->keyval >= GDK_A && X->keyval <= GDK_Z) ||  (X->keyval >= GDK_a && X->keyval <= GDK_z) || X->keyval == GDK_space)
#define True (void*) true
#define False (void*) false

// Settings
#define GET_CHAR(prop)              ((gchar*)(((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.p))
#define GET_BOOL(prop)              (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.b)
#define GET_INT(prop)               (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.i)
#define GET_DOUBLE(prop)            (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.d)
/*}}}*/

/* TYPES {{{*/
typedef enum _Mode Mode;
typedef enum _Open Open;
typedef enum _Layout Layout;
typedef enum _Direction Direction;
typedef enum _DwbType DwbType;
typedef enum _SettingsScope SettingsScope;

typedef struct _Arg Arg;
typedef struct _Misc Misc;
typedef struct _Dwb Dwb;
typedef struct _Gui Gui;
typedef struct _State State;
typedef struct _View View;
typedef struct _Color Color;
typedef struct _Font Font;
typedef struct _FunctionMap FunctionMap;
typedef struct _KeyMap KeyMap;
typedef struct _Key Key;
typedef struct _KeyValue KeyValue;
typedef struct _ViewStatus ViewStatus;
typedef struct _Files Files;
typedef struct _FileContent FileContent;
typedef struct _Navigation Navigation;
typedef struct _Quickmark Quickmark;
typedef struct _WebSettings WebSettings;
typedef struct _Settings Settings;
typedef struct _Completion Completion;
typedef union _Type Type;
/*}}}*/

/* DECLARATIONS {{{*/
gchar * dwb_modmask_to_string(guint modmask);

void dwb_reload_scripts(GList *,  WebSettings *);
void dwb_reload_colors(GList *,  WebSettings *);
void dwb_reload_layout(GList *,  WebSettings *);
void dwb_save_searchengine(void);
void dwb_focus_entry(void);
gboolean dwb_test_cookie_allowed(const gchar *);
void dwb_save_cookies(void);
gboolean dwb_search(Arg *);
void dwb_apply_settings(WebSettings *);
gboolean dwb_update_hints(GdkEventKey *);
gchar * dwb_get_directory_content(const gchar *);
Navigation * dwb_navigation_new_from_line(const gchar *);
Navigation * dwb_navigation_new(const gchar *, const gchar *);
void dwb_navigation_free(Navigation *);
void dwb_quickmark_free(Quickmark *);
Quickmark * dwb_quickmark_new(const gchar *, const gchar *, const gchar *);
Quickmark * dwb_quickmark_new_from_line(const gchar *);
void dwb_web_view_add_history_item(GList *gl);
void dwb_web_settings_set_value(const gchar *id, Arg *a);

gboolean dwb_eval_key(GdkEventKey *);

void dwb_webkit_setting(GList *, WebSettings *);
void dwb_webview_property(GList *, WebSettings *);
void dwb_resize(gdouble);
void dwb_normal_mode(gboolean);

void dwb_prepend_navigation_with_argument(GList **, const gchar *, const gchar *);
gboolean dwb_prepend_navigation(GList *, GList **);
void dwb_set_error_message(GList *, const gchar *);
void dwb_set_normal_message(GList *, const gchar *, gboolean);
void dwb_set_status_bar_text(GtkWidget *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_set_status_text(GList *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_tab_label_set_text(GList *, const gchar *);
void dwb_update_status(GList *gl);
void dwb_update_status_text(GList *gl);

void dwb_save_quickmark(const gchar *);
void dwb_open_quickmark(const gchar *);
Key dwb_strv_to_key(gchar **string, gsize length);

GSList * dwb_keymap_delete(GSList *, KeyValue );
GSList * dwb_keymap_add(GSList *gl, KeyValue key);

void dwb_set_active_style(GList *gl);
void dwb_set_normal_style(GList *gl);
void dwb_view_modify_style(GList *, GdkColor *, GdkColor *, GdkColor *, GdkColor *, PangoFontDescription *, gint);
GList * dwb_create_web_view(GList *);
void dwb_add_view(Arg *);
void dwb_remove_view(Arg *);
void dwb_source_remove(GList *);

gboolean dwb_web_view_close_web_view_cb(WebKitWebView *, GList *);
gboolean dwb_web_view_console_message_cb(WebKitWebView *, gchar *, gint , gchar *, GList *);
GtkWidget * dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *);
gboolean dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *);
void dwb_web_view_hovering_over_link_cb(WebKitWebView *, gchar *, gchar *, GList *);
gboolean dwb_web_view_mime_type_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, gchar *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_navigation_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_new_window_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_script_alert_cb(WebKitWebView *, WebKitWebFrame *, gchar *, GList *);
void dwb_web_view_window_object_cleared_cb(WebKitWebView *, WebKitWebFrame *, JSGlobalContextRef *, JSObjectRef *, GList *);
void dwb_web_view_load_status_cb(WebKitWebView *, GParamSpec *, GList *);
void dwb_web_view_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, WebKitNetworkRequest *, WebKitNetworkResponse *, GList *);
void dwb_web_view_title_cb(WebKitWebView *, GParamSpec *, GList *);
void dwb_cookie_changed_cb(SoupCookieJar *, SoupCookie *, SoupCookie *);
gboolean dwb_web_view_focus_cb(WebKitWebView *, GtkDirectionType *, GList *);


void dwb_complete(gint);
void dwb_clean_completion(void);

gboolean dwb_web_view_button_press_cb(WebKitWebView *, GdkEventButton *, GList *);
gboolean dwb_entry_keypress_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_keyrelease_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_activate_cb(GtkEntry *);

void dwb_update_layout(void);
void dwb_update_tab_label(void);
void dwb_focus(GList *);
void dwb_load_uri(Arg *);
void dwb_grab_focus(GList *);
gchar * dwb_get_resolved_uri(const gchar *);
gchar * dwb_execute_script(const gchar *);

gboolean dwb_allow_cookie(Arg *);
gboolean dwb_create_hints(Arg *);
gboolean dwb_find(Arg *);
gboolean dwb_focus_input(Arg *);
gboolean dwb_resize_master(Arg *);
gboolean dwb_show_hints(Arg *);
gboolean dwb_show_keys(Arg *);
gboolean dwb_show_settings(Arg *);
gboolean dwb_quickmark(Arg *);
gboolean dwb_bookmark(Arg *);
gboolean dwb_open(Arg *);
gboolean dwb_focus_next(Arg *);
gboolean dwb_focus_prev(Arg *);
gboolean dwb_push_master(Arg *);
gboolean dwb_reload(Arg *);
gboolean dwb_set_orientation(Arg *);
void dwb_toggle_maximized(Arg *);
gboolean dwb_view_source(Arg *);
gboolean dwb_zoom_in(Arg *);
gboolean dwb_zoom_out(Arg *);
void dwb_set_zoom_level(Arg *);
gboolean dwb_scroll(Arg *);
gboolean dwb_history_back(Arg *);
gboolean dwb_history_forward(Arg *);
gboolean dwb_insert_mode(Arg *);
gboolean dwb_toggle_property(Arg *); 
gboolean dwb_toggle_proxy(Arg *); 
gboolean dwb_add_search_field(Arg *);
gboolean dwb_toggle_custom_encoding(Arg *);

void dwb_set_websetting(GList *, WebSettings *);

void dwb_init_key_map(void);
void dwb_init_settings(void);
void dwb_init_style(void);
void dwb_init_scripts(void);
void dwb_init_gui(void);

void dwb_clean_vars(void);
void dwb_exit(void);
/*}}}*/

/* ENUMS {{{*/
enum _Mode {
  NormalMode,
  InsertMode,
  OpenMode,
  QuickmarkSave,
  QuickmarkOpen, 
  HintMode,
  FindMode,
  CompletionMode,
  SearchFieldMode,
  SearchKeywordMode,
};


enum _Open {
  OpenNormal, 
  OpenNewView,
  OpenNewWindow,
};

enum _Layout {
  NormalLayout = 0,
  BottomStack = 1<<0, 
  Maximized = 1<<1, 
};

enum _Direction {
  Up, 
  Down, 
  Left, 
  Right, 
  PageUp,
  PageDown, 
  Top,
  Bottom,
};
enum _DwbType {
  Char, 
  Integer,
  Double,
  Boolean, 
  Pointer, 
};
enum _SettingsScope {
  Global,
  PerView,
};
/*}}}*/

/* STRUCTS {{{*/
struct _Arg {
  guint n;
  gint i;
  gdouble d;
  gpointer p;
  gboolean b;
  gchar *e;
};
struct _Key {
  const gchar *str;
  guint mod;
};
struct _KeyValue {
  const gchar *id;
  Key key;
};
struct _FunctionMap {
  const gchar *id;
  gboolean (*func)(void*);
  const gchar *desc;
  const gchar *error; 
  gboolean hide;
  Arg arg;
};
struct _KeyMap {
  const gchar *key;
  guint mod;
  FunctionMap *map;
};
struct _Navigation {
  gchar *first;
  gchar *second;
};
struct _Quickmark {
  gchar *key; 
  Navigation *nav;
};

struct _State {
  GList *views;
  GList *fview;
  WebKitWebSettings *web_settings;
  Mode mode;
  GString *buffer;
  gint nummod;
  Open nv;
  guint scriptlock;
  gint size;
  GHashTable *settings_hash;
  SettingsScope setting_apply;
  gboolean forward_search;
  SoupCookieJar *cookiejar;
  SoupCookie *last_cookie;
  GList *active_comp;
  GList *completions;
  Layout layout;

  gchar *search_engine;
  gchar *form_name;

};

union _Type {
  gboolean b;
  gdouble f;
  guint i; 
  gpointer p;
};
struct _WebSettings {
  gchar *id;
  gboolean builtin;
  gboolean global;
  DwbType type;
  Arg arg;
  void (*func)(void *, WebSettings *);
  gchar *desc;
};

struct _View {
  GtkWidget *vbox;
  GtkWidget *web;
  GtkWidget *tabevent;
  GtkWidget *tablabel;
  GtkWidget *statusbox;
  GtkWidget *rstatus;
  GtkWidget *lstatus;
  GtkWidget *scroll; 
  GtkWidget *entry;
  GtkWidget *compbox;
  GtkWidget *bottombox;
  View *next;
  ViewStatus *status;
  GHashTable *setting;
};
struct _Completion {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  const gchar *text;
};
struct _ViewStatus {
  guint message_id;
  gchar *hover_uri;
  gboolean add_history;
  gchar *search_string;
};
struct _Color {
  GdkColor active_fg;
  GdkColor active_bg;
  GdkColor normal_fg;
  GdkColor normal_bg;
  GdkColor tab_active_fg;
  GdkColor tab_active_bg;
  GdkColor tab_normal_fg;
  GdkColor tab_normal_bg;
  GdkColor insert_bg;
  GdkColor insert_fg;
  GdkColor error;
  GdkColor active_c_fg;
  GdkColor active_c_bg;
  GdkColor normal_c_fg;
  GdkColor normal_c_bg;
  gchar *settings_bg_color;
  gchar *settings_fg_color;
};
struct _Font {
  PangoFontDescription *fd_normal;
  PangoFontDescription *fd_bold;
  PangoFontDescription *fd_oblique;
  gint active_size;
  gint normal_size;
};
struct _Setting {
  gboolean inc_search;
  gboolean wrap_search;
};

struct _Gui {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *topbox;
  GtkWidget *paned;
  GtkWidget *right;
  GtkWidget *left;
  GtkWidget *entry;
  gint width;
  gint height;
};
struct _Misc {
  const gchar *name;
  const gchar *scripts;
  const gchar *profile;
  const gchar *default_search;
  SoupSession *soupsession;
  SoupURI *proxyuri;

  gdouble factor;
  gint max_c_items;
  gint message_delay;
  gint history_length;

  gchar *settings_border;
};
struct _Files {
  const gchar *bookmarks;
  const gchar *history;
  const gchar *mimetype;
  const gchar *quickmarks;
  const gchar *session;
  const gchar *searchengines;
  const gchar *stylesheet;
  const gchar *keys;
  const gchar *scriptdir;
  const gchar *settings;
  const gchar *cookies;
  const gchar *cookies_allow;
};
struct _FileContent {
  GList *bookmarks;
  GList *history;
  GList *mimetype;
  GList *quickmarks;
  GList *searchengines;
  GList *keys;
  GList *settings;
  GList *cookies_allow;
  GList *commands;
};

struct _Dwb {
  Gui gui;
  Color color;
  Font font;
  Misc misc;
  State state;
  GSList *keymap;
  GHashTable *settings;
  Files files;
  FileContent fc;
};

/*}}}*/

/* VARIABLES {{{*/
static GdkNativeWindow embed = 0;
static Dwb dwb;
static gint signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static gint MAX_COMPLETIONS = 11;
// TODO toggle proxy
/*}}}*/

#include "config.h"

/* FUNCTION_MAP{{{*/
#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"
FunctionMap FMAP [] = {
  { "add_view",              (void*)dwb_add_view,           "Add a new view",                 NULL,                       true,                                             },
  { "allow_cookie",          (void*)dwb_allow_cookie,       "Cookie allowed",                 "No cookie in current context",  true,                                             },
  { "bookmark",              (void*)dwb_bookmark,           "Bookmark current page",          NO_URL,                     true,                                             },
  { "find_forward",          (void*)dwb_find,               "Find Forward ",                   NO_URL,                     false,     { .b = true },                          },
  { "find_backward",         (void*)dwb_find,               "Find Backward ",                  NO_URL,                     false,     { .b = false },                         },
  { "find_next",             (void*)dwb_search,             "Find next",                      "No matches",                     false,     { .b = true },                         },
  { "find_previous",         (void*)dwb_search,             "Find next",                      "No matches",                     false,     { .b = false },                         },
  { "focus_input",           (void*)dwb_focus_input,        "Focus input",                    "No input found in current context",        true,                                             },
  { "focus_next",            (void*)dwb_focus_next,         "Focus next view",                "No other view",            true,                                             },
  { "focus_prev",            (void*)dwb_focus_prev,         "Focus prevous view",             "No other view",            true,                                             }, 
  { "hint_mode",             (void*)dwb_show_hints,         "Follow hints ",                   NO_HINTS,                   false,    { .n = OpenNormal },                    },
  { "hint_mode_new_view",    (void*)dwb_show_hints,         "Follow hints in a new view ",     NO_HINTS,                   false,    { .n = OpenNewView },                    },
  { "history_back",          (void*)dwb_history_back,       "Go Back",                        "Beginning of History",     true,                                             },
  { "history_forward",       (void*)dwb_history_forward,    "Go Forward",                     "End of History",           true,                                             },
  { "insert_mode",           (void*)dwb_insert_mode,        INSERT_MODE,                    NULL,                       false,                                            },
  { "increase_master",       (void*)dwb_resize_master,      "Increase master area",           "Cannot increase further",  true,    { .n = -5 }                           },
  { "decrease_master",       (void*)dwb_resize_master,      "Decrease master area",           "Cannot decrease further",  true,    { .n = 5 }                            },
  { "save_search_field",     (void*)dwb_add_search_field,   "Add a new searchengine",         "No input in current context",  true,                                },


  { "open",                  (void*)dwb_open,               "Open URL:",                       NULL,                       false,   { .n = OpenNormal,      .p = NULL }      },
  { "open_new_view",         (void*)dwb_open,               "Open URL in a new view:",         NULL,                       false,   { .n = OpenNewView,     .p = NULL }      },
  { "open_quickmark",        (void*)dwb_quickmark,          "Open quickmark",                 NO_URL,                     false,   { .n = QuickmarkOpen, .i=OpenNormal },   },
  { "open_quickmark_nv",     (void*)dwb_quickmark,          "Open quickmark in a new view",   NULL,                       false,    { .n = QuickmarkOpen, .i=OpenNewView },  },
  { "push_master",           (void*)dwb_push_master,        "Push to master area",            "No other view",            true,                                             },
  { "reload",                (void*)dwb_reload,             "Reload",                    NULL,                       true,                                             },
  { "remove_view",           (void*)dwb_remove_view,        "Close view",                     NULL,                       true,                                             },
  { "save_quickmark",        (void*)dwb_quickmark,          "Save a quickmark for this page", NO_URL,                     false,    { .n = QuickmarkSave },                  },
  { "scroll_bottom",         (void*)dwb_scroll,             "Scroll to  bottom of the page",  NULL,                       true,    { .n = Bottom },                         },
  { "scroll_down",           (void*)dwb_scroll,             "Scroll down",                    "Bottom of the page",       true,    { .n = Down, },                          },
  { "scroll_left",           (void*)dwb_scroll,             "Scroll left",                    "Left side of the  page",   true,    { .n = Left },                           },
  { "scroll_page_down",      (void*)dwb_scroll,             "Scroll one page down",           "Bottom of the page",       true,    { .n = PageDown, },                      },
  { "scroll_page_up",        (void*)dwb_scroll,             "Scroll one page up",             "Top of the page",          true,    { .n = PageUp, },                        },
  { "scroll_right",          (void*)dwb_scroll,             "Scroll left",                    "Right side of the page",   true,    { .n = Right },                          },
  { "scroll_top",            (void*)dwb_scroll,             "Scroll to the top of the page",   NULL,                       true,    { .n = Top },                            },
  { "scroll_up",             (void*)dwb_scroll,             "Scroll up",                      "Top of the page",          true,    { .n = Up, },                            },
  { "show_keys",             (void*)dwb_show_keys,          "Key configuration",              NULL,                       true,                                             },
  { "show_settings",         (void*)dwb_show_settings,      "Settings",                       NULL,                       true,    { .n = PerView }                         },
  { "show_global_settings",  (void*)dwb_show_settings,      "Show global settings",           NULL,                       true,    { .n = Global }                          },
  { "toggle_bottomstack",    (void*)dwb_set_orientation,    "Toggle bottomstack",             NULL,                       true,                                             }, 
  { "toggle_maximized",      (void*)dwb_toggle_maximized,   "Toggle maximized",               NULL,                       true,                                             },  
  { "view_source",           (void*)dwb_view_source,        "View source",                    NULL,                       true,                                             },  
  { "zoom_in",               (void*)dwb_zoom_in,            "Zoom in",                        "Cannot zoom in further",   true,                                             },
  { "zoom_normal",           (void*)dwb_set_zoom_level,     "Zoom 100%",                      NULL,                       true,    { .d = 1.0,   .p = NULL }                },
  { "zoom_out",              (void*)dwb_zoom_out,           "Zoom out",                       "Cannot zoom out further",  true,                                             },
  { "autoload_images",       (void*)dwb_toggle_property,           "Setting: autoload images",         NULL,                       true,    { .p = "auto-load-images" }              },
  { "autoresize_window",     (void*)dwb_toggle_property,         "Setting: autoresize window",         NULL,                       true,    { .p = "auto-resize-window" }              },
  { "toggle_shrink_images",  (void*)dwb_toggle_property,    "Setting: autoshrink images",         NULL,                       true,    { .p = "auto-shrink-images" }              },
  { "toggle_encoding",       (void*)dwb_toggle_custom_encoding,    "",                       NULL,                  true,                  },
  { "caret_browsing",        (void*)dwb_toggle_property,    "Setting: caret browsing",         NULL,                       true,    { .p = "enable-caret-browsing" }              },
  { "java_applets",          (void*)dwb_toggle_property,      "Setting: java applets",         NULL,                       true,    { .p = "enable-java-applets" }              },
  { "plugins",               (void*)dwb_toggle_property,           "Setting: plugins",         NULL,                       true,    { .p = "enable-plugins" }              },
  { "private_browsing",      (void*)dwb_toggle_property,  "Setting: private browsing",         NULL,                       true,    { .p = "enable-private-browsing" }              },
  { "scripts",               (void*)dwb_toggle_property,      "Setting: scripts",         NULL,                       true,    { .p = "enable-scripts" }              },
  { "spell_checking",        (void*)dwb_toggle_property,      "Setting: spell checking",         NULL,                       true,    { .p = "enable-spell-checking" }              },
  { "proxy",                 (void*)dwb_toggle_proxy,      "Setting: proxy",                   NULL,                       true,    { 0 }              },

  //{ "create_hints",          (void*)dwb_create_hints,       "Hints",                          NULL,                       true,                                             },

};/*}}}*/

/* DWB_SETTINGS {{{*/
static WebSettings DWB_SETTINGS[] = {
  { "auto-load-images",			                       true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Autoload images", }, 
  { "auto-resize-window",			                     true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Autoresize images", }, 
  { "auto-shrink-images",			                     true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Autoshrink images", },
  { "cursive-font-family",			                   true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting,      "Cursive font family", },
  { "default-encoding",			                       true, false,  Char,    { .p = "iso-8859-1"      }, (void*) dwb_webkit_setting,      "Default encoding", }, 
  { "default-font-family",			                   true, false,  Char,    { .p = "sans-serif"      }, (void*) dwb_webkit_setting,      "Default font family", }, 
  { "default-font-size",			                     true, false,  Integer, { .i = 12                }, (void*) dwb_webkit_setting,      "Default font size", }, 
  { "default-monospace-font-size",			           true, false,  Integer, { .i = 10                }, (void*) dwb_webkit_setting,      "Default monospace font size", },
  { "enable-caret-browsing",			                 true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Caret Browsing", },
  { "enable-default-context-menu",			           true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Enable default context menu", }, 
  { "enable-developer-extras",			               true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Enable developer extras",    }, 
  { "enable-dom-paste",			                       true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Enable DOM paste", },
  { "enable-file-access-from-file-uris",			     true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "File access from file uris", },
  { "enable-html5-database",			                 true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Enable HTML5-database" },
  { "enable-html5-local-storage",			             true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Enable HTML5 local storage", },
  { "enable-java-applet",			                     true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Java Applets", },
  { "enable-offline-web-application-cache",			   true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Offline web application cache", },
  { "enable-page-cache",			                     true, false,  Boolean, { .b = false              }, (void*) dwb_webkit_setting,      "Page cache", },
  { "enable-plugins",			                         true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Plugins", },
  { "enable-private-browsing",			               true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Private Browsing", },
  { "enable-scripts",			                         true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Script", },
  { "enable-site-specific-quirks",			           true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Site specific quirks", },
  { "enable-spatial-navigation",			             true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Spatial navigation", },
  { "enable-spell-checking",			                 true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Spell checking", },
  { "enable-universal-access-from-file-uris",		   true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Universal access from file  uris", },
  { "enable-xss-auditor",			                     true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "XSS auditor", },
  { "enforce-96-dpi",			                         true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Enforce 96 dpi", },
  { "fantasy-font-family",			                   true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting,      "Fantasy font family", },
  { "javascript-can-access-clipboard",			       true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Javascript can access clipboard", },
  { "javascript-can-open-windows-automatically",   true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting,      "Javascript can open windows automatically", },
  { "minimum-font-size",			                     true, false,  Integer, { .i = 5                 }, (void*) dwb_webkit_setting,      "Minimum font size", },
  { "minimum-logical-font-size",			             true, false,  Integer, { .i = 5                 }, (void*) dwb_webkit_setting,      "Minimum logical font size", },
  { "monospace-font-family",			                 true, false,  Char,    { .p = "monospace"       }, (void*) dwb_webkit_setting,      "Monospace font family", },
  { "print-backgrounds",			                     true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Print backgrounds", },
  { "resizable-text-areas",			                   true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Resizable text areas", },
  { "sans-serif-font-family",			                 true, false,  Char,    { .p = "sans-serif"      }, (void*) dwb_webkit_setting,      "Sans serif font family", },
  { "serif-font-family",			                     true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting,      "Serif font family", },
  { "spell-checking-languages",			               true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting,      "Spell checking languages", },
  { "tab-key-cycles-through-elements",			       true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting,      "Tab cycles through elements in insert mode", },
  { "user-agent",			                             true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting,      "User agent", },
  { "user-stylesheet-uri",			                   true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting,      "User stylesheet uri", },
  { "zoom-step",			                             true, false,  Double,  { .d = 0.1               }, (void*) dwb_webkit_setting,      "Zoom Step", }, 
  { "custom-encoding",                             false, false, Char,    { .p = "utf-8"           }, (void*) dwb_webview_property,     "Custom encoding", },
  { "editable",                                    false, false, Boolean, { .b = false             }, (void*) dwb_webview_property,     "Content editable", },
  { "full-content-zoom",                           false, false, Boolean, { .b = false             }, (void*) dwb_webview_property,     "Full content zoom", },
  { "zoom-level",                                  false, false, Double,  { .d = 1.0               }, (void*) dwb_webview_property,     "Zoom level", },
  { "proxy",                                       false, true,  Boolean, { .b = true              }, (void*) dwb_set_websetting,         "HTTP-proxy", }, 
  { "cookie",                                      false, true,  Boolean, { .b = false             }, (void*) dwb_set_websetting,       "All Cookies allowed", }, 

  { "active_fg_color",                             false, true,  Char, { .p = "#ffffff"         },    (void*) dwb_reload_layout,      "UI: Active view foreground", }, 
  { "active_bg_color",                             false, true,  Char, { .p = "#000000"         },    (void*) dwb_reload_layout,      "UI: Active view background", }, 
  { "normal_fg_color",                             false, true,  Char, { .p = "#cccccc"         },    (void*) dwb_reload_layout,      "UI: Inactive view foreground", }, 
  { "normal_bg_color",                             false, true,  Char, { .p = "#505050"         },    (void*) dwb_reload_layout,      "UI: Inactive view background", }, 

  { "tab_active_fg_color",                         false, true,  Char, { .p = "#ffffff"         },    (void*) dwb_reload_layout,      "UI: Active view tabforeground", }, 
  { "tab_active_bg_color",                         false, true,  Char, { .p = "#000000"         },    (void*) dwb_reload_layout,      "UI: Active view tabbackground", }, 
  { "tab_normal_fg_color",                         false, true,  Char, { .p = "#cccccc"         },    (void*) dwb_reload_layout,      "UI: Inactive view tabforeground", }, 
  { "tab_normal_bg_color",                         false, true,  Char, { .p = "#505050"         },    (void*) dwb_reload_layout,      "UI: Inactive view tabbackground", }, 

  { "active_comp_fg_color",                        false, true,  Char, { .p = "#1793d1"         }, (void*) dwb_reload_colors,         "UI: Completion active foreground", }, 
  { "active_comp_bg_color",                        false, true,  Char, { .p = "#000000"         }, (void*) dwb_reload_colors,         "UI: Completion active background", }, 
  { "normal_comp_fg_color",                        false, true,  Char, { .p = "#eeeeee"         }, (void*) dwb_reload_colors,         "UI: Completion inactive foreground", }, 
  { "normal_comp_bg_color",                        false, true,  Char, { .p = "#151515"         }, (void*) dwb_reload_colors,         "UI: Completion inactive background", }, 

  { "insert_fg_color",                             false, true,  Char, { .p = "#ffffff"         }, (void*) dwb_reload_colors,         "UI: Insertmode foreground", }, 
  { "insert_bg_color",                             false, true,  Char, { .p = "#00008b"         }, (void*) dwb_reload_colors,         "UI: Insertmode background", }, 
  { "error_color",                                 false, true,  Char, { .p = "#ff0000"         }, (void*) dwb_reload_colors,         "UI: Error color", }, 

  { "settings_fg_color",                           false, true,  Char, { .p = "#ffffff"         }, (void*) dwb_reload_colors,         "UI: Settings view foreground", }, 
  { "settings_bg_color",                           false, true,  Char, { .p = "#151515"         }, (void*) dwb_reload_colors,         "UI: Settings view background", }, 
  { "settings_border",                             false, true,  Char, { .p = "1px dotted black"}, (void*) dwb_reload_colors,         "UI: Settings view border", }, 

  { "active_font_size",                            false, true,  Integer, { .i = 12                }, (void*) dwb_reload_layout,    "UI: Active view fontsize", }, 
  { "normal_font_size",                            false, true,  Integer, { .i = 10                 }, (void*) dwb_reload_layout,    "UI: Inactive view fontsize", }, 

  { "font",                                        false, true,  Char, { .p = "monospace"          }, (void*) dwb_reload_layout,    "UI: Font", }, 

  { "hint_vertical_off",                         false, true,  Integer, { .i = 0 },     (void*) dwb_reload_scripts,      "Hints: Vertical offset", }, 
  { "hint_horizontal_off",                         false, true,  Integer, { .i = -10 },     (void*) dwb_reload_scripts,      "Hints: Horizontal offset", }, 
  { "hint_letter_seq",                               false, true,  Char, { .p = "FDSARTGBVECWXQYIOPMNHZULKJ"  },     (void*) dwb_reload_scripts,      "Hints: Letter sequence for letter hints", }, 
  { "hint_style",                               false, true,  Char, { .p = "letter"         },     (void*) dwb_reload_scripts,      "Hints: Hintstyle (letter or number)", }, 
  { "hint_font_size",                              false, true,  Char, { .p = "12px"         },     (void*) dwb_reload_scripts,      "Hints: Font size", }, 
  { "hint_font_weight",                            false, true,  Char, { .p = "normal"         },     (void*) dwb_reload_scripts,      "Hints: Font weight", }, 
  { "hint_font_family",                            false, true,  Char, { .p = "monospace"      },     (void*) dwb_reload_scripts,      "Hints: Font family", }, 
  { "hint_fg_color",                              false, true,  Char, { .p = "#ffffff"         },    (void*) dwb_reload_scripts,      "Hints: Foreground color", }, 
  { "hint_bg_color",                              false, true,  Char, { .p = "#000088"         },    (void*) dwb_reload_scripts,      "Hints: Background color", }, 
  { "hint_active_color",                             false, true,  Char, { .p = "#00ff00"         }, (void*) dwb_reload_scripts,      "Hints: Active link color", }, 
  { "hint_normal_color",                             false, true,  Char, { .p = "#ffff99"         }, (void*) dwb_reload_scripts,      "Hints: Inactive link color", }, 
  { "hint_border",                             false, true,  Char, { .p = "2px dashed #000000"    }, (void*) dwb_reload_scripts,      "Hints: Hint Border", }, 
  { "overlay_border",                             false, true,  Char, { .p = "1px dotted #000000"    }, (void*) dwb_reload_scripts,      "Hints: Overlay Border", }, 
  { "hint_opacity",                             false, true,  Double, { .d = 0.75         },         (void*) dwb_reload_scripts,      "Hints: Hint Opacity", }, 
  { "overlay_opacity",                             false, true,  Double, { .d = 0.3         },         (void*) dwb_reload_scripts,      "Hints: Overlay opacity", }, 

  { "default_width",                               false, true,  Integer, { .i = 1280          }, NULL,                            "Default width", }, 
  { "default_height",                               false, true,  Integer, { .i = 1024          }, NULL,                            "Default height", }, 
  { "message_delay",                               false, true,  Integer, { .i = 2          }, NULL,                                "Message delay", }, 
  { "history_length",                              false, true,  Integer, { .i = 500          }, NULL,                                "History length", }, 
  { "size",                                        false, true,  Integer, { .i = 30          }, NULL,                                "UI: Default tiling area size (in %)", }, 
  { "factor",                                      false, true,  Double, { .d = 0.3          }, NULL,                                "UI: Default Zoom factor of tiling area", }, 
  { "layout",                                      false, true,  Char, { .p = "Normal Maximized" },  NULL,                             "UI: Default layout (Normal, Bottomstack, Maximized)", }, 
};/*}}}*/

/*}}}*/

/* UTIL {{{*/

gboolean
dwb_false() {
  return false;
}
gboolean
dwb_true() {
  return true;
}
/* dwb_return {{{*/
gchar *
dwb_return(const gchar *ret) {
  return g_strdup(ret);
}/*}}}*/

/* dwb_get_default_settings {{{*/
GHashTable * 
dwb_get_default_settings() {
  GHashTable *ret = g_hash_table_new(g_str_hash, g_str_equal);
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    WebSettings *new = g_malloc(sizeof(WebSettings)); 
    *new = *s;
    gchar *value = g_strdup(s->id);
    g_hash_table_insert(ret, value, new);
  }
  return ret;
}/*}}}*/

/* dwb_arg_to_char(Arg *arg, DwbType type) {{{*/
gchar *
dwb_arg_to_char(Arg *arg, DwbType type) {
  gchar *value = NULL;
  if (type == Boolean) {
    if (arg->b) 
      value = g_strdup("true");
    else
      value = g_strdup("false");
  }
  else if (type == Double) {
    value = g_strdup_printf("%.2f", arg->d);
  }
  else if (type == Integer) {
    value = g_strdup_printf("%d", arg->i);
  }
  else if (type == Char) {
    if (arg->p) {
      gchar *tmp = (gchar*) arg->p;
      value = g_strdup_printf(tmp);
    }
  }
  return value;
}/*}}}*/

/* dwb_char_to_arg {{{*/
Arg *
dwb_char_to_arg(gchar *value, DwbType type) {
  errno = 0;
  Arg *ret = NULL;
  if (type == Boolean && !value)  {
    Arg a =  { .b = false };
    ret = &a;
  }
  else if (value) {
    g_strstrip(value);
    if (strlen(value) == 0) {
      return NULL;
    }
    if (type == Boolean) {
      if(!g_ascii_strcasecmp(value, "false") || !strcmp(value, "0")) {
        Arg a = { .b = false };
        ret = &a;
      }
      else {
        Arg a = { .b = true };
        ret = &a;
      }
    }
    else if (type == Integer) {
      gint n = strtol(value, NULL, 10);
      if (n != LONG_MAX &&  n != LONG_MIN && !errno ) {
        Arg a = { .i = n };
        ret = &a;
      }
    }
    else if (type == Double) {
      gdouble d;
      if ((d = g_strtod(value, NULL)) ) {
        Arg a = { .d = d };
        ret = &a;
      }
    }
    else if (type == Char) {
      Arg a = { .p = g_strdup(value) };
      ret = &a;
    }
  }
  return ret;
}/*}}}*/

/* dwb_keyval_to_char (guint keyval)      return: char  *{{{*/
gchar *
dwb_keyval_to_char(guint keyval) {
  gchar *key = g_malloc(6);
  guint32 unichar;
  gint length;
  if ( (unichar = gdk_keyval_to_unicode(keyval)) ) {
    if ( (length = g_unichar_to_utf8(unichar, key)) ) {
      memset(&key[length], '\0', 6-length); 
    }
  }
  if (length && isprint(key[0])) {
    return key;
  }
  else {
    g_free(key);
    return NULL;
  }
}/*}}}*/

/* dwb_char_to_keyval (gchar *buf) {{{*/
guint 
dwb_char_to_keyval(gchar *buf) {
  return  0;
}/*}}}*/

/* dwb_keymap_sort_text {{{*/
gint
dwb_keymap_sort_text(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->desc, b->map->desc);
}/*}}}*/

/* dwb_web_settings_sort{{{*/
gint
dwb_web_settings_sort(WebSettings *a, WebSettings *b) {
  return strcmp(a->desc, b->desc);
}/*}}}*/

/*dwb_get_directory_content(const gchar *filename) {{{*/
gchar *
dwb_get_directory_content(const gchar *dirname) {
  GDir *dir;
  GString *buffer = g_string_new(NULL);
  gchar *content;
  GError *error = NULL;
  gchar *filename, *filepath;
  gchar *ret;

  if ( (dir = g_dir_open(dirname, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      if (filename[0] != '.') {
        filepath = g_strconcat(dirname, "/",  filename, NULL);
        if (g_file_get_contents(filepath, &content, NULL, &error)) {
          g_string_append(buffer, content);
        }
        else {
          fprintf(stderr, "Cannot read %s: %s\n", filename, error->message);
        }
        g_free(filepath);
        g_free(content);
      }
    }
    g_dir_close (dir);
  }
  ret = g_strdup(buffer->str);
  g_string_free(buffer, true);
  return ret;

}/*}}}*/

/* dwb_get_file_content(const gchar *filename){{{*/
gchar *
dwb_get_file_content(const gchar *filename) {
  GError *error = NULL;
  gchar *content = NULL;
  if (!(g_file_test(filename, G_FILE_TEST_IS_REGULAR) &&  g_file_get_contents(filename, &content, NULL, &error))) {
    fprintf(stderr, "Cannot open %s: %s\n", filename, error ? error->message : "file not found");
  }
  return content;
}/*}}}*/

/* dwb_set_file_content(const gchar *filename, const gchar *content){{{*/
void
dwb_set_file_content(const gchar *filename, const gchar *content) {
  GError *error = NULL;
  if (!g_file_set_contents(filename, content, -1, &error)) {
    fprintf(stderr, "Cannot save %s : %s", filename, error->message);
  }
}/*}}}*/

/* dwb_navigation_new(const gchar *uri, const gchar *title) {{{*/
Navigation *
dwb_navigation_new(const gchar *uri, const gchar *title) {
  Navigation *nv = malloc(sizeof(Navigation)); 
  nv->first = uri ? g_strdup(uri) : NULL;
  nv->second = title ? g_strdup(title) : NULL;
  return nv;

}/*}}}*/

/* dwb_navigation_new_from_line(const gchar *text){{{*/
Navigation * 
dwb_navigation_new_from_line(const gchar *text) {
  gchar **line;
  Navigation *nv = NULL;
  
  if (text) {
    line = g_strsplit(text, " ", 2);
    nv = dwb_navigation_new(line[0], line[1]);
    g_free(line);
  }
  return nv;
}/*}}}*/

/* dwb_navigation_free(Navigation *n){{{*/
void
dwb_navigation_free(Navigation *n) {
  if (n->first)   
    g_free(n->first);
  if (n->second)  
    g_free(n->second);
  g_free(n);
}/*}}}*/

/* dwb_quickmark_new_from_line(const gchar *line) {{{*/
Quickmark * 
dwb_quickmark_new_from_line(const gchar *line) {
  Quickmark *q = NULL;
  gchar **token;
  if (line) {
    token = g_strsplit(line, " ", 3);
    q = dwb_quickmark_new(token[1], token[2], token[0]);
    g_strfreev(token);
  }
  return q;

}/*}}}*/

/* dwb_quickmark_new(const gchar *uri, const gchar *title,  const gchar *key)  {{{*/
Quickmark *
dwb_quickmark_new(const gchar *uri, const gchar *title, const gchar *key) {
  Quickmark *q = malloc(sizeof(Quickmark));
  q->key = key ? g_strdup(key) : NULL;
  q->nav = dwb_navigation_new(uri, title);
  return q;
}/* }}} */

/* dwb_quickmark_free(Quickmark *q) {{{*/
void
dwb_quickmark_free(Quickmark *q) {
  if (q->key) 
    g_free(q->key);
  if (q->nav)
    dwb_navigation_free(q->nav);
  g_free(q);

}/*}}}*/

/* }}} */

/* CALLBACKS {{{*/

/* WEB_VIEW_CALL_BACKS {{{*/

/* dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
gboolean
dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  Arg arg = { .p = gl };

  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);

  View *v = gl->data;

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_insert_mode(NULL);
  }
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    if (v->status->hover_uri && e->button == 2) {
      Arg a = { .p = v->status->hover_uri };
      dwb.state.nv = OpenNewView;
      dwb_load_uri(&a);
    }
  }
  else if (e->button == 1) {
    if (e->type == GDK_BUTTON_PRESS) {
      dwb_focus(gl);
    }
    if (e->type == GDK_2BUTTON_PRESS) {
      dwb_push_master(&arg);
    }
  }
  dwb_normal_mode(true);
  return false;
}/*}}}*/

/* dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
gboolean 
dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  Arg a = { .p = gl };
  dwb_remove_view(&a);
  return true;
}/*}}}*/

/* dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {{{*/
gboolean 
dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {
  // TODO implement
  if (!strcmp(sourceid, KEY_SETTINGS)) {
    KeyValue value;
    gchar **token = g_strsplit(message, " ", 2);

    value.id = g_strdup(token[0]);

    gchar  **keys = g_strsplit(token[1], " ", -1);
    value.key = dwb_strv_to_key(keys, g_strv_length(keys));

    dwb.keymap = dwb_keymap_add(dwb.keymap, value);
    dwb.keymap = g_slist_sort(dwb.keymap, (GCompareFunc)dwb_keymap_sort_text);
    dwb_set_normal_message(dwb.state.fview, "Key saved.", true);

    g_strfreev(token);
    g_strfreev(keys);
    dwb_normal_mode(false);

  }
  else if (!(strcmp(sourceid, SETTINGS))) {
    gchar **token = g_strsplit(message, " ", 2);
    GHashTable *t = dwb.state.setting_apply == Global ? dwb.settings : ((View*)dwb.state.fview->data)->setting;
    if (token[0]) {
      WebSettings *s = g_hash_table_lookup(t, token[0]);
      Arg *a = dwb_char_to_arg(token[1], s->type);
      if (a) {
        s->arg = *a;
        dwb_apply_settings(s);
      }
    }
    g_strfreev(token);
  }
  return false;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
GtkWidget * 
dwb_web_view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  // TODO implement
  dwb_add_view(NULL); 
  return ((View*)dwb.state.fview->data)->web;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
gboolean 
dwb_web_view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_hovering_over_link_cb(WebKitWebView *, gchar *title, gchar *uri, GList *) {{{*/
void 
dwb_web_view_hovering_over_link_cb(WebKitWebView *web, gchar *title, gchar *uri, GList *gl) {
  // TODO implement
  VIEW(gl)->status->hover_uri = uri;
  if (uri) {
    dwb_set_status_text(gl, uri, NULL, NULL);
  }
  else {
    dwb_update_status_text(gl);
  }

}/*}}}*/

/* dwb_web_view_mime_type_policy_cb {{{*/
gboolean 
dwb_web_view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, gchar *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  // TODO implement
  if (webkit_web_view_can_show_mime_type(web, mimetype)) {
    return  false;
  }
  else {
    webkit_web_policy_decision_download(policy);
    return true;
  }
}/*}}}*/

/* dwb_web_view_navigation_policy_cb {{{*/
gboolean 
dwb_web_view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  const gchar *request_uri = webkit_network_request_get_uri(request);
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);

  if (dwb.state.mode == SearchFieldMode && reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED ) {
    webkit_web_policy_decision_ignore(policy);
    dwb.state.search_engine = dwb.state.form_name && !g_strrstr(request_uri, HINT_SEARCH_SUBMIT) 
      ? g_strdup_printf("%s?%s=%s", request_uri, dwb.state.form_name, HINT_SEARCH_SUBMIT) 
      : g_strdup(request_uri);
    dwb_set_normal_message(dwb.state.fview, "Enter a keyword for this search:", false);
    dwb_focus_entry();
    dwb.state.mode = SearchKeywordMode;
    return true;
  }
  return false;
}/*}}}*/

/* dwb_web_view_new_window_policy_cb {{{*/
gboolean 
dwb_web_view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action, WebKitWebPolicyDecision *policy, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_resource_request_cb{{{*/
void 
dwb_web_view_resource_request_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {

}/*}}}*/

/* dwb_web_view_script_alert_cb {{{*/
gboolean 
dwb_web_view_script_alert_cb(WebKitWebView *web, WebKitWebFrame *frame, gchar *message, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_window_object_cleared_cb {{{*/
void 
dwb_web_view_window_object_cleared_cb(WebKitWebView *web, WebKitWebFrame *frame, 
    JSGlobalContextRef *context, JSObjectRef *object, GList *gl) {
  JSStringRef script; 
  JSValueRef exc;

  script = JSStringCreateWithUTF8CString(dwb.misc.scripts);
  JSEvaluateScript((JSContextRef)context, script, JSContextGetGlobalObject((JSContextRef)context), 
      NULL, 0, &exc);
  JSStringRelease(script);
}/*}}}*/

/* dwb_web_view_title_cb {{{*/
void 
dwb_web_view_title_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_status(gl);
}/*}}}*/

/* dwb_web_view_focus_cb {{{*/
gboolean 
dwb_web_view_focus_cb(WebKitWebView *web, GtkDirectionType *direction, GList *gl) {
  return false;
}/*}}}*/

/* dwb_web_view_load_status_cb {{{*/
void 
dwb_web_view_load_status_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  gdouble progress = webkit_web_view_get_progress(web);
  gchar *text = NULL;
  switch (status) {
    case WEBKIT_LOAD_PROVISIONAL: 
      break;
    case WEBKIT_LOAD_COMMITTED: 
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      dwb_prepend_navigation(gl, &dwb.fc.history);
      dwb_normal_mode(false);
      break;
    case WEBKIT_LOAD_FAILED: 
      break;
    default:
      text = g_strdup_printf("loading [%d%%]", (gint)(progress * 100));
      dwb_set_status_text(gl, text, NULL, NULL); 
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), text);
      g_free(text);
      break;
  }
}/*}}}*/

/*}}}*/

/* dwb_entry_keyrelease_cb {{{*/
gboolean 
dwb_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
  if (dwb.state.mode == HintMode || dwb.state.mode == SearchFieldMode) {
    if (DIGIT(e) || e->keyval == GDK_Tab) {
      return true;
    }
    else {
      return dwb_update_hints(e);
    }
  }
  return false;
}/*}}}*/

/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
gboolean 
dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  if (dwb.state.mode == HintMode || dwb.state.mode == SearchFieldMode) {
    if (e->keyval == GDK_BackSpace) {
      return false;
    }
    else if (DIGIT(e) || e->keyval == GDK_Tab) {
      return dwb_update_hints(e);
    }
  }
  if (e->keyval == GDK_Tab) {
    dwb_complete(e->state & GDK_CONTROL_MASK);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
gboolean 
dwb_entry_activate_cb(GtkEntry* entry) {
  gchar *text = g_strdup(gtk_entry_get_text(entry));
  gboolean ret = false;

  if (dwb.state.mode == HintMode) {
    ret = false;
  }
  else if (dwb.state.mode == FindMode) {
    gtk_widget_grab_focus(CURRENT_VIEW()->scroll);
    dwb_search(NULL);
  }
  else if (dwb.state.mode == SearchKeywordMode) {
    dwb_save_searchengine();
  }
  else {
    Arg a = { .n = 0, .p = text };
    dwb_prepend_navigation_with_argument(&dwb.fc.commands, text, NULL);
    dwb_load_uri(&a);
    dwb_normal_mode(true);
  }
  g_free(text);

  return false;
}/*}}}*/

/* dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
gboolean 
dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;

  gchar *key = dwb_keyval_to_char(e->keyval);
  if (e->keyval == GDK_Escape) {
    dwb_normal_mode(true);
    return false;
  }
  else if (dwb.state.mode == InsertMode) {
    return false;
  }
  else if (dwb.state.mode == QuickmarkSave) {
    if (key) {
      dwb_save_quickmark(key);
    }
    return true;
  }
  else if (dwb.state.mode == QuickmarkOpen) {
    if (key) {
      dwb_open_quickmark(key);
    }
    return true;
  }
  else if (gtk_widget_has_focus(dwb.gui.entry)) {
    return false;
  }
  ret = dwb_eval_key(e);
  g_free(key);
  
  return ret;
}/*}}}*/

/* dwb_cookie_changed_cb {{{*/
void 
dwb_cookie_changed_cb(SoupCookieJar *cookiejar, SoupCookie *old, SoupCookie *new) {
  if (new) {
    dwb.state.last_cookie = soup_cookie_copy(new);
    gchar *message = g_strdup_printf("Cookie received, domain: %s", new->domain);
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
    if  (dwb_test_cookie_allowed(new->domain) || ((WebSettings*)g_hash_table_lookup(dwb.settings, "cookie"))->arg.b) {
      dwb_save_cookies();
    }
  }
}/*}}}*/

/* dwb_key_release_cb {{{*/
gboolean 
dwb_key_release_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  if (e->keyval == GDK_Tab) {
    return true;
  }
  if (dwb.state.mode == InsertMode && e->keyval == GDK_Return) {
    dwb_normal_mode(true);
  }
  return false;
}/*}}}*/

/*}}}*/

/* COMMANDS {{{*/

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

gboolean
dwb_focus_input(Arg *a) {
  gchar *value;
  value = dwb_execute_script("focus_input()");
  if (!strcmp(value, "_dwb_no_input_")) {
    return false;
  }
  return true;
}

/* dwb_simple_command(keyMap *km) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  gboolean (*func)(void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;

  if (func(arg)) {
    if (strlen(km->map->desc)) {
      dwb_set_normal_message(dwb.state.fview, km->map->desc, km->map->hide);
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

/* dwb_add_search_field(Arg *) {{{*/
gboolean
dwb_add_search_field(Arg *a) {
  gchar *value;
  gboolean ret = true;
  value = dwb_execute_script("add_searchengine()");
  if (value ) {
    if (!strcmp(value, "_dwb_no_hints_")) {
      ret = false;
    }
    else {
      dwb.state.mode = SearchFieldMode;
      dwb_focus_entry();
    }
    g_free(value);
  }
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
  return true;
}/*}}}*/

/* dwb_toggle_proxy {{{*/
gboolean
dwb_toggle_proxy(Arg *a) {
  SoupURI *uri = NULL;

  WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
  s->arg.b = !s->arg.b;

  printf("%d\n", s->arg.b);
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
  gtk_widget_show(dwb.gui.entry);
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
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_string_append(buffer, HTML_DIV_START);
    g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, m->map->desc);
    g_string_append_printf(buffer, HTML_DIV_KEYS_VALUE, m->map->id, dwb_modmask_to_string(m->mod), m->key ? : "");
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
  l = g_list_sort(l, (GCompareFunc)dwb_web_settings_sort);

  g_string_append_printf(buffer, SETTINGS_VIEW, dwb.color.settings_bg_color, dwb.color.settings_fg_color, dwb.misc.settings_border);
  g_string_append_printf(buffer, HTML_H2, setting_string, dwb.misc.profile);

  g_string_append(buffer, HTML_BODY_START);
  g_string_append(buffer, HTML_FORM_START);
  for (; l; l=l->next) {
    WebSettings *m = l->data;
    if (!m->global || (m->global && dwb.state.setting_apply == Global)) {
      g_string_append(buffer, HTML_DIV_START);
      g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, m->desc);
      if (m->type == Boolean) {
        const gchar *value = m->arg.b ? "checked" : "";
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_CHECKBOX, m->id, value);
      }
      else {
        gchar *value = dwb_arg_to_char(&m->arg, m->type);
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_VALUE, m->id, value ? value : "");
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

/* dwb_insert_mode(Arg *arg) {{{*/
gboolean
dwb_insert_mode(Arg *arg) {
  if (dwb.state.mode == HintMode) {
    dwb_set_normal_message(dwb.state.fview, INSERT_MODE, true);
  }
  dwb_view_modify_style(dwb.state.fview, &dwb.color.insert_fg, &dwb.color.insert_bg, NULL, NULL, NULL, 0);
  dwb.state.mode = InsertMode;
  return true;
}/*}}}*/

/* dwb_allow_cookie {{{*/
gboolean
dwb_allow_cookie(Arg *arg) {
  if (dwb.state.last_cookie) {
    dwb.fc.cookies_allow = g_list_append(dwb.fc.cookies_allow, dwb.state.last_cookie->domain);
    soup_cookie_jar_add_cookie(dwb.state.cookiejar, dwb.state.last_cookie);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_bookmark {{{*/
gboolean 
dwb_bookmark(Arg *arg) {
  GList *gl = arg && arg->p ? arg->p : dwb.state.fview;

  return dwb_prepend_navigation(gl, &dwb.fc.bookmarks);
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

  GList *gl = arg && arg->p ? arg->p : dwb.state.fview;
  View *v = gl->data;

  GtkAdjustment *a = arg->n == Left || arg->n == Right 
    ? gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll)) 
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
    dwb_update_status_text(gl);
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
  WebKitWebView *w = WEBVIEW_FROM_ARG(arg);
  //if (!dwb.state.nummod) {
  //  if (webkit_web_view_can_go_back(w)) {
      webkit_web_view_go_back(w);
      ret = true;
  //  }
  //}
  //dwb_normal_mode(false);
  return ret;
}
gboolean 
dwb_history_forward(Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = WEBVIEW_FROM_ARG(arg);
  if (!dwb.state.nummod) {
    if (webkit_web_view_can_go_forward(w)) {
      webkit_web_view_go_forward(w);
      ret = true;
    }
  }
  return ret;
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

/* dwb_open(Arg *arg) {{{*/
gboolean  
dwb_open(Arg *arg) {
  dwb.state.nv = arg->n;
  //gtk_widget_grab_focus(dwb.gui.entry);
  
  dwb_focus_entry();
  dwb.state.mode = OpenMode;
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

/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  gint fact = dwb.state.layout & BottomStack;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
  dwb.state.size = size;
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
/*}}}*/

/* DWB_WEB_VIEW {{{*/

/* dwb_web_view_add_history_item(GList *gl) {{{*/
void 
dwb_web_view_add_history_item(GList *gl) {
  WebKitWebView *web = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(web);
  const gchar *title = webkit_web_view_get_title(web);
  WebKitWebBackForwardList *bl = webkit_web_view_get_back_forward_list(web);
  WebKitWebHistoryItem *hitem = webkit_web_history_item_new_with_data(uri,  title);
  webkit_web_back_forward_list_add_item(bl, hitem);
}/*}}}*/

void 
dwb_set_active_style(GList *gl) {
  dwb_view_modify_style(gl, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_bold, dwb.font.active_size);
}
void 
dwb_set_normal_style(GList *gl) {
  dwb_view_modify_style(gl, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_bold, dwb.font.normal_size);
}

/* dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {{{*/
void 
dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {
  View *v = gl->data;
  if (fg) {
    gtk_widget_modify_fg(v->rstatus, GTK_STATE_NORMAL, fg);
    gtk_widget_modify_fg(v->lstatus, GTK_STATE_NORMAL, fg);
    gtk_widget_modify_text(v->entry, GTK_STATE_NORMAL, fg);
  }
  if (bg) {
    gtk_widget_modify_bg(v->statusbox, GTK_STATE_NORMAL, bg);
    gtk_widget_modify_base(v->entry, GTK_STATE_NORMAL, bg);
  }
  if (tabfg) 
    gtk_widget_modify_fg(v->tablabel, GTK_STATE_NORMAL, tabfg);
  if (tabbg)
    gtk_widget_modify_bg(v->tabevent, GTK_STATE_NORMAL, tabbg);
  if (fd) {
    pango_font_description_set_absolute_size(fd, fontsize * PANGO_SCALE);
    gtk_widget_modify_font(v->rstatus, fd);
    gtk_widget_modify_font(v->lstatus, fd);
    gtk_widget_modify_font(v->tablabel, fd);
  }

} /*}}}*/

/* dwb_web_view_init_signals(View *v) {{{*/
void
dwb_web_view_init_signals(GList *gl) {
  View *v = gl->data;
  //g_signal_connect(v->vbox, "key-press-event", G_CALLBACK(dwb_key_press_cb), v);
  g_signal_connect(v->web, "button-press-event",                    G_CALLBACK(dwb_web_view_button_press_cb), gl);
  g_signal_connect(v->web, "close-web-view",                        G_CALLBACK(dwb_web_view_close_web_view_cb), gl);
  g_signal_connect(v->web, "console-message",                       G_CALLBACK(dwb_web_view_console_message_cb), gl);
  g_signal_connect(v->web, "create-web-view",                       G_CALLBACK(dwb_web_view_create_web_view_cb), gl);
  g_signal_connect(v->web, "download-requested",                    G_CALLBACK(dwb_web_view_download_requested_cb), gl);
  g_signal_connect(v->web, "hovering-over-link",                    G_CALLBACK(dwb_web_view_hovering_over_link_cb), gl);
  g_signal_connect(v->web, "mime-type-policy-decision-requested",   G_CALLBACK(dwb_web_view_mime_type_policy_cb), gl);
  g_signal_connect(v->web, "navigation-policy-decision-requested",  G_CALLBACK(dwb_web_view_navigation_policy_cb), gl);
  g_signal_connect(v->web, "new-window-policy-decision-requested",  G_CALLBACK(dwb_web_view_new_window_policy_cb), gl);
  g_signal_connect(v->web, "resource-request-starting",             G_CALLBACK(dwb_web_view_resource_request_cb), gl);
  g_signal_connect(v->web, "script-alert",                          G_CALLBACK(dwb_web_view_script_alert_cb), gl);
  g_signal_connect(v->web, "window-object-cleared",                 G_CALLBACK(dwb_web_view_window_object_cleared_cb), gl);

  g_signal_connect(v->web, "notify::load-status",                   G_CALLBACK(dwb_web_view_load_status_cb), gl);
  g_signal_connect(v->web, "notify::title",                         G_CALLBACK(dwb_web_view_title_cb), gl);
  g_signal_connect(v->web, "focus",                                 G_CALLBACK(dwb_web_view_focus_cb), gl);

  g_signal_connect(v->entry, "key-press-event", G_CALLBACK(dwb_entry_keypress_cb), NULL);
  g_signal_connect(v->entry, "key-release-event", G_CALLBACK(dwb_entry_keyrelease_cb), NULL);
  g_signal_connect(v->entry, "activate", G_CALLBACK(dwb_entry_activate_cb), NULL);
} /*}}}*/

/* dwb_create_web_view(View *v) {{{*/
GList * 
dwb_create_web_view(GList *gl) {
  View *v = malloc(sizeof(View));
  ViewStatus *status = malloc(sizeof(ViewStatus));
  v->status = status;
  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();

  // Entry
  v->entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), NULL);

  gtk_widget_modify_font(v->entry, dwb.font.fd_bold);
  gtk_entry_set_has_frame(GTK_ENTRY(v->entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), false);

  // completion
  v->bottombox = gtk_vbox_new(false, 1);

  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new(NULL);
  v->rstatus = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(v->lstatus), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(v->rstatus), 1.0, 0.5);
  status_hbox = gtk_hbox_new(false, 5);
  gtk_box_pack_end(GTK_BOX(v->bottombox), v->statusbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->rstatus, true, true, 0);
  gtk_label_set_use_markup(GTK_LABEL(v->lstatus), true);
  gtk_label_set_use_markup(GTK_LABEL(v->rstatus), true);

  // Srolling
  v->scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(v->scroll), v->web);
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(dwb_true), NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->scroll, true, true, 0);


  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tablabel = gtk_label_new(NULL);
  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  gtk_widget_modify_font(v->tablabel, dwb.font.fd_normal);

  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->bottombox, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_start(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0);
  gtk_widget_show(v->vbox);
  gtk_widget_show_all(v->bottombox);
  gtk_widget_show_all(v->scroll);
  gtk_widget_show_all(v->tabevent);

  gl = g_list_prepend(gl, v);
  dwb_web_view_init_signals(gl);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  // apply settings
  v->setting = dwb_get_default_settings();
  return gl;
} /*}}}*/

/* dwb_add_view(Arg *arg) ret: View *{{{*/
void 
dwb_add_view(Arg *arg) {
  if (dwb.state.views) {
    View *views = dwb.state.views->data;
    CLEAR_COMMAND_TEXT(dwb.state.views);
    gtk_widget_reparent(views->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
    if (dwb.state.layout & Maximized) {
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
      if (dwb.state.fview != dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
    }
  }
  dwb.state.views = dwb_create_web_view(dwb.state.views);
  dwb_focus(dwb.state.views);

  for (GList *l = g_hash_table_get_values(((View*)dwb.state.views->data)->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (!s->builtin && !s->global) {
      s->func(dwb.state.views, s);
    }
  }
  dwb_update_layout();
} /*}}}*//*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, GdkColor *fg,  PangoFontDescription *fd) {
  gtk_label_set_text(GTK_LABEL(label), text);
  if (fg) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    gtk_widget_modify_font(label, fd);
  }
}/*}}}*/

/* hide command text {{{*/
void 
dwb_source_remove(GList *gl) {
  guint id;
  View *v = gl->data;
  if ( (id = v->status->message_id) ) {
    g_source_remove(id);
  }
}
gpointer 
dwb_hide_message(GList *gl) {
  CLEAR_COMMAND_TEXT(gl);
  return NULL;
}/*}}}*/

/* dwb_set_normal_message {{{*/
void 
dwb_set_normal_message(GList *gl, const gchar  *text, gboolean hide) {
  View *v = gl->data;
  dwb_set_status_bar_text(v->lstatus, text, &dwb.color.active_fg, dwb.font.fd_bold);
  dwb_source_remove(gl);
  if (hide) {
    v->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  }
}/*}}}*/

/* dwb_set_error_message {{{*/
void 
dwb_set_error_message(GList *gl, const gchar *error) {
  dwb_source_remove(gl);
  dwb_set_status_bar_text(VIEW(gl)->lstatus, error, &dwb.color.error, dwb.font.fd_bold);
  VIEW(gl)->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
}/*}}}*/

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl) {
  View *v = gl->data;
  GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  const gchar *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  GString *string = g_string_new(uri);
  gdouble lower = gtk_adjustment_get_lower(a);
  gdouble upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  gdouble value = gtk_adjustment_get_value(a); 
  gchar *position = 
          upper == lower ? g_strdup(" -- [all]") : 
          value == lower ? g_strdup(" -- [top]") : 
          value == upper ? g_strdup(" -- [bot]") : 
          g_strdup_printf(" -- [%02d%%]", (gint)(value * 100/upper));
  g_string_append(string, position);

  dwb_set_status_text(gl, string->str, NULL, NULL);
  g_string_free(string, true);
  g_free(position);
}/*}}}*/

/*dwb_set_status_text(GList *gl, const gchar *text, GdkColor *fg, PangoFontDescription *fd) {{{*/
void 
dwb_set_status_text(GList *gl, const gchar *text, GdkColor *fg, PangoFontDescription *fd) {
  gchar *escaped = g_markup_escape_text(text, -1);
  dwb_set_status_bar_text(VIEW(gl)->rstatus, escaped, fg, fd);
  g_free(escaped);
}/*}}}*/
/*}}}*/

/* COMPLETION {{{*/

/* dwb_clean_completion() {{{*/
void 
dwb_clean_completion() {
  for (GList *l = dwb.state.completions; l; l=l->next) {
    g_free(l->data);
  }
  g_list_free(dwb.state.completions);
  gtk_widget_destroy(CURRENT_VIEW()->compbox);
  dwb.state.completions = NULL;
  dwb.state.active_comp = NULL;
}/*}}}*/

/* dwb_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {{{*/
void 
dwb_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {
  gtk_widget_modify_fg(c->llabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->rlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_bg(c->event, GTK_STATE_NORMAL, bg);
  gtk_widget_modify_font(c->llabel, fd);
  gtk_widget_modify_font(c->rlabel, dwb.font.fd_oblique);
}/*}}}*/

/* dwb_get_completion_item(Navigation *)      return: Completion * {{{*/
Completion * 
dwb_get_completion_item(Navigation *n) {
  Completion *c = malloc(sizeof(Completion));

  c->rlabel = gtk_label_new(n->second);
  c->llabel = gtk_label_new(n->first);
  c->event = gtk_event_box_new();
  GtkWidget *hbox = gtk_hbox_new(false, 0);
  gtk_box_pack_start(GTK_BOX(hbox), c->llabel, true, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->rlabel, true, true, 5);

  gtk_misc_set_alignment(GTK_MISC(c->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->rlabel), 1.0, 0.5);

  dwb_modify_completion_item(c, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);

  gtk_container_add(GTK_CONTAINER(c->event), hbox);
  return c;
}/*}}}*/

/* dwb_init_completion {{{*/
GList * 
dwb_init_completion(GList *store, GList *gl) {
  Navigation *n;
  const gchar *input = GET_TEXT();
  for (GList *l = gl; l; l=l->next) {
    n = l->data;
    if (g_strrstr(n->first, input)) {
      Completion *c = dwb_get_completion_item(n);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      store = g_list_prepend(store, c);
    }
  }
  return store;
}/*}}}*/

/* dwb_completion_set_text(Completion *) {{{*/
void
dwb_completion_set_entry_text(Completion *c) {
  const gchar *text = gtk_label_get_text(GTK_LABEL(c->llabel));
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);

}/*}}}*/

/* dwb_complete {{{*/
void
dwb_complete(gint back) {
  View *v = CURRENT_VIEW();

  if (dwb.state.mode != CompletionMode) {
    v->compbox = gtk_vbox_new(true, 0);
    gtk_box_pack_end(GTK_BOX(v->bottombox), v->compbox, false, false, 0);
    if (dwb.state.mode == OpenMode) {
      dwb.state.completions = dwb_init_completion(dwb.state.completions, dwb.fc.history);
      dwb.state.completions = dwb_init_completion(dwb.state.completions, dwb.fc.bookmarks);
      dwb.state.completions = dwb_init_completion(dwb.state.completions, dwb.fc.commands);
    }
    if (!dwb.state.completions) {
      return;
    }
    int i=0;
    if (back) {
      dwb.state.active_comp = g_list_last(dwb.state.completions);
      for (GList *l = dwb.state.active_comp; l && i<dwb.misc.max_c_items; l=l->prev, i++) {
        gtk_widget_show_all(((Completion*)l->data)->event);
      }
    }
    else {
      dwb.state.active_comp = g_list_first(dwb.state.completions);
      for (GList *l = dwb.state.active_comp; l && i<dwb.misc.max_c_items; l=l->next, i++) {
        gtk_widget_show_all(((Completion*)l->data)->event);
      }
    }
    dwb_modify_completion_item(dwb.state.active_comp->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
    dwb_completion_set_entry_text(dwb.state.active_comp->data);
    gtk_widget_show(CURRENT_VIEW()->compbox);
    dwb.state.mode = CompletionMode;
  }
  else if (dwb.state.completions && dwb.state.active_comp) {
    // TODO implement update completion
    GList *old, *new;
    Completion *c;

    gint length = g_list_length(dwb.state.completions);
    gint items = MAX(length, dwb.misc.max_c_items);
    gint r = (dwb.misc.max_c_items) % 2;
    gint offset = dwb.misc.max_c_items / 2 - 1 + r;

    old = dwb.state.active_comp;
    gint position = g_list_position(dwb.state.completions, dwb.state.active_comp) + 1;
    if (!back) {
      if (! (new = old->next) ) {
        new = g_list_first(dwb.state.completions);
      }
      if (position > offset &&  position < items - offset - 1 + r) {
        c = g_list_nth(dwb.state.completions, position - offset - 1)->data;
        gtk_widget_hide(c->event);
        c = g_list_nth(dwb.state.completions, position + offset + 1 - r)->data;
        gtk_widget_show_all(c->event);
      }
      else {
        if (position == items || position  == 1) {
          gtk_widget_hide_all(v->compbox);
          gtk_widget_show(v->compbox);
          int i = 0;
          for (GList *l = g_list_first(dwb.state.completions); l && i<dwb.misc.max_c_items ;l=l->next, i++) {
            gtk_widget_show_all(((Completion*)l->data)->event);
          }
        }
      }
    }
    else {
      if (! (new = old->prev) ) {
        new = g_list_last(dwb.state.completions);
      }
      if (position > offset   &&  position < items - offset - 1 + r) {
        c = g_list_nth(dwb.state.completions, position - offset - 1)->data;
        gtk_widget_show_all(c->event);
        c = g_list_nth(dwb.state.completions, position + offset + 1 - r)->data;
        gtk_widget_hide(c->event);
      }
      else {
        if (position == 1) {
          gtk_widget_hide_all(v->compbox);
          gtk_widget_show(v->compbox);
          int i = 0;
          for (GList *l = g_list_last(dwb.state.completions); l && i<dwb.misc.max_c_items ;l=l->prev, i++) {
            c = l->data;
            gtk_widget_show_all(c->event);
          }
        }
      }
    }
    dwb_modify_completion_item(old->data, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);
    dwb_modify_completion_item(new->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
    dwb.state.active_comp = new;
    dwb_completion_set_entry_text(dwb.state.active_comp->data);
  }

}/*}}}*/
/*}}}*/

/* FUNCTIONS {{{*/

/* dwb_reload_colors(GList *,  WebSettings  *s) {{{*/
void 
dwb_reload_scripts(GList *gl, WebSettings *s) {
  dwb_init_scripts();
}/*}}}*/

/* dwb_reload_colors(GList *,  WebSettings  *s) {{{*/
void 
dwb_reload_colors(GList *gl, WebSettings *s) {
  dwb_init_style();
}/*}}}*/

/* dwb_reload_layout(GList *,  WebSettings  *s) {{{*/
void 
dwb_reload_layout(GList *gl, WebSettings *s) {
  dwb_init_style();
  for (GList *l = dwb.state.views; l; l=l->next) {
    if (l == dwb.state.fview) {
      dwb_set_active_style(l);
    }
    else {
      dwb_set_normal_style(l);
    }
  }
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(void) {
  gchar *text = g_strdup(GET_TEXT());
  if (text) {
    g_strstrip(text);
    if (text && strlen(text) > 0) {
      dwb_prepend_navigation_with_argument(&dwb.fc.searchengines, text, dwb.state.search_engine);
      dwb_set_normal_message(dwb.state.fview, "Search saved", true);
      g_free(dwb.state.search_engine);
    }
    else {
      dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
    }
    g_free(text);
  }
  dwb_normal_mode(false);
  
}/*}}}*/

/* dwb_set_websetting(GList *, WebSettings *) {{{*/
void
dwb_set_websetting(GList *gl, WebSettings *s) {
  if (s->global) {
    WebSettings *new = g_hash_table_lookup(dwb.settings, s->id);
    new->arg = s->arg;
  }

}/*}}}*/

Layout
dwb_layout_from_char(const gchar *desc) {
  gchar **token = g_strsplit(desc, " ", 0);
  gint i=0;
  Layout layout;
  while (token[i]) {
    if (!(layout & BottomStack) && !g_ascii_strcasecmp(token[i], "normal")) {
      layout |= NormalLayout;
    }
    else if (!(layout & NormalLayout) && !g_ascii_strcasecmp(token[i], "bottomstack")) {
      layout |= BottomStack;
    }
    else if (!g_ascii_strcasecmp(token[i], "maximized")) {
      layout |= Maximized;
    }
    else {
      layout = NormalLayout;
    }
    i++;
  }
  return layout;
}

/* dwb_test_cookie_allowed(const gchar *)     return:  gboolean{{{*/
gboolean 
dwb_test_cookie_allowed(const gchar *domain) {
  for (GList *l = dwb.fc.cookies_allow; l; l=l->next) {
    if (!strcmp(domain, l->data)) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_save_cookies {{{*/
void 
dwb_save_cookies() {
  SoupCookieJar *jar; 

  jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  for (GSList *l = soup_cookie_jar_all_cookies(dwb.state.cookiejar); l; l=l->next) {
    soup_cookie_jar_add_cookie(jar, l->data);
  }
  g_object_unref(jar);
}/*}}}*/

/* dwb_apply_settings(WebSettings *s) {{{*/
void
dwb_apply_settings(WebSettings *s) {
  if (dwb.state.setting_apply == Global) {
    for (GList *l = dwb.state.views; l; l=l->next)  {
      WebSettings *new =  g_hash_table_lookup((((View*)l->data)->setting), s->id);
      new->arg = s->arg;
      if (s->func) {
        s->func(l, s);
      }
    }
  }
  else {
    s->func(dwb.state.fview, s);
    WebSettings *new =  g_hash_table_lookup((((View*)dwb.state.fview->data)->setting), s->id);
    new->arg = s->arg;
  }

}/*}}}*/

/* dwb_web_settings_get_value(const gchar *id, void *value) {{{*/
Arg *  
dwb_web_settings_get_value(const gchar *id) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, id);
  return &s->arg;
}/*}}}*/

/* dwb_web_settings_set_value(const gchar *, Arg *){{{*/
void 
dwb_web_settings_set_value(const gchar *id, Arg *a) {
  WebSettings *s;
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    s = l->data;
    if  (!strcmp(id, s->id)) {
      s->arg = *a;
      break;
    }
  }
  s->func(dwb.state.fview, s);
}/*}}}*/

/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
void
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case Double:  g_object_set(settings, s->id, s->arg.d, NULL); break;
    case Integer: g_object_set(settings, s->id, s->arg.i, NULL); break;
    case Boolean: g_object_set(settings, s->id, s->arg.b, NULL); break;
    case Char:    g_object_set(settings, s->id, (gchar*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = CURRENT_WEBVIEW();
  switch (s->type) {
    case Double:  g_object_set(web, s->id, s->arg.d, NULL); break;
    case Integer: g_object_set(web, s->id, s->arg.i, NULL); break;
    case Boolean: g_object_set(web, s->id, s->arg.b, NULL); break;
    case Char:    g_object_set(web, s->id, (gchar*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  gchar *buffer = NULL;
  gboolean ret = true;
  gchar *com = NULL;

  if (e->keyval == GDK_Return) {
    com = g_strdup("get_active()");
  }
  else if (DIGIT(e)) {
    dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    gchar *text = g_strdup_printf("hint number: %d", dwb.state.nummod);
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, text, &dwb.color.active_fg, dwb.font.fd_normal);

    com = g_strdup_printf("update_hints(\"%d\")", dwb.state.nummod);
    g_free(text);
    ret = true;
  }
  else if (e->keyval == GDK_Tab) {
    if (e->state & GDK_CONTROL_MASK) {
      com = g_strdup("focus_prev()");
    }
    else {
      com = g_strdup("focus_next()");
    }
    ret = true;
  }
  else {
    com = g_strdup_printf("update_hints(\"%s\")", GET_TEXT());
  }
  buffer = dwb_execute_script(com);
  g_free(com);
  if (buffer && strcmp(buffer, "undefined")) {
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
      dwb_normal_mode(false);
    }
    else if (!strcmp(buffer, "_dwb_input_")) {
      puts(buffer);
      if (dwb.state.mode == SearchFieldMode) {
        gchar *com = g_strdup_printf("submit_searchengine(\"%s\")", HINT_SEARCH_SUBMIT);
        gchar *value = dwb_execute_script(com);
        if (value) {
          dwb.state.form_name = value;
        }
        g_free(com);
      }
      else {
        dwb_insert_mode(NULL);
      }
    }
    else if  (!strcmp(buffer, "_dwb_click_")) {
      dwb.state.scriptlock = 1;
    }
    else  if (!strcmp(buffer, "_dwb_check_")) {
      dwb_normal_mode(true);
    }
    else {
      Arg a = { .p = buffer };
      dwb_load_uri(&a);
      dwb.state.scriptlock = 1;
      dwb_normal_mode(true);
    }
  }
  g_free(buffer);

  return ret;
}/*}}}*/

/* dwb_execute_script {{{*/
gchar * 
dwb_execute_script(const char *com) {
  View *v = dwb.state.fview->data;

  JSValueRef exc, eval_ret;
  size_t length;
  gchar *ret = NULL;

  WebKitWebFrame *frame =  webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  JSContextRef context = webkit_web_frame_get_global_context(frame);
  JSStringRef text = JSStringCreateWithUTF8CString(com);
  eval_ret = JSEvaluateScript(context, text, JSContextGetGlobalObject(context), NULL, 0, &exc);
  JSStringRelease(text);

  if (eval_ret) {
    JSStringRef string = JSValueToStringCopy(context, eval_ret, NULL);
    length = JSStringGetMaximumUTF8CStringSize(string);
    ret = g_new(gchar, length);
    JSStringGetUTF8CString(string, ret, length);
    JSStringRelease(string);
  }
  return ret;
}
/*}}}*/

void
dwb_prepend_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_prepend((*fc), n);
}
/* dwb_prepend_navigation(GList *gl, GList *view) {{{*/
gboolean 
dwb_prepend_navigation(GList *gl, GList **fc) {
  WebKitWebView *w = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri) > 0 && strcmp(uri, SETTINGS) && strcmp(uri, KEY_SETTINGS)) {
    const gchar *title = webkit_web_view_get_title(w);
    dwb_prepend_navigation_with_argument(fc, uri, title);
    return true;
  }
  return false;
  
}/*}}}*/

/* dwb_save_quickmark(const gchar *key) {{{*/
void 
dwb_save_quickmark(const gchar *key) {
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri)) {
    const gchar *title = webkit_web_view_get_title(w);
    for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
      Quickmark *q = l->data;
      if (!strcmp(key, q->key)) {
        dwb_quickmark_free(q);
        dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, l);
        break;
      }
    }
    dwb.fc.quickmarks = g_list_prepend(dwb.fc.quickmarks, dwb_quickmark_new(uri, title, key));
    dwb_normal_mode(false);

    gchar *message = g_strdup_printf("Added quickmark: %s - %s", key, uri);
    dwb_set_normal_message(dwb.state.fview, message, true);

    g_free(message);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
}/*}}}*/

/* dwb_open_quickmark(const gchar *key){{{*/
void 
dwb_open_quickmark(const gchar *key) {
  gchar *message = NULL;
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!strcmp(key, q->key)) {
      Arg a = { .p = q->nav->first };
      dwb_load_uri(&a);

      message = g_strdup_printf("Loading quickmark %s: %s", key, q->nav->first);
      dwb_set_normal_message(dwb.state.fview, message, true);
    }
  }
  if (!message) {
    message = g_strdup_printf("No such quickmark: %s", key);
    dwb_set_error_message(dwb.state.fview, message);
  }
  dwb_normal_mode(false);
  g_free(message);
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
void
dwb_tab_label_set_text(GList *gl, const gchar *text) {
  View *v = gl->data;
  const gchar *uri = text ? text : webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
  gchar *escaped = g_markup_printf_escaped("%d : %s", g_list_position(dwb.state.views, gl), uri ? uri : "about:blank");
  gtk_label_set_text(GTK_LABEL(v->tablabel), escaped);
  g_free(escaped);
}/*}}}*/

/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl) {
  View *v = gl->data;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  const gchar *title = webkit_web_view_get_title(w);
  const gchar *uri = webkit_web_view_get_uri(w);

  gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title ? title : uri);
  dwb_tab_label_set_text(gl, title);

  dwb_update_status_text(gl);
}/*}}}*/

/* dwb_update_tab_label {{{*/
void
dwb_update_tab_label() {
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const gchar *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
}/*}}}*/

/* dwb_grab_focus(GList *gl) {{{*/
void 
dwb_grab_focus(GList *gl) {
  View *v = gl->data;

  if (dwb.gui.entry) {
    gtk_widget_hide(dwb.gui.entry);
  }
  dwb.state.fview = gl;
  dwb.gui.entry = v->entry;
  gtk_widget_show(v->entry);
  dwb_set_active_style(gl);
  gtk_widget_grab_focus(v->scroll);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(Arg *arg) {
  gchar *uri = dwb_get_resolved_uri(arg->p);

  if (dwb.state.last_cookie) {
    soup_cookie_free(dwb.state.last_cookie); 
    dwb.state.last_cookie = NULL;
  }
  if (dwb.state.nv == OpenNewView) {
    dwb_add_view(NULL);
  }
  View *fview = dwb.state.fview->data;
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(fview->web), uri);
  g_free(uri);

}/*}}}*/

/* dwb_update_layout() {{{*/
void
dwb_update_layout() {
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  View *v;

  if (dwb.state.layout & Maximized) {
    return; 
  }
  if (dwb.state.views->next) {
    if (!visible) {
      gtk_widget_show_all(dwb.gui.right);
      gtk_widget_hide(((View*)dwb.state.views->next->data)->entry);
    }
    v = dwb.state.views->next->data;
    if (dwb.misc.factor != 1.0) {
      webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), dwb.misc.factor);
    }
    Arg *a = dwb_web_settings_get_value("full-content-zoom");
    webkit_web_view_set_full_content_zoom(WEBKIT_WEB_VIEW(v->web), a->b);
  }
  else if (visible) {
    gtk_widget_hide(dwb.gui.right);
  }
  v = dwb.state.views->data;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  dwb_update_tab_label();
  dwb_resize(dwb.state.size);
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = true;
  gchar *pre = "buffer: ";
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  gint keyval = e->keyval;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (keyval == GDK_Tab) {
    dwb_focus_next(NULL);
    return true;
  }
  // don't show backspace in the buffer
  if (keyval == GDK_BackSpace ) {
    if (dwb.state.buffer && dwb.state.buffer->str ) {
      if (dwb.state.buffer->len > strlen(pre)) {
        g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
        dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
      }
      return false;
    }
    else  
      return true;
  }
  gchar *key = dwb_keyval_to_char(keyval);
  if (!key) {
    return false;
  }

  if (e->is_modifier) {
    return false;
  }

  if (!old) {
    dwb.state.buffer = g_string_new(pre);
    old = dwb.state.buffer->str;
  }
  // nummod 
  if (DIGIT(e)) {
    if (isdigit(old[strlen(old)-1])) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_0;
    }
  }
  // TODO tab and space hardcoded
  if (keyval == GDK_space) {
    dwb_push_master(NULL);
    return true;
  }
  g_string_append(dwb.state.buffer, key);
  if (isprint(key[0])) {
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
  }

  const gchar *buf = dwb.state.buffer->str;
  gint length = dwb.state.buffer->len;
  gint longest = 0;
  KeyMap *tmp = NULL;

  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    gsize l = strlen(km->key);
    if (!km->key || !l) {
      continue;
    }
    if (!strcmp(&buf[length - l], km->key) && !(CLEAN_STATE(e) ^ km->mod)) {
      if  (!longest || l > longest) {
        longest = l;
        tmp = km;
      }
      ret = true;
    }
  }
  if (tmp) {
    dwb_simple_command(tmp);
  }
  g_free(key);
  return ret;

}/*}}}*/

/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode(gboolean clean) {
  View *v = dwb.state.fview->data;

    dwb_execute_script("clear()");
  if (dwb.state.mode  == InsertMode) {
    dwb_view_modify_style(dwb.state.fview, &dwb.color.active_fg, &dwb.color.active_bg, NULL, NULL, NULL, 0);
  }
  else if (dwb.state.mode == CompletionMode) {
    dwb_clean_completion();
  }

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  gtk_widget_grab_focus(v->scroll);

  //gtk_widget_hide(dwb.gui.entry);

  if (clean) {
    CLEAR_COMMAND_TEXT(dwb.state.fview);
  }

  webkit_web_view_unmark_text_matches(CURRENT_WEBVIEW());

  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
  }
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb_clean_vars();
}/*}}}*/

/* dwb_get_resolved_uri(const gchar *uri) {{{*/
gchar * 
dwb_get_resolved_uri(const gchar *uri) {
    char *tmp = NULL;
    // check if uri is a file
    if ( g_file_test(uri, G_FILE_TEST_IS_REGULAR) ) {
        tmp = g_str_has_prefix(uri, "file://") 
            ? g_strdup(uri) 
            : g_strdup_printf("file://%s", uri);
    }
    else if ( !(tmp = dwb_get_search_engine(uri)) || strstr(uri, "localhost:")) {
        tmp = g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")
        ? g_strdup(uri)
        : g_strdup_printf("http://%s", uri);
    }
    return tmp;
}
/*}}}*/

/* dwb_search {{{*/
gboolean
dwb_search(Arg *arg) {
  gboolean forward = dwb.state.forward_search;
  if (arg && !arg->b) {
    forward = !dwb.state.forward_search;
  }
  View *v = CURRENT_VIEW();
  WebKitWebView *web = CURRENT_WEBVIEW();
  const gchar *text = GET_TEXT();
  if (strlen(text) > 0) {
    g_free(v->status->search_string);
    v->status->search_string =  g_strdup(GET_TEXT());
  }
  if (!v->status->search_string) {
    return false;
  }
  webkit_web_view_unmark_text_matches(web);
  webkit_web_view_search_text(web, v->status->search_string, false, forward, true);
  if ( webkit_web_view_mark_text_matches(web, v->status->search_string, false, 0) ) {
    webkit_web_view_set_highlight_text_matches(web, true);
  }
  else {
    dwb_set_error_message(dwb.state.fview, "No matches");
  }
  return true;
}/*}}}*/
/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
void 
dwb_clean_vars() {
  dwb.state.mode = NormalMode;
  dwb.state.buffer = NULL;
  dwb.state.nummod = 0;
  dwb.state.nv = 0;
  dwb.state.scriptlock = 0;
}/*}}}*/

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_free(m);
  }
  g_slist_free(dwb.keymap);
  return true;
}/*}}}*/

/* dwb_save_navigation_fc{{{*/
void 
dwb_save_navigation_fc(GList *fc, const gchar *filename, gint length) {
  GString *string = g_string_new(NULL);
  gint i=0;
  for (GList *l = fc; l && (length<0 || i<length) ; l=l->next, i++)  {
    Navigation *n = l->data;
    g_string_append_printf(string, "%s %s\n", n->first, n->second);
    g_free(n);
  }
  dwb_set_file_content(filename, string->str);
  g_string_free(string, true);
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files() {
  gsize l;
  GError *error = NULL;
  gchar *content;
  GKeyFile *keyfile;

  dwb_save_navigation_fc(dwb.fc.bookmarks, dwb.files.bookmarks, -1);
  dwb_save_navigation_fc(dwb.fc.history, dwb.files.history, dwb.misc.history_length);
  dwb_save_navigation_fc(dwb.fc.searchengines, dwb.files.searchengines, -1);


  // quickmarks
  GString *quickmarks = g_string_new(NULL); 
  for (GList *l = dwb.fc.quickmarks; l; l=l->next)  {
    Quickmark *q = l->data;
    Navigation *n = q->nav;
    g_string_append_printf(quickmarks, "%s %s %s\n", q->key, n->first, n->second);
    dwb_quickmark_free(q);
  }
  dwb_set_file_content(dwb.files.quickmarks, quickmarks->str);
  g_string_free(quickmarks, true);

  // cookie allow
  GString *cookies_allow = g_string_new(NULL);
  for (GList *l = dwb.fc.cookies_allow; l; l=l->next)  {
    g_string_append_printf(cookies_allow, "%s\n", (gchar*)l->data);
    g_free(l->data);
  }
  dwb_set_file_content(dwb.files.cookies_allow, cookies_allow->str);
  g_string_free(cookies_allow, true);
  // save keys

  keyfile = g_key_file_new();
  error = NULL;
  
  if (!g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    error = NULL;
  }
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    gchar *sc = g_strdup_printf("%s %s", dwb_modmask_to_string(map->mod), map->key ? map->key : "");
    g_key_file_set_value(keyfile, dwb.misc.profile, map->map->id, sc);

    g_free(sc);
  }
  if ( (content = g_key_file_to_data(keyfile, &l, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, l, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
  }
  g_key_file_free(keyfile);

  // save settings
  error = NULL;
  keyfile = g_key_file_new();

  if (!g_key_file_load_from_file(keyfile, dwb.files.settings, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    error = NULL;
  }
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    gchar *value = dwb_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->id, value ? value : "" );
    g_free(value);
  }
  if ( (content = g_key_file_to_data(keyfile, &l, &error)) ) {
    g_file_set_contents(dwb.files.settings, content, l, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save settingsfile: %s\n", error->message);
  }
  g_key_file_free(keyfile);

  return true;
}
/* }}} */

/* dwb_end() {{{*/
gboolean
dwb_end() {
  if (dwb_save_files()) {
    if (dwb_clean_up()) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_exit() {{{ */
void 
dwb_exit() {
  dwb_end();
  gtk_main_quit();
} /*}}}*/
/*}}}*/

/* KEYS {{{*/

/* dwb_mod_mask_to_string(guint modmask)      return gchar*{{{*/
gchar *
dwb_modmask_to_string(guint modmask) {
  gchar *mod[7];
  int i=0;
  for (; i<7 && modmask; i++) {
    if (modmask & GDK_CONTROL_MASK) {
      mod[i] = "Control";
      modmask ^= GDK_CONTROL_MASK;
    }
    else if (modmask & GDK_MOD1_MASK) {
      mod[i] = "Mod1";
      modmask ^= GDK_MOD1_MASK;
    }
    else if (modmask & GDK_MOD2_MASK) {
      mod[i] = "Mod2";
      modmask ^= GDK_MOD2_MASK;
    }
    else if (modmask & GDK_MOD3_MASK) {
      mod[i] = "Mod3";
      modmask ^= GDK_MOD3_MASK;
    }
    else if (modmask & GDK_MOD4_MASK) {
      mod[i] = "Mod4";
      modmask ^= GDK_MOD4_MASK;
    }
    else if (modmask & GDK_MOD5_MASK) {
      mod[i] = "Mod5";
      modmask ^= GDK_MOD5_MASK;
    }
  }
  mod[i] = NULL; 
  gchar *line = g_strjoinv(" ", mod);
  return line;
}/*}}}*/

/* dwb_strv_to_key(gchar **string, gsize length)      return: Key{{{*/
Key 
dwb_strv_to_key(gchar **string, gsize length) {
  Key key = { .mod = 0, .str = NULL };
  GString *buffer = g_string_new(NULL);

  for (int i=0; i<length; i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod2")) {
      key.mod |= GDK_MOD2_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod3")) {
      key.mod |= GDK_MOD3_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod5")) {
      key.mod |= GDK_MOD5_MASK;
    }
    else {
      g_string_append(buffer, string[i]);
    }
  }
  key.str = g_strdup(buffer->str);
  //printf("%d %d %s\n", GDK_CONTROL_MASK, key.mod, buffer->str);
  g_string_free(buffer, true);
  return key;
}/*}}}*/

/* dwb_generate_keyfile {{{*/
void 
dwb_generate_keyfile() {
  gchar *path = g_strconcat(g_get_user_config_dir(), "/", dwb.misc.name, "/",  NULL);
  dwb.files.keys = g_strconcat(path, "keys", NULL);
  GKeyFile *keyfile = g_key_file_new();
  gsize l;
  GError *error = NULL;
  
  for (gint i=0; i<LENGTH(KEYS); i++) {
    KeyValue k = KEYS[i];
    gchar *value = k.key.mod ? g_strdup_printf("%s %s", dwb_modmask_to_string(k.key.mod), k.key.str) : g_strdup(k.key.str);
    g_key_file_set_value(keyfile, dwb.misc.profile, k.id, value);
    g_free(value);
  }
  gchar *content;
  if ( (content = g_key_file_to_data(keyfile, &l, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, l, &error);
  }
  if (error) {
    fprintf(stderr, "Couldn't create keyfile: %s\n", error->message);
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "Keyfile created.\n");
}/*}}}*/

/* dwb_keymap_delete(GSList *, KeyValue )     return: GSList * {{{*/
GSList * 
dwb_keymap_delete(GSList *gl, KeyValue key) {
  for (GSList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(km->map->id, key.id)) {
      gl = g_slist_delete_link(gl, l);
      break;
    }
  }
  gl = g_slist_sort(gl, (GCompareFunc)dwb_keymap_sort_text);
  return gl;
}/*}}}*/

/* dwb_keymap_add(GSList *, KeyValue)     return: GSList* {{{*/
GSList *
dwb_keymap_add(GSList *gl, KeyValue key) {
  gl = dwb_keymap_delete(gl, key);
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!strcmp(FMAP[i].id, key.id)) {
      KeyMap *keymap = malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : "";
      keymap->mod = key.key.mod;
      fmap->id = key.id;
      keymap->map = fmap;
      gl = g_slist_prepend(gl, keymap);
      break;
    }
  }
  return gl;
}/*}}}*/
/*}}}*/

/* INIT {{{*/

/* dwb_read_config()    return: GSList *{{{*/
GSList *
dwb_read_key_config() {
  GKeyFile *keyfile = g_key_file_new();
  gsize numkeys;
  GError *error = NULL;
  gchar **keys;
  GSList *gl;

  if (g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    if (! g_key_file_has_group(keyfile, dwb.misc.profile) ) {
      return NULL;
    }
    if ( (keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error)) ) {
      for  (int i=0; i<numkeys; i++) {
        gchar *string = g_key_file_get_value(keyfile, dwb.misc.profile, keys[i], NULL);
        gchar **stringlist = g_strsplit(string, " ", -1);
        Key key = dwb_strv_to_key(stringlist, g_strv_length(stringlist));
        KeyValue kv;
        kv.id = keys[i];
        kv.key = key;
        gl = dwb_keymap_add(gl, kv);
        g_strfreev(stringlist);
      }
    }
  }
  if (error) {
    fprintf(stderr, "Couldn't read config: %s\n", error->message);
  }
  return gl;
}/*}}}*/

/* dwb_setup_cookies() {{{*/
void
dwb_init_cookies() {
    SoupCookieJar *jar;

    dwb.state.cookiejar = soup_cookie_jar_new();
    soup_session_add_feature(dwb.misc.soupsession, SOUP_SESSION_FEATURE(dwb.state.cookiejar));
    jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
    for (GSList *c = soup_cookie_jar_all_cookies(jar); c; c = c->next) {
        soup_cookie_jar_add_cookie(dwb.state.cookiejar, c->data);
    }
    g_signal_connect(dwb.state.cookiejar, "changed", G_CALLBACK(dwb_cookie_changed_cb), NULL);
    g_object_unref(jar);
}/*}}}*/

/* dwb_read_settings() {{{*/
gboolean
dwb_read_settings() {
  GError *error = NULL;
  gsize length, numkeys;
  gchar  **keys;
  gchar  *content;
  GKeyFile  *keyfile = g_key_file_new();
  Arg *arg;
  setlocale(LC_NUMERIC, "C");

  if (   g_file_get_contents(dwb.files.settings, &content, &length, &error) && g_key_file_load_from_data(keyfile, content, length, G_KEY_FILE_KEEP_COMMENTS, &error) ) {
    if (! g_key_file_has_group(keyfile, dwb.misc.profile) ) {
      return false;
    }
    if  ( (keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error)) )  {

      for (int i=0; i<numkeys; i++) {
        gchar *value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
        for (int j=0; j<LENGTH(DWB_SETTINGS); j++) {
          if (!strcmp(keys[i], DWB_SETTINGS[j].id)) {
            WebSettings *s = malloc(sizeof(WebSettings));
            *s = DWB_SETTINGS[j];
            if ( (arg = dwb_char_to_arg(value, s->type)) ) {
              s->arg = *arg;
            }
            //ret = g_slist_append(ret, s);
            gchar *key = g_strdup(s->id);
            g_hash_table_insert(dwb.settings, key, s);
          }
        }
        g_free(value);
      }
    }
  }
  if (error) {
    fprintf(stderr, "Couldn't read config: %s\n", error->message);
    return false;
  }
  return true;
}/*}}}*/

/* dwb_init_key_map() {{{*/
void 
dwb_init_key_map() {
  dwb.keymap = NULL;
  if (!g_file_test(dwb.files.keys, G_FILE_TEST_IS_REGULAR) 
      || !(dwb.keymap = dwb_read_key_config()) ) {
    for (int j=0; j<LENGTH(KEYS); j++) {
      dwb.keymap = dwb_keymap_add(dwb.keymap, KEYS[j]);
    }
  }
  dwb.keymap = g_slist_sort(dwb.keymap, (GCompareFunc)dwb_keymap_sort_text);
}/*}}}*/

/* dwb_init_settings() {{{*/
void
dwb_init_settings() {
  dwb.settings = g_hash_table_new(g_str_hash, g_str_equal);
  dwb.state.web_settings = webkit_web_settings_new();
  if (! g_file_test(dwb.files.settings, G_FILE_TEST_IS_REGULAR) || ! (dwb_read_settings()) ) {
    for (int i=0; i<LENGTH(DWB_SETTINGS); i++) {
      WebSettings *s = &DWB_SETTINGS[i];
      gchar *key = g_strdup(s->id);
      g_hash_table_insert(dwb.settings, key, s);
    }
  }
  for (GList *l =  g_hash_table_get_values(dwb.settings); l; l = l->next) {
    WebSettings *s = l->data;
    if (s->builtin) {
      s->func(NULL, s);
    }
  }
}/*}}}*/

/* dwb_init_scripts{{{*/
void 
dwb_init_scripts() {
  GString *buffer = g_string_new(NULL);

  setlocale(LC_NUMERIC, "C");
  g_string_append_printf(buffer, "hint_vertical_off = %d;\n",     GET_INT("hint_vertical_off"));
  g_string_append_printf(buffer, "hint_horizontal_off = %d;\n",   GET_INT("hint_horizontal_off"));
  g_string_append_printf(buffer, "hint_letter_seq = '%s';\n",       GET_CHAR("hint_letter_seq"));
  g_string_append_printf(buffer, "hint_style = '%s';\n",            GET_CHAR("hint_style"));
  g_string_append_printf(buffer, "hint_font_size = '%s';\n",        GET_CHAR("hint_font_size"));
  g_string_append_printf(buffer, "hint_font_weight = '%s';\n",      GET_CHAR("hint_font_weight"));
  g_string_append_printf(buffer, "hint_font_family = '%s';\n",      GET_CHAR("hint_font_family"));
  g_string_append_printf(buffer, "hint_style = '%s';\n",            GET_CHAR("hint_style"));
  g_string_append_printf(buffer, "hint_fg_color = '%s';\n",         GET_CHAR("hint_fg_color"));
  g_string_append_printf(buffer, "hint_bg_color = '%s';\n",         GET_CHAR("hint_bg_color"));
  g_string_append_printf(buffer, "hint_active_color = '%s';\n",     GET_CHAR("hint_active_color"));
  g_string_append_printf(buffer, "hint_normal_color = '%s';\n",     GET_CHAR("hint_normal_color"));
  g_string_append_printf(buffer, "hint_border = '%s';\n",           GET_CHAR("hint_border"));
  g_string_append_printf(buffer, "overlay_border = '%s';\n",        GET_CHAR("overlay_border"));
  g_string_append_printf(buffer, "hint_opacity = %f;\n",          GET_DOUBLE("hint_opacity"));
  g_string_append_printf(buffer, "overlay_opacity = %f;\n",       GET_DOUBLE("overlay_opacity"));

  gchar *dircontent = dwb_get_directory_content(dwb.files.scriptdir);
  g_string_append(buffer, dircontent);
  g_free(dircontent);
  dwb.misc.scripts = buffer->str;
  g_string_free(buffer, false);
  puts(dwb.misc.scripts);
  //dwb.misc.scripts = dwb_get_directory_content(dwb.files.scriptdir);
}/*}}}*/

/* dwb_init_style() {{{*/
void
dwb_init_style() {
  // Colors 
  // Statusbar
  gdk_color_parse(GET_CHAR("active_fg_color"), &dwb.color.active_fg);
  gdk_color_parse(GET_CHAR("active_bg_color"), &dwb.color.active_bg);
  gdk_color_parse(GET_CHAR("normal_fg_color"), &dwb.color.normal_fg);
  gdk_color_parse(GET_CHAR("normal_bg_color"), &dwb.color.normal_bg);

  // Tabs
  gdk_color_parse(GET_CHAR("tab_active_fg_color"), &dwb.color.tab_active_fg);
  gdk_color_parse(GET_CHAR("tab_active_bg_color"), &dwb.color.tab_active_bg);
  gdk_color_parse(GET_CHAR("tab_normal_fg_color"), &dwb.color.tab_normal_fg);
  gdk_color_parse(GET_CHAR("tab_normal_bg_color"), &dwb.color.tab_normal_bg);

  //InsertMode 
  gdk_color_parse(GET_CHAR("insert_fg_color"), &dwb.color.insert_fg);
  gdk_color_parse(GET_CHAR("insert_bg_color"), &dwb.color.insert_bg);

  gdk_color_parse(GET_CHAR("active_comp_bg_color"), &dwb.color.active_c_bg);
  gdk_color_parse(GET_CHAR("active_comp_fg_color"), &dwb.color.active_c_fg);
  gdk_color_parse(GET_CHAR("normal_comp_bg_color"), &dwb.color.normal_c_bg);
  gdk_color_parse(GET_CHAR("normal_comp_fg_color"), &dwb.color.normal_c_fg);

  gdk_color_parse(GET_CHAR("error_color"), &dwb.color.error);

  dwb.color.settings_fg_color = GET_CHAR("settings_fg_color");
  dwb.color.settings_bg_color = GET_CHAR("settings_bg_color");

  // Fonts
  gint active_font_size = GET_INT("active_font_size");
  gint normal_font_size = GET_INT("normal_font_size");
  gchar *font = GET_CHAR("font");

  dwb.font.fd_normal = pango_font_description_from_string(font);
  dwb.font.fd_bold = pango_font_description_from_string(font);
  dwb.font.fd_oblique = pango_font_description_from_string(font);

  pango_font_description_set_absolute_size(dwb.font.fd_normal, active_font_size * PANGO_SCALE);
  pango_font_description_set_absolute_size(dwb.font.fd_bold, active_font_size * PANGO_SCALE);
  pango_font_description_set_absolute_size(dwb.font.fd_oblique, active_font_size * PANGO_SCALE);

  pango_font_description_set_weight(dwb.font.fd_normal, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_weight(dwb.font.fd_bold, PANGO_WEIGHT_BOLD);
  pango_font_description_set_style(dwb.font.fd_oblique, PANGO_STYLE_OBLIQUE);
  
  // Fontsizes
  dwb.font.active_size = active_font_size;
  dwb.font.normal_size = normal_font_size;
  dwb.misc.settings_border = GET_CHAR("settings_border");
} /*}}}*/

/* dwb_init_gui() {{{*/
void 
dwb_init_gui() {
  // Window
  dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (embed) {
    dwb.gui.window = gtk_plug_new(embed);
  } 
  else {
    dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(dwb.gui.window), dwb.misc.name, dwb.misc.name);
  }
  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), GET_INT("default_width"), GET_INT("default_height"));
  g_signal_connect(dwb.gui.window, "delete-event", G_CALLBACK(dwb_exit), NULL);
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(dwb_key_press_cb), NULL);
  g_signal_connect(dwb.gui.window, "key-release-event", G_CALLBACK(dwb_key_release_cb), NULL);

  // Main
  dwb.gui.vbox = gtk_vbox_new(false, 1);
  dwb.gui.topbox = gtk_hbox_new(true, 1);
  dwb.gui.paned = gtk_hpaned_new();
  dwb.gui.left = gtk_vbox_new(true, 0);
  dwb.gui.right = gtk_vbox_new(true, 1);

  // Paned
  GtkWidget *paned_event = gtk_event_box_new(); 
  gtk_widget_modify_bg(paned_event, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_PRELIGHT, &dwb.color.active_bg);
  gtk_container_add(GTK_CONTAINER(paned_event), dwb.gui.paned);

  // Pack
  gtk_paned_pack1(GTK_PANED(dwb.gui.paned), dwb.gui.left, true, true);
  gtk_paned_pack2(GTK_PANED(dwb.gui.paned), dwb.gui.right, true, true);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), paned_event, true, true, 0);
  //gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.entry, false, false, 0);


  dwb_add_view(NULL);
  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  //gtk_widget_show(dwb.gui.entry);
  gtk_widget_show(dwb.gui.left);
  gtk_widget_show(dwb.gui.paned);
  gtk_widget_show(paned_event);
  gtk_widget_show_all(dwb.gui.topbox);

  gtk_widget_show(dwb.gui.vbox);
  gtk_widget_show(dwb.gui.window);
} /*}}}*/

/* dwb_init_file_content {{{*/
GList *
dwb_init_file_content(GList *gl, const gchar *filename, void *(*func)(const gchar *)) {
  gchar *content = dwb_get_file_content(filename);
  if (content) {
    gchar **token = g_strsplit(content, "\n", 0);
    gint length = MAX(g_strv_length(token) - 1, 0);
    for (int i=0;  i < length; i++) {
      gl = g_list_append(gl, func(token[i]));
    }
    g_free(content);
    g_strfreev(token);
  }
  return gl;
}/*}}}*/

/* dwb_init_files() {{{*/
void
dwb_init_files() {
  gchar *path = g_strconcat(g_get_user_config_dir(), "/", dwb.misc.name, "/",  NULL);
  dwb.files.bookmarks     = g_strconcat(path, "bookmarks", NULL);
  dwb.files.history       = g_strconcat(path, "history", NULL);
  dwb.files.stylesheet    = g_strconcat(path, "stylesheet", NULL);
  dwb.files.quickmarks    = g_strconcat(path, "quickmarks", NULL);
  dwb.files.session       = g_strconcat(path, "session", NULL);
  dwb.files.searchengines = g_strconcat(path, "searchengines", NULL);
  dwb.files.stylesheet    = g_strconcat(path, "stylesheet", NULL);
  dwb.files.keys          = g_strconcat(path, "keys", NULL);
  dwb.files.scriptdir     = g_strconcat(path, "/scripts", NULL);
  dwb.files.settings      = g_strconcat(path, "settings", NULL);
  dwb.files.cookies       = g_strconcat(path, "cookies", NULL);
  dwb.files.cookies_allow = g_strconcat(path, "cookies.allow", NULL);

  //dwb.misc.scripts = dwb_get_directory_content(dwb.files.scriptdir);

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (void*)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (void*)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (void*)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (void*)dwb_navigation_new_from_line); 
  dwb.misc.default_search = ((Navigation*)g_list_last(dwb.fc.searchengines)->data)->second;
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files.cookies_allow, (void*)dwb_return);
}/*}}}*/

/* signals{{{*/
void
dwb_handle_signal(gint s) {
  if (s == SIGALRM || s == SIGFPE || s == SIGILL || s == SIGINT || s == SIGQUIT || s == SIGTERM) {
    dwb_end();
    exit(EXIT_SUCCESS);
  }
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, try to clean up.\n");
    if (dwb_end()) {
      fprintf(stderr, "Success.\n");
    }
    exit(EXIT_FAILURE);
  }
}
void 
dwb_init_proxy() {
  const gchar *proxy;
  gchar *newproxy;
  if ( (proxy =  g_getenv("http_proxy")) ) {
    newproxy = g_strrstr(proxy, "http://") ? g_strdup(proxy) : g_strdup_printf("http://%s", proxy);
    dwb.misc.proxyuri = soup_uri_new(newproxy);
    g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", dwb.misc.proxyuri, NULL); 
    g_free(newproxy);
    WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
    s->arg.b = true;
  }
}

void 
dwb_init_signals() {
  for (int i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

/* dwb_init() {{{*/
void dwb_init() {

  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;
  dwb.state.last_cookie = NULL;
  dwb.misc.max_c_items = MAX_COMPLETIONS;
  
  dwb.misc.soupsession = webkit_get_default_session();

  dwb_init_signals();
  dwb_init_files();
  dwb_init_key_map();
  dwb_init_settings();
  dwb_init_style();
  dwb_init_scripts();
  dwb_init_proxy();
  dwb_init_gui();
  dwb_init_cookies();

  dwb.misc.message_delay = GET_INT("message_delay");
  dwb.misc.history_length = GET_INT("history_length");
  dwb.state.size = GET_INT("size");
  dwb.misc.factor = GET_DOUBLE("factor");
  dwb.state.layout = dwb_layout_from_char(GET_CHAR("layout"));
  if (dwb.state.layout & BottomStack) {
    Arg a = { .n = dwb.state.layout };
    dwb_set_orientation(&a);
  }
} /*}}}*/ /*}}}*/

void 
dwb_test_function() {
  const gchar *path = g_getenv("PATH");
  gchar **dirs = g_strsplit(path, ":", -1);
  GDir *current;
  const gchar *filename;
  GList *bins = NULL;
  for (gint i=0; i<g_strv_length(dirs); i++) {
    if ( (current = g_dir_open(dirs[i], 0, NULL)) ) {
      while ( (filename = g_dir_read_name(current)) ) {
        Navigation *n = dwb_navigation_new(filename, NULL);
        bins = g_list_append(bins, n);
      }
    }
  }
}

int main(gint argc, gchar **argv) {
  dwb.misc.name = NAME;
  dwb.misc.profile = "default";

  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  if (argc > 1) {
    for (int i=0; i<argc; i++) {
      if (!strcmp(argv[i], "--generate-keyfile")) {
        dwb_generate_keyfile();
        exit(EXIT_SUCCESS);
      }
      if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--profile")) {
        if (argv[i+1]) {
          dwb.misc.profile = argv[i+1];
        }
      }
    }
  }
  gtk_init(&argc, &argv);
  dwb_test_function();;
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
