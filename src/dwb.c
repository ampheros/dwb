#define _POSIX_SOURCE
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libsoup/soup.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h> 
#include "dwb.h"
#include "completion.h"
#include "commands.h"
#include "view.h"
#include "util.h"
#include "config.h"
#define NAME "dwb";

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

#define CLIPBOARD_PTR 
#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

/* DECLARATIONS {{{*/

void dwb_clean_buffer(GList *);

void dwb_reload_scripts(GList *,  WebSettings *);
void dwb_reload_colors(GList *,  WebSettings *);
void dwb_reload_layout(GList *,  WebSettings *);
gboolean dwb_test_cookie_allowed(const gchar *);
void dwb_save_cookies(void);
void dwb_web_settings_set_value(const gchar *id, Arg *a);

gboolean dwb_eval_key(GdkEventKey *);

void dwb_webkit_setting(GList *, WebSettings *);
void dwb_webview_property(GList *, WebSettings *);
void dwb_resize(gdouble);

void dwb_tab_label_set_text(GList *, const gchar *);

void dwb_save_quickmark(const gchar *);
void dwb_open_quickmark(const gchar *);

GList * dwb_keymap_delete(GList *, KeyValue );

void dwb_set_active_style(GList *gl);
void dwb_view_modify_style(GList *, GdkColor *, GdkColor *, GdkColor *, GdkColor *, PangoFontDescription *, gint);

void dwb_update_tab_label(void);
gchar * dwb_get_resolved_uri(const gchar *);


void dwb_set_websetting(GList *, WebSettings *);

void dwb_init_key_map(void);
void dwb_init_settings(void);
void dwb_init_style(void);
void dwb_init_scripts(void);
void dwb_init_gui(void);

void dwb_clean_vars(void);
/*}}}*/

static GdkNativeWindow embed = 0;
static gint signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static gint MAX_COMPLETIONS = 11;


/* FUNCTION_MAP{{{*/
#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"
static FunctionMap FMAP [] = {
  { { "add_view",              "Add a new view",                    },  (void*)dwb_add_view,           NULL,                              AlwaysSM,     { .p = NULL }, },
  { { "allow_cookie",          "Cookie allowed",                    },  (void*)dwb_allow_cookie,       "No cookie in current context",    PostSM, },
  { { "autoload_images",       "Setting: autoload images",          },  (void*)dwb_toggle_property,    NULL,                              PostSM,    { .p = "auto-load-images" } },
  { { "autoresize_window",     "Setting: autoresize window",        },  (void*)dwb_toggle_property,    NULL,                              PostSM,    { .p = "auto-resize-window" } },
  { { "bookmark",              "Bookmark current page",             },  (void*)dwb_bookmark,           NO_URL,                            PostSM, },
  { { "caret_browsing",        "Setting: caret browsing",           },  (void*)dwb_toggle_property,    NULL,                              PostSM,    { .p = "enable-caret-browsing" } },
  { { "decrease_master",       "Decrease master area",              },  (void*)dwb_resize_master,      "Cannot decrease further",         AlwaysSM,    { .n = 5 } },
  { { "find_backward",         "Find Backward ",                    },  (void*)dwb_find,               NO_URL,                            NeverSM,     { .b = false }, },
  { { "find_forward",          "Find Forward ",                     },  (void*)dwb_find,               NO_URL,                            NeverSM,     { .b = true }, },
  { { "find_next",             "Find next",                         },  (void*)dwb_search,             "No matches",                      AlwaysSM,     { .b = true }, },
  { { "find_previous",         "Find next",                         },  (void*)dwb_search,             "No matches",                      AlwaysSM,     { .b = false }, },
  { { "focus_input",           "Focus input",                       },  (void*)dwb_focus_input,        "No input found in current context",      AlwaysSM, },
  { { "focus_next",            "Focus next view",                   },  (void*)dwb_focus_next,         "No other view",                   AlwaysSM, },
  { { "focus_prev",            "Focus previous view",               },  (void*)dwb_focus_prev,         "No other view",                   AlwaysSM, },
  { { "hint_mode",             "Follow hints ",                     },  (void*)dwb_show_hints,         NO_HINTS,                          NeverSM,    { .n = OpenNormal }, },
  { { "hint_mode_nv",    "Follow hints in a new view ",       },  (void*)dwb_show_hints,         NO_HINTS,                                NeverSM,    { .n = OpenNewView }, },
  { { "history_back",          "Go Back",                           },  (void*)dwb_history_back,       "Beginning of History",            AlwaysSM, },
  { { "history_forward",       "Go Forward",                        },  (void*)dwb_history_forward,    "End of History",                  AlwaysSM, },
  { { "increase_master",       "Increase master area",              },  (void*)dwb_resize_master,      "Cannot increase further",         AlwaysSM,    { .n = -5 } },
  { { "insert_mode",           "Insert Mode",                       },  (void*)dwb_insert_mode,        NULL,                              NeverSM, },
  { { "java_applets",          "Setting: java applets",             },  (void*)dwb_toggle_property,      NULL,                            PostSM,    { .p = "enable-java-applets" } },
  { { "open",                  "Open URL",                          },  (void*)dwb_open,               NULL,                              NeverSM,   { .n = OpenNormal,      .p = NULL } },
  { { "open_new_view",         "Open URL in a new view",            },  (void*)dwb_open,               NULL,                              NeverSM,   { .n = OpenNewView,     .p = NULL } },
  { { "open_quickmark",        "Open quickmark",                    },  (void*)dwb_quickmark,          NO_URL,                            NeverSM,   { .n = QuickmarkOpen, .i=OpenNormal }, },
  { { "open_quickmark_nv",     "Open quickmark in a new view",      },  (void*)dwb_quickmark,          NULL,                              NeverSM,    { .n = QuickmarkOpen, .i=OpenNewView }, },
  { { "plugins",               "Setting: plugins",                  },  (void*)dwb_toggle_property,           NULL,                       PostSM,    { .p = "enable-plugins" } },
  { { "private_browsing",      "Setting: private browsing",         },  (void*)dwb_toggle_property,  NULL,                                PostSM,    { .p = "enable-private-browsing" } },
  { { "proxy",                 "Setting: proxy",                    },  (void*)dwb_toggle_proxy,      NULL,                               PostSM,    { 0 } },
  { { "push_master",           "Push to master area",               },  (void*)dwb_push_master,        "No other view",                   AlwaysSM, },
  { { "reload",                "Reload",                            },  (void*)dwb_reload,             NULL,                              AlwaysSM, },
  { { "remove_view",           "Close view",                        },  (void*)dwb_remove_view,        NULL,                              AlwaysSM, },
  { { "save_quickmark",        "Save a quickmark for this page",    },  (void*)dwb_quickmark,          NO_URL,                            NeverSM,    { .n = QuickmarkSave }, },
  { { "save_search_field",     "Add a new searchengine",       },  (void*)dwb_add_search_field,   "No input in current context",          NeverSM, },
  { { "scripts",               "Setting: scripts",                  },  (void*)dwb_toggle_property,      NULL,                            PostSM,    { .p = "enable-scripts" } },
  { { "scroll_bottom",         "Scroll to  bottom of the page",     },  (void*)dwb_scroll,             NULL,                              AlwaysSM,    { .n = Bottom }, },
  { { "scroll_down",           "Scroll down",                       },  (void*)dwb_scroll,             "Bottom of the page",              AlwaysSM,    { .n = Down, }, },
  { { "scroll_left",           "Scroll left",                       },  (void*)dwb_scroll,             "Left side of the page",           AlwaysSM,    { .n = Left }, },
  { { "scroll_page_down",      "Scroll one page down",              },  (void*)dwb_scroll,             "Bottom of the page",              AlwaysSM,    { .n = PageDown, }, },
  { { "scroll_page_up",        "Scroll one page up",                },  (void*)dwb_scroll,             "Top of the page",                 AlwaysSM,    { .n = PageUp, }, },
  { { "scroll_right",          "Scroll left",                       },  (void*)dwb_scroll,             "Right side of the page",          AlwaysSM,    { .n = Right }, },
  { { "scroll_top",            "Scroll to the top of the page",     },  (void*)dwb_scroll,             NULL,                              AlwaysSM,    { .n = Top }, },
  { { "scroll_up",             "Scroll up",                         },  (void*)dwb_scroll,             "Top of the page",                 AlwaysSM,    { .n = Up, }, },
  { { "set_global_setting",    "Set global property",               },  (void*)dwb_set_setting,        NULL,                              NeverSM,    { .n = Global } },
  { { "set_key",               "Set keybinding",                    },  (void*)dwb_set_key,                NULL,                          NeverSM,    { 0 } },
  { { "set_setting",           "Set property",                      },  (void*)dwb_set_setting,        NULL,                              NeverSM,    { .n = PerView } },
  { { "show_global_settings",  "Show global settings",              },  (void*)dwb_show_settings,      NULL,                              AlwaysSM,    { .n = Global } },
  { { "show_keys",             "Key configuration",                 },  (void*)dwb_show_keys,          NULL,                              AlwaysSM, },
  { { "show_settings",         "Settings",                          },  (void*)dwb_show_settings,      NULL,                              AlwaysSM,    { .n = PerView } },
  { { "spell_checking",        "Setting: spell checking",           },  (void*)dwb_toggle_property,      NULL,                            PostSM,    { .p = "enable-spell-checking" } },
  { { "toggle_bottomstack",    "Toggle bottomstack",                },  (void*)dwb_set_orientation,    NULL,                              AlwaysSM, },
  { { "toggle_encoding",       "Toggle Custom encoding",            },  (void*)dwb_toggle_custom_encoding,    NULL,                       AlwaysSM, },
  { { "toggle_maximized",      "Toggle maximized",                  },  (void*)dwb_toggle_maximized,   NULL,                              AlwaysSM, },
  { { "toggle_shrink_images",  "Setting: autoshrink images",        },  (void*)dwb_toggle_property,    NULL,                              PostSM,    { .p = "auto-shrink-images" } },
  { { "view_source",           "View source",                       },  (void*)dwb_view_source,        NULL,                              AlwaysSM, },
  { { "zoom_in",               "Zoom in",                           },  (void*)dwb_zoom_in,            "Cannot zoom in further",          AlwaysSM, },
  { { "zoom_normal",           "Zoom 100%",                         },  (void*)dwb_set_zoom_level,     NULL,                              AlwaysSM,    { .d = 1.0,   .p = NULL } },
  { { "zoom_out",              "Zoom out",                          },  (void*)dwb_zoom_out,           "Cannot zoom out further",         AlwaysSM, },
  { { "yank",                  "Yank",                              },  (void*)dwb_yank,                NO_URL,                           PostSM,  { .p = GDK_NONE } },
  { { "yank_primary",          "Yank to Primary selection",         },  (void*)dwb_yank,                NO_URL,                           PostSM,  { .p = GDK_SELECTION_PRIMARY } },
  { { "paste",                 "Paste",                             },  (void*)dwb_paste,              "Clipboard is empty",              AlwaysSM,  { .n = OpenNormal, .p = GDK_NONE } },
  { { "paste_primary",         "Paste primary selection",           },  (void*)dwb_paste,              "No primary selection",            AlwaysSM,  { .n = OpenNormal, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nv",              "Paste, new view",                   },  (void*)dwb_paste,              "Clipboard is empty",              AlwaysSM,  { .n = OpenNewView, .p = GDK_NONE } },
  { { "paste_primary_nv",      "Paste primary selection, new view", },  (void*)dwb_paste,              "No primary selection",            AlwaysSM,  { .n = OpenNewView, .p = GDK_SELECTION_PRIMARY } },

  //{ "create_hints",          (void*)dwb_create_hints,       "Hints",                          NULL,                       true,                                             },

};/*}}}*/

/* DWB_SETTINGS {{{*/
static WebSettings DWB_SETTINGS[] = {
  { { "auto-load-images",			                   "Autoload images", },                                         true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "auto-resize-window",			                 "Autoresize images", },                                       true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "auto-shrink-images",			                 "Autoshrink images", },                                       true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "cursive-font-family",			               "Cursive font family", },                                     true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting, },
  { { "default-encoding",			                   "Default encoding", },                                        true, false,  Char,    { .p = "iso-8859-1"      }, (void*) dwb_webkit_setting, },
  { { "default-font-family",			               "Default font family", },                                     true, false,  Char,    { .p = "sans-serif"      }, (void*) dwb_webkit_setting, },
  { { "default-font-size",			                 "Default font size", },                                       true, false,  Integer, { .i = 12                }, (void*) dwb_webkit_setting, },
  { { "default-monospace-font-size",			       "Default monospace font size", },                             true, false,  Integer, { .i = 10                }, (void*) dwb_webkit_setting, },
  { { "enable-caret-browsing",			             "Caret Browsing", },                                          true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-default-context-menu",			       "Enable default context menu", },                             true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-developer-extras",			           "Enable developer extras",    },                              true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-dom-paste",			                   "Enable DOM paste", },                                        true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-file-access-from-file-uris",			 "File access from file uris", },                              true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-html5-database",			             "Enable HTML5-database" },                                    true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-html5-local-storage",			         "Enable HTML5 local storage", },                              true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-java-applet",			                 "Java Applets", },                                            true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-offline-web-application-cache",		 "Offline web application cache", },                           true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-page-cache",			                 "Page cache", },                                              true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-plugins",			                     "Plugins", },                                                 true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-private-browsing",			           "Private Browsing", },                                        true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-scripts",			                     "Script", },                                                  true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-site-specific-quirks",			       "Site specific quirks", },                                    true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-spatial-navigation",			         "Spatial navigation", },                                      true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-spell-checking",			             "Spell checking", },                                          true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "enable-universal-access-from-file-uris",	 "Universal access from file  uris", },                        true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enable-xss-auditor",			                 "XSS auditor", },                                             true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "enforce-96-dpi",			                     "Enforce 96 dpi", },                                          true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "fantasy-font-family",			               "Fantasy font family", },                                     true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting, },
  { { "javascript-can-access-clipboard",			   "Javascript can access clipboard", },                         true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "javascript-can-open-windows-automatically", "Javascript can open windows automatically", },             true, false,  Boolean, { .b = false             }, (void*) dwb_webkit_setting, },
  { { "minimum-font-size",			                 "Minimum font size", },                                       true, false,  Integer, { .i = 5                 }, (void*) dwb_webkit_setting, },
  { { "minimum-logical-font-size",			         "Minimum logical font size", },                               true, false,  Integer, { .i = 5                 }, (void*) dwb_webkit_setting, },
  { { "monospace-font-family",			             "Monospace font family", },                                   true, false,  Char,    { .p = "monospace"       }, (void*) dwb_webkit_setting, },
  { { "print-backgrounds",			                 "Print backgrounds", },                                       true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "resizable-text-areas",			               "Resizable text areas", },                                    true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "sans-serif-font-family",			             "Sans serif font family", },                                  true, false,  Char,    { .p = "sans-serif"      }, (void*) dwb_webkit_setting, },
  { { "serif-font-family",			                 "Serif font family", },                                       true, false,  Char,    { .p = "serif"           }, (void*) dwb_webkit_setting, },
  { { "spell-checking-languages",			           "Spell checking languages", },                                true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting, },
  { { "tab-key-cycles-through-elements",			   "Tab cycles through elements in insert mode", },              true, false,  Boolean, { .b = true              }, (void*) dwb_webkit_setting, },
  { { "user-agent",			                         "User agent", },                                              true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting, },
  { { "user-stylesheet-uri",			               "User stylesheet uri", },                                     true, false,  Char,    { .p = NULL              }, (void*) dwb_webkit_setting, },
  { { "zoom-step",			                         "Zoom Step", },                                               true, false,  Double,  { .d = 0.1               }, (void*) dwb_webkit_setting, },
  { { "custom-encoding",                         "Custom encoding", },                                         false, false, Char,    { .p = "utf-8"           }, (void*) dwb_webview_property, },
  { { "editable",                                "Content editable", },                                        false, false, Boolean, { .b = false             }, (void*) dwb_webview_property, },
  { { "full-content-zoom",                       "Full content zoom", },                                       false, false, Boolean, { .b = false             }, (void*) dwb_webview_property, },
  { { "zoom-level",                              "Zoom level", },                                              false, false, Double,  { .d = 1.0               }, (void*) dwb_webview_property, },
  { { "proxy",                                   "HTTP-proxy", },                                              false, true,  Boolean, { .b = true              }, (void*) dwb_set_websetting, },
  { { "cookie",                                  "All Cookies allowed", },                                     false, true,  Boolean, { .b = false             }, (void*) dwb_set_websetting, },

  { { "active-fg-color",                         "UI: Active view foreground", },                              false, true,  ColorChar, { .p = "#ffffff"         },    (void*) dwb_reload_layout, },
  { { "active-bg-color",                         "UI: Active view background", },                              false, true,  ColorChar, { .p = "#000000"         },    (void*) dwb_reload_layout, },
  { { "normal-fg-color",                         "UI: Inactive view foreground", },                            false, true,  ColorChar, { .p = "#cccccc"         },    (void*) dwb_reload_layout, },
  { { "normal-bg-color",                         "UI: Inactive view background", },                            false, true,  ColorChar, { .p = "#505050"         },    (void*) dwb_reload_layout, },

  { { "tab-active-fg-color",                     "UI: Active view tabforeground", },                           false, true,  ColorChar, { .p = "#ffffff"         },    (void*) dwb_reload_layout, },
  { { "tab-active-bg-color",                     "UI: Active view tabbackground", },                           false, true,  ColorChar, { .p = "#000000"         },    (void*) dwb_reload_layout, },
  { { "tab-normal-fg-color",                     "UI: Inactive view tabforeground", },                         false, true,  ColorChar, { .p = "#cccccc"         },    (void*) dwb_reload_layout, },
  { { "tab-normal-bg-color",                     "UI: Inactive view tabbackground", },                         false, true,  ColorChar, { .p = "#505050"         },    (void*) dwb_reload_layout, },

  { { "active-comp-fg-color",                    "UI: Completion active foreground", },                        false, true,  ColorChar, { .p = "#1793d1"         }, (void*) dwb_reload_colors, },
  { { "active-comp-bg-color",                    "UI: Completion active background", },                        false, true,  ColorChar, { .p = "#000000"         }, (void*) dwb_reload_colors, },
  { { "normal-comp-fg-color",                    "UI: Completion inactive foreground", },                      false, true,  ColorChar, { .p = "#eeeeee"         }, (void*) dwb_reload_colors, },
  { { "normal-comp-bg-color",                    "UI: Completion inactive background", },                      false, true,  ColorChar, { .p = "#151515"         }, (void*) dwb_reload_colors, },

  { { "insert-fg-color",                         "UI: Insertmode foreground", },                               false, true,  ColorChar, { .p = "#ffffff"         }, (void*) dwb_reload_colors, },
  { { "insert-bg-color",                         "UI: Insertmode background", },                               false, true,  ColorChar, { .p = "#00008b"         }, (void*) dwb_reload_colors, },
  { { "error-color",                             "UI: Error color", },                                         false, true,  ColorChar, { .p = "#ff0000"         }, (void*) dwb_reload_colors, },

  { { "settings-fg-color",                       "UI: Settings view foreground", },                            false, true,  ColorChar, { .p = "#ffffff"         }, (void*) dwb_reload_colors, },
  { { "settings-bg-color",                       "UI: Settings view background", },                            false, true,  ColorChar, { .p = "#151515"         }, (void*) dwb_reload_colors, },
  { { "settings-border",                         "UI: Settings view border", },                                false, true,  Char, { .p = "1px dotted black"}, (void*) dwb_reload_colors, },
 
  { { "active-font-size",                        "UI: Active view fontsize", },                                false, true,  Integer, { .i = 12                }, (void*) dwb_reload_layout, },
  { { "normal-font-size",                        "UI: Inactive view fontsize", },                              false, true,  Integer, { .i = 10                }, (void*) dwb_reload_layout, },
  
  { { "font",                                    "UI: Font", },                                                false, true,  Char, { .p = "monospace"          }, (void*) dwb_reload_layout, },
   
  { { "hint-letter-seq",                       "Hints: Letter sequence for letter hints", },             false, true,  Char, { .p = "FDSARTGBVECWXQYIOPMNHZULKJ"  },     (void*) dwb_reload_scripts, },
  { { "hint-style",                              "Hints: Hintstyle (letter or number)", },                  false, true,  Char, { .p = "letter"         },     (void*) dwb_reload_scripts, },
  { { "hint-font-size",                          "Hints: Font size", },                                        false, true,  Char, { .p = "12px"         },     (void*) dwb_reload_scripts, },
  { { "hint-font-weight",                        "Hints: Font weight", },                                      false, true,  Char, { .p = "normal"         },     (void*) dwb_reload_scripts, },
  { { "hint-font-family",                        "Hints: Font family", },                                      false, true,  Char, { .p = "monospace"      },     (void*) dwb_reload_scripts, },
  { { "hint-fg-color",                           "Hints: Foreground color", },                                false, true,  ColorChar, { .p = "#ffffff"         },    (void*) dwb_reload_scripts, },
  { { "hint-bg-color",                           "Hints: Background color", },                                false, true,  ColorChar, { .p = "#000088"         },    (void*) dwb_reload_scripts, },
  { { "hint-active-color",                       "Hints: Active link color", },                                  false, true,  ColorChar, { .p = "#00ff00"         }, (void*) dwb_reload_scripts, },
  { { "hint-normal-color",                       "Hints: Inactive link color", },                                false, true,  ColorChar, { .p = "#ffff99"         }, (void*) dwb_reload_scripts, },
  { { "hint-border",                             "Hints: Hint Border", },                                  false, true,  Char, { .p = "2px dashed #000000"    }, (void*) dwb_reload_scripts, },
  { { "hint-opacity",                            "Hints: Hint Opacity", },                                  false, true,  Double, { .d = 0.75         },         (void*) dwb_reload_scripts, },
  { { "auto-completion",                         "Show possible keystrokes", },                                  false, true,  Boolean, { .b = true         },         (void*) dwb_set_autcompletion, },
    
  { { "default-width",                           "Default width", },                                           false, true,  Integer, { .i = 1280          }, NULL, },
  { { "default-height",                          "Default height", },                                           false, true,  Integer, { .i = 1024          }, NULL, },
  { { "message-delay",                           "Message delay", },                                           false, true,  Integer, { .i = 2          }, NULL, },
  { { "history-length",                          "History length", },                                          false, true,  Integer, { .i = 500          }, NULL, },
  { { "size",                                    "UI: Default tiling area size (in %)", },                     false, true,  Integer, { .i = 30          }, NULL, },
  { { "factor",                                  "UI: Default Zoom factor of tiling area", },                  false, true,  Double, { .d = 0.3          }, NULL, },
  { { "layout",                                  "UI: Default layout (Normal, Bottomstack, Maximized)", },     false, true,  Char, { .p = "Normal Maximized" },  NULL, },
};/*}}}*/


/* UTIL {{{*/
/* }}} */

/* CALLBACKS {{{*/


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
  else if (dwb.state.mode & AutoComplete) {
    if (e->keyval == GDK_Tab) {
      dwb_autocomplete(NULL, e);
      return true;
    }
    else if (e->keyval == GDK_Return) {
      dwb_eval_autocompletion();
      return true;
    }
  }
  else if (gtk_widget_has_focus(dwb.gui.entry) || dwb.state.mode & CompletionMode) {
    return false;
  }
  else if (e->keyval == GDK_Tab) {
    dwb_autocomplete(dwb.keymap, e);
  }
  ret = dwb_eval_key(e);
  g_free(key);

  return ret;
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


/*}}}*/


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
  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));
  const gchar *bof = back && forward ? "[+-]" : back ? "[+]" : forward  ? "[-]" : "";
  gchar *position = 
    upper == lower ? g_strdup_printf(" %s [all]", bof) : 
    value == lower ? g_strdup_printf(" %s [top]", bof) : 
    value == upper ? g_strdup_printf(" %s [bot]", bof) : 
    g_strdup_printf(" %s [%02d%%]", bof, (gint)(value * 100/upper));
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

/* FUNCTIONS {{{*/

/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  gint fact = dwb.state.layout & BottomStack;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
  dwb.state.size = size;
}/*}}}*/

/* dwb_clean_buffer{{{*/
void
dwb_clean_buffer(GList *gl) {
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
  CLEAR_COMMAND_TEXT(gl);
}/*}}}*/

/* dwb_parse_setting(const gchar *){{{*/
void
dwb_parse_setting(const gchar *text) {
  WebSettings *s;
  Arg *a = NULL;
  gchar **token = g_strsplit(text, " ", 2);

  GHashTable *t = dwb.state.setting_apply == Global ? dwb.settings : ((View*)dwb.state.fview->data)->setting;
  if (token[0]) {
    if  ( (s = g_hash_table_lookup(t, token[0])) ) {
      if ( (a = dwb_char_to_arg(token[1], s->type)) ) {
        s->arg = *a;
        dwb_apply_settings(s);
        gchar *message = g_strdup_printf("Saved setting %s: %s", s->n.first, s->type == Boolean ? ( s->arg.b ? "true" : "false") : token[1]);
        dwb_set_normal_message(dwb.state.fview, message, true);
        g_free(message);
      }
      else {
        dwb_set_error_message(dwb.state.fview, "No valid value.");
      }
    }
    else {
      gchar *message = g_strconcat("No such setting: ", token[0], NULL);
      dwb_set_normal_message(dwb.state.fview, message, true);
      g_free(message);
    }
  }
  dwb_normal_mode(false);

  g_strfreev(token);

}/*}}}*/

void
dwb_parse_key_setting(const gchar *text) {
  KeyValue value;
  gchar **token = g_strsplit(text, " ", 2);

  value.id = g_strdup(token[0]);

  gchar  **keys = g_strsplit(token[1], " ", -1);
  value.key = dwb_strv_to_key(keys, g_strv_length(keys));

  dwb.keymap = dwb_keymap_add(dwb.keymap, value);
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_keymap_sort_second);

  g_strfreev(token);
  g_strfreev(keys);
  dwb_normal_mode(true);
}

/* dwb_reload_colors(GList *,  WebSettings  *s) {{{*/
void 
dwb_reload_scripts(GList *gl, WebSettings *s) {
  g_free(dwb.misc.scripts);
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
    WebSettings *new = g_hash_table_lookup(dwb.settings, s->n.first);
    new->arg = s->arg;
  }

}/*}}}*/

/* dwb_layout_from_char {{{*/
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
}/*}}}*/

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
  WebSettings *new;
  if (dwb.state.setting_apply == Global) {
    new = g_hash_table_lookup(dwb.settings, s->n.first);
    new->arg = s->arg;
    for (GList *l = dwb.state.views; l; l=l->next)  {
      WebSettings *new =  g_hash_table_lookup((((View*)l->data)->setting), s->n.first);
      new->arg = s->arg;
      if (s->func) {
        s->func(l, s);
      }
    }
  }
  else {
    s->func(dwb.state.fview, s);
    //WebSettings *new =  g_hash_table_lookup((((View*)dwb.state.fview->data)->setting), s->n.first);
    //new->arg = s->arg;
  }
  dwb_normal_mode(false);

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
    if  (!strcmp(id, s->n.first)) {
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
    case Double:  g_object_set(settings, s->n.first, s->arg.d, NULL); break;
    case Integer: g_object_set(settings, s->n.first, s->arg.i, NULL); break;
    case Boolean: g_object_set(settings, s->n.first, s->arg.b, NULL); break;
    case Char:    g_object_set(settings, s->n.first, (gchar*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = CURRENT_WEBVIEW();
  switch (s->type) {
    case Double:  g_object_set(web, s->n.first, s->arg.d, NULL); break;
    case Integer: g_object_set(web, s->n.first, s->arg.i, NULL); break;
    case Boolean: g_object_set(web, s->n.first, s->arg.b, NULL); break;
    case Char:    g_object_set(web, s->n.first, (gchar*)s->arg.p, NULL); break;
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
  if (com) {
    buffer = dwb_execute_script(com);
    g_free(com);
  }
  if (buffer && strcmp(buffer, "undefined")) {
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
      dwb_normal_mode(false);
    }
    else if (!strcmp(buffer, "_dwb_input_")) {
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

/*dwb_prepend_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {{{*/
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

  (*fc) = g_list_append((*fc), n);
}/*}}}*/

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

  if (! (dwb.state.layout & Maximized)) {
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
    dwb_resize(dwb.state.size);
  }
  dwb_update_tab_label();
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = true;
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  gint keyval = e->keyval;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (e->is_modifier) {
    return false;
  }
  // don't show backspace in the buffer
  if (keyval == GDK_BackSpace ) {
    if (dwb.state.mode & AutoComplete) {
      dwb_clean_autocompletion();
    }
    if (dwb.state.buffer && dwb.state.buffer->str ) {
      if (dwb.state.buffer->len) {
        g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
        dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
      }
      ret = false;
    }
    else {
      ret = true;
    }
    return ret;
  }
  gchar *key = dwb_keyval_to_char(keyval);
  if (!key) {
    return false;
  }


  if (!old) {
    dwb.state.buffer = g_string_new(NULL);
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
  g_string_append(dwb.state.buffer, key);
  if (ALPHA(e) || DIGIT(e)) {
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
  }

  const gchar *buf = dwb.state.buffer->str;
  gint length = dwb.state.buffer->len;
  gint longest = 0;
  KeyMap *tmp = NULL;
  GList *coms = NULL;
  //gint prelen = strlen(pre);

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    gsize l = strlen(km->key);
    if (!km->key || !l) {
      continue;
    }
    if (dwb.comps.autocompletion && g_str_has_prefix(km->key, buf) && CLEAN_STATE(e) == km->mod) {
      coms = g_list_append(coms, km);
    }
    if (!strcmp(&buf[length - l], km->key) && (CLEAN_STATE(e) == km->mod)) {
      if  (!longest || l > longest) {
        longest = l;
        tmp = km;
      }
      ret = true;
    }
  }
  // autocompletion
  if (dwb.state.mode & AutoComplete) {
    dwb_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    dwb_autocomplete(coms, NULL);
  }
  if (tmp) {
    dwb_simple_command(tmp);
  }
  g_free(key);
  return ret;

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

/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode(gboolean clean) {
  View *v = dwb.state.fview->data;

  dwb_execute_script("clear()");
  if (dwb.state.mode  == InsertMode) {
    dwb_view_modify_style(dwb.state.fview, &dwb.color.active_fg, &dwb.color.active_bg, NULL, NULL, NULL, 0);
  }
  if (dwb.state.mode & CompletionMode) {
    dwb_clean_completion();
  }
  if (dwb.state.mode & AutoComplete) {
    dwb_clean_autocompletion();
  }

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  gtk_widget_grab_focus(v->scroll);

  //gtk_widget_hide(dwb.gui.entry);

  if (clean) {
    //CLEAR_COMMAND_TEXT(dwb.state.fview);
    dwb_clean_buffer(dwb.state.fview);
  }

  webkit_web_view_unmark_text_matches(CURRENT_WEBVIEW());

  //if (dwb.state.buffer) {
  //  g_string_free(dwb.state.buffer, true);
  //}
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
    if (v->status->search_string) {
      g_free(v->status->search_string);
    }
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
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_free(m);
  }
  remove(dwb.misc.fifo);
  g_list_free(dwb.keymap);
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
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    gchar *sc = g_strdup_printf("%s %s", dwb_modmask_to_string(map->mod), map->key ? map->key : "");
    g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);

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
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );
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

/* dwb_keymap_delete(GList *, KeyValue )     return: GList * {{{*/
GList * 
dwb_keymap_delete(GList *gl, KeyValue key) {
  for (GList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(km->map->n.first, key.id)) {
      gl = g_list_delete_link(gl, l);
      break;
    }
  }
  gl = g_list_sort(gl, (GCompareFunc)dwb_keymap_sort_second);
  return gl;
}/*}}}*/

/* dwb_keymap_add(GList *, KeyValue)     return: GList* {{{*/
GList *
dwb_keymap_add(GList *gl, KeyValue key) {
  gl = dwb_keymap_delete(gl, key);
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!strcmp(FMAP[i].n.first, key.id)) {
      KeyMap *keymap = malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : "";
      keymap->mod = key.key.mod;
      fmap->n.first = (gchar*)key.id;
      keymap->map = fmap;
      gl = g_list_prepend(gl, keymap);
      break;
    }
  }
  return gl;
}/*}}}*/
/*}}}*/

/* INIT {{{*/

/* dwb_read_config()    return: GList *{{{*/
GList *
dwb_read_key_config() {
  GKeyFile *keyfile = g_key_file_new();
  gsize numkeys;
  GError *error = NULL;
  gchar **keys;
  GList *gl;

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
          if (!strcmp(keys[i], DWB_SETTINGS[j].n.first)) {
            WebSettings *s = malloc(sizeof(WebSettings));
            *s = DWB_SETTINGS[j];
            if ( (arg = dwb_char_to_arg(value, s->type)) ) {
              s->arg = *arg;
            }
            //ret = g_slist_append(ret, s);
            gchar *key = g_strdup(s->n.first);
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
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_keymap_sort_second);
}/*}}}*/

/* dwb_init_settings() {{{*/
void
dwb_init_settings() {
  dwb.settings = g_hash_table_new(g_str_hash, g_str_equal);
  dwb.state.web_settings = webkit_web_settings_new();
  if (! g_file_test(dwb.files.settings, G_FILE_TEST_IS_REGULAR) || ! (dwb_read_settings()) ) {
    for (int i=0; i<LENGTH(DWB_SETTINGS); i++) {
      WebSettings *s = &DWB_SETTINGS[i];
      gchar *key = g_strdup(s->n.first);
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
  g_string_append_printf(buffer, "hint_letter_seq = '%s';\n",       GET_CHAR("hint-letter-seq"));
  g_string_append_printf(buffer, "hint_style = '%s';\n",            GET_CHAR("hint-style"));
  g_string_append_printf(buffer, "hint_font_size = '%s';\n",        GET_CHAR("hint-font-size"));
  g_string_append_printf(buffer, "hint_font_weight = '%s';\n",      GET_CHAR("hint-font-weight"));
  g_string_append_printf(buffer, "hint_font_family = '%s';\n",      GET_CHAR("hint-font-family"));
  g_string_append_printf(buffer, "hint_style = '%s';\n",            GET_CHAR("hint-style"));
  g_string_append_printf(buffer, "hint_fg_color = '%s';\n",         GET_CHAR("hint-fg-color"));
  g_string_append_printf(buffer, "hint_bg_color = '%s';\n",         GET_CHAR("hint-bg-color"));
  g_string_append_printf(buffer, "hint_active_color = '%s';\n",     GET_CHAR("hint-active-color"));
  g_string_append_printf(buffer, "hint_normal_color = '%s';\n",     GET_CHAR("hint-normal-color"));
  g_string_append_printf(buffer, "hint_border = '%s';\n",           GET_CHAR("hint-border"));
  g_string_append_printf(buffer, "hint_opacity = %f;\n",          GET_DOUBLE("hint-opacity"));

  // init system scripts

  gchar *dir;
  if ( (dir = dwb_get_data_dir("scripts")) ) {
    dwb_get_directory_content(&buffer, dir);
    //g_string_append(buffer, dircontent);
  }
  dwb_get_directory_content(&buffer, dwb.files.scriptdir);
  //dircontent = dwb_get_directory_content(dwb.files.scriptdir);
  //g_string_append(buffer, dircontent);
  //g_free(dircontent);
  dwb.misc.scripts = buffer->str;
  g_string_free(buffer, false);
}/*}}}*/

/* dwb_init_style() {{{*/
void
dwb_init_style() {
  // Colors 
  // Statusbar
  gdk_color_parse(GET_CHAR("active-fg-color"), &dwb.color.active_fg);
  gdk_color_parse(GET_CHAR("active-bg-color"), &dwb.color.active_bg);
  gdk_color_parse(GET_CHAR("normal-fg-color"), &dwb.color.normal_fg);
  gdk_color_parse(GET_CHAR("normal-bg-color"), &dwb.color.normal_bg);

  // Tabs
  gdk_color_parse(GET_CHAR("tab-active-fg-color"), &dwb.color.tab_active_fg);
  gdk_color_parse(GET_CHAR("tab-active-bg-color"), &dwb.color.tab_active_bg);
  gdk_color_parse(GET_CHAR("tab-normal-fg-color"), &dwb.color.tab_normal_fg);
  gdk_color_parse(GET_CHAR("tab-normal-bg-color"), &dwb.color.tab_normal_bg);

  //InsertMode 
  gdk_color_parse(GET_CHAR("insert-fg-color"), &dwb.color.insert_fg);
  gdk_color_parse(GET_CHAR("insert-bg-color"), &dwb.color.insert_bg);

  gdk_color_parse(GET_CHAR("active-comp-bg-color"), &dwb.color.active_c_bg);
  gdk_color_parse(GET_CHAR("active-comp-fg-color"), &dwb.color.active_c_fg);
  gdk_color_parse(GET_CHAR("normal-comp-bg-color"), &dwb.color.normal_c_bg);
  gdk_color_parse(GET_CHAR("normal-comp-fg-color"), &dwb.color.normal_c_fg);

  gdk_color_parse(GET_CHAR("error-color"), &dwb.color.error);

  dwb.color.settings_fg_color = GET_CHAR("settings-fg-color");
  dwb.color.settings_bg_color = GET_CHAR("settings-bg-color");

  // Fonts
  gint active_font_size = GET_INT("active-font-size");
  gint normal_font_size = GET_INT("normal-font-size");
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
  dwb.misc.settings_border = GET_CHAR("settings-border");
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
  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), GET_INT("default-width"), GET_INT("default-height"));
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


  //dwb_add_view(NULL);
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
  gchar *path = dwb_build_path();
  gchar *profile_path = g_strconcat(path, dwb.misc.profile, "/", NULL);
  if (!g_file_test(profile_path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(profile_path, 0755);
  }
  dwb.files.bookmarks     = g_strconcat(profile_path, "/bookmarks",     NULL);
  dwb.files.history       = g_strconcat(profile_path, "/history",       NULL);
  dwb.files.stylesheet    = g_strconcat(profile_path, "/stylesheet",    NULL);
  dwb.files.quickmarks    = g_strconcat(profile_path, "/quickmarks",    NULL);
  dwb.files.session       = g_strconcat(profile_path, "/session",       NULL);
  dwb.files.searchengines = g_strconcat(path, "searchengines", NULL);
  dwb.files.stylesheet    = g_strconcat(profile_path, "/stylesheet",    NULL);
  dwb.files.keys          = g_strconcat(path, "keys",          NULL);
  dwb.files.scriptdir     = g_strconcat(path, "/scripts",      NULL);
  dwb.files.settings      = g_strconcat(path, "settings",      NULL);
  dwb.files.cookies       = g_strconcat(profile_path, "/cookies",       NULL);
  dwb.files.cookies_allow = g_strconcat(profile_path, "/cookies.allow", NULL);

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (void*)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (void*)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (void*)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (void*)dwb_navigation_new_from_line); 
  if (g_list_last(dwb.fc.searchengines)) {
    dwb.misc.default_search = ((Navigation*)g_list_last(dwb.fc.searchengines)->data)->second;
  }
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
  dwb.comps.completions = NULL; 
  dwb.comps.active_comp = NULL;
  dwb.misc.max_c_items = MAX_COMPLETIONS;


  dwb_init_signals();
  dwb_init_files();
  dwb_init_key_map();
  dwb_init_settings();
  dwb_init_style();
  dwb_init_scripts();
  dwb_init_gui();

  dwb.misc.soupsession = webkit_get_default_session();
  dwb_init_proxy();
  dwb_init_cookies();

  dwb.misc.message_delay = GET_INT("message-delay");
  dwb.misc.history_length = GET_INT("history-length");
  dwb.state.size = GET_INT("size");
  dwb.misc.factor = GET_DOUBLE("factor");
  dwb.state.layout = dwb_layout_from_char(GET_CHAR("layout"));
  dwb.comps.autocompletion = GET_BOOL("auto-completion");
  if (dwb.state.layout & BottomStack) {
    Arg a = { .n = dwb.state.layout };
    dwb_set_orientation(&a);
  }
  if (dwb.misc.argc > 0) {
    for (gint i=0; i<dwb.misc.argc; i++) {
      Arg a = { .p = dwb.misc.argv[i] };
      dwb_add_view(&a);
    }
  }
  else {
    dwb_add_view(NULL);
  }
} /*}}}*/ /*}}}*/

void 
parse_command_line(const gchar *line) {
  gchar **token = g_strsplit(line, " ", 2);

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!strcmp(m->map->n.first, token[0])) {
      Arg a = m->map->arg;
      if (token[1]) {
        m->map->arg.p = token[1];
      }
      dwb_simple_command(m);
      m->map->arg = a;
    }
  }
  g_strfreev(token);
}

gboolean
handle_channel(GIOChannel *c, GIOCondition condition, void *data) {
  gchar *line = NULL;

  g_io_channel_read_line(c, &line, NULL, NULL, NULL);
  if (line) {
    g_strstrip(line);
    parse_command_line(line);
    g_io_channel_flush(c, NULL);
    g_free(line);
  }

  return true;
}
void 
dwb_init_fifo() {
  FILE *ff;
  gchar *path = dwb_build_path();
  gchar *unifile = g_strconcat(path, "dwb-uni.fifo", NULL);

  if (dwb.misc.single || g_file_test(unifile, G_FILE_TEST_EXISTS)) {
    dwb.misc.fifo = unifile;
  }
  else {
    dwb.misc.fifo = g_strdup_printf("%s/dwb-%d.fifo", path, getpid());
  }
  g_free(path);

  if (!g_file_test(dwb.misc.fifo, G_FILE_TEST_EXISTS)) {
    mkfifo(dwb.misc.fifo, 0666);
    GIOChannel *channel = g_io_channel_new_file(dwb.misc.fifo, "r+", NULL);
    g_io_add_watch(channel, G_IO_IN, (GIOFunc)handle_channel, NULL);
  }
  else {
    if ( (ff = fopen(dwb.misc.fifo, "w")) ) {
      if (dwb.misc.argc) {
        for (int i=0; i<dwb.misc.argc; i++) {
          fprintf(ff, "add_view %s\n", dwb.misc.argv[i]);
        }
      }
      else {
        fprintf(ff, "add_view\n");
      }
      fclose(ff);
    }
    exit(EXIT_SUCCESS);
  }
}

int main(gint argc, gchar *argv[]) {
  dwb.misc.name = NAME;
  dwb.misc.profile = "default";
  dwb.misc.argc = 0;
  gint last = 0;

  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'p' && argv[i++]) {
          dwb.misc.profile = argv[i];
        }
         if (argv[i][1] == 'u' && argv[i++]) {
          dwb.misc.single = true;
        }
      }
      else {
        last = i;
        break;
      }
    }
  }
  if (last) {
    dwb.misc.argv = &argv[last];
    dwb.misc.argc = g_strv_length(dwb.misc.argv);
  }
  dwb_init_fifo();
  gtk_init(&argc, &argv);
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
