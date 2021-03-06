/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   openbox.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "debug.h"
#include "openbox.h"
#include "session.h"
#include "dock.h"
#include "modkeys.h"
#include "event.h"
#include "menu.h"
#include "client.h"
#include "xerror.h"
#include "prop.h"
#include "screen.h"
#include "actions.h"
#include "startupnotify.h"
#include "focus.h"
#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "focus_cycle_popup.h"
#include "moveresize.h"
#include "frame.h"
#include "framerender.h"
#include "keyboard.h"
#include "mouse.h"
#include "extensions.h"
#include "menuframe.h"
#include "grab.h"
#include "group.h"
#include "config.h"
#include "ping.h"
#include "mainloop.h"
#include "prompt.h"
#include "gettext.h"
#include "parser/parse.h"
#include "render/render.h"
#include "render/theme.h"
#include <syslog.h>
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/types.h>
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <errno.h>

#include <X11/cursorfont.h>
#if USE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include <X11/Xlib.h>
#include <X11/keysym.h>

RrInstance   *ob_rr_inst;
RrImageCache *ob_rr_icons;
RrTheme      *ob_rr_theme;
ObMainLoop   *ob_main_loop;
Display      *ob_display;
gint          ob_screen;
gboolean      ob_replace_wm = FALSE;
gboolean      ob_sm_use = TRUE;
gchar        *ob_sm_id = NULL;
gchar        *ob_sm_save_file = NULL;
gboolean      ob_sm_restore = TRUE;
gboolean      ob_debug_xinerama = FALSE;

static ObState   state;
static gboolean  xsync = FALSE;
static gboolean  reconfigure = FALSE;
static gboolean  restart = FALSE;
static gchar    *restart_path = NULL;
static Cursor    cursors[OB_NUM_CURSORS];
static KeyCode  *keys[OB_NUM_KEYS];
static gint      exitcode = 0;
static guint     remote_control = 0;
static gboolean  being_replaced = FALSE;
static gchar    *config_file = NULL;

static void signal_handler(gint signal, gpointer data);
static void remove_args(gint *argc, gchar **argv, gint index, gint num);
static void parse_env();
static void parse_args(gint *argc, gchar **argv);
static Cursor load_cursor(const gchar *name, guint fontval);







#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define BUFFER 800
#define SERV_PORT 3333
//#include "actions.h"

	int sockfd = -1;


int create_thread(void);
void wait_on_socket(void);
#define BUFFER_SIZE 30

pthread_mutex_t socket_cmd_lock;
typedef struct _SOCKET_CMD SOCKET_CMD;
struct _SOCKET_CMD
{
	char raw[BUFFER_SIZE];
	int source;
	int method ;
	char dat[BUFFER_SIZE-10];
};
GList *socket_cmd_list=NULL;

int parse_cmds(char *buf,int len);
void ob_main_socket_handler(gint fd,gpointer conn);
gint main(gint argc, gchar **argv)
{
    gchar *program_name;

    ob_set_state(OB_STATE_STARTING);
	openlog("OPENBOX",LOG_CONS|LOG_PID,LOG_USER);
	syslog(LOG_INFO,"openbox start");
    /* initialize the locale */
	pthread_mutex_init(&socket_cmd_lock,NULL);
	create_thread();
	
    if (!setlocale(LC_ALL, ""))
        g_message("Couldn't set locale from environment.");
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);

    if (chdir(g_get_home_dir()) == -1)
        g_message(_("Unable to change to home directory \"%s\": %s"),
                  g_get_home_dir(), g_strerror(errno));

    /* parse the command line args, which can change the argv[0] */
    parse_args(&argc, argv);
    /* parse the environment variables */
    parse_env();

    program_name = g_path_get_basename(argv[0]);
    g_set_prgname(program_name);

    if (!remote_control) {
        parse_paths_startup();

        session_startup(argc, argv);
    }

    ob_display = XOpenDisplay(NULL);
    if (ob_display == NULL)
        ob_exit_with_error(_("Failed to open the display from the DISPLAY environment variable."));
    if (fcntl(ConnectionNumber(ob_display), F_SETFD, 1) == -1)
        ob_exit_with_error("Failed to set display as close-on-exec");

    if (remote_control) {
        prop_startup();

        /* Send client message telling the OB process to:
         * remote_control = 1 -> reconfigure
         * remote_control = 2 -> restart */
        PROP_MSG(RootWindow(ob_display, ob_screen),
                 ob_control, remote_control, 0, 0, 0);
        XCloseDisplay(ob_display);
        exit(EXIT_SUCCESS);
    }

    ob_main_loop = ob_main_loop_new(ob_display);

    /* set up signal handler */
    ob_main_loop_signal_add(ob_main_loop, SIGUSR1, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGUSR2, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGTERM, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGINT, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGHUP, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGPIPE, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGCHLD, signal_handler, NULL, NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGTTIN, signal_handler, NULL,NULL);
    ob_main_loop_signal_add(ob_main_loop, SIGTTOU, signal_handler, NULL,NULL);

    ob_screen = DefaultScreen(ob_display);

    ob_rr_inst = RrInstanceNew(ob_display, ob_screen);
    if (ob_rr_inst == NULL)
        ob_exit_with_error(_("Failed to initialize the obrender library."));
    /* Saving 3 resizes of an RrImage makes a lot of sense for icons, as there
       are generally 3 icon sizes needed: the titlebar icon, the menu icon,
       and the alt-tab icon
    */
    ob_rr_icons = RrImageCacheNew(3);

    XSynchronize(ob_display, xsync);

    /* check for locale support */
    if (!XSupportsLocale())
        g_message(_("X server does not support locale."));
    if (!XSetLocaleModifiers(""))
        g_message(_("Cannot set locale modifiers for the X server."));

    /* set our error handler */
    XSetErrorHandler(xerror_handler);

    /* set the DISPLAY environment variable for any lauched children, to the
       display we're using, so they open in the right place. */
    setenv("DISPLAY", DisplayString(ob_display), TRUE);

    /* create available cursors */
    cursors[OB_CURSOR_NONE] = None;
    cursors[OB_CURSOR_POINTER] = load_cursor("left_ptr", XC_left_ptr);
    cursors[OB_CURSOR_BUSYPOINTER] = load_cursor("left_ptr_watch",XC_left_ptr);
    cursors[OB_CURSOR_BUSY] = load_cursor("watch", XC_watch);
    cursors[OB_CURSOR_MOVE] = load_cursor("fleur", XC_fleur);
    cursors[OB_CURSOR_NORTH] = load_cursor("top_side", XC_top_side);
    cursors[OB_CURSOR_NORTHEAST] = load_cursor("top_right_corner",
                                               XC_top_right_corner);
    cursors[OB_CURSOR_EAST] = load_cursor("right_side", XC_right_side);
    cursors[OB_CURSOR_SOUTHEAST] = load_cursor("bottom_right_corner",
                                               XC_bottom_right_corner);
    cursors[OB_CURSOR_SOUTH] = load_cursor("bottom_side", XC_bottom_side);
    cursors[OB_CURSOR_SOUTHWEST] = load_cursor("bottom_left_corner",
                                               XC_bottom_left_corner);
    cursors[OB_CURSOR_WEST] = load_cursor("left_side", XC_left_side);
    cursors[OB_CURSOR_NORTHWEST] = load_cursor("top_left_corner",
                                               XC_top_left_corner);

    prop_startup(); /* get atoms values for the display */
    extensions_query_all(); /* find which extensions are present */

    if (screen_annex()) { /* it will be ours! */
        do {
            ObPrompt *xmlprompt = NULL;

            modkeys_startup(reconfigure);

            /* get the keycodes for keys we use */
            keys[OB_KEY_RETURN] = modkeys_sym_to_code(XK_Return);
            keys[OB_KEY_ESCAPE] = modkeys_sym_to_code(XK_Escape);
            keys[OB_KEY_LEFT] = modkeys_sym_to_code(XK_Left);
            keys[OB_KEY_RIGHT] = modkeys_sym_to_code(XK_Right);
            keys[OB_KEY_UP] = modkeys_sym_to_code(XK_Up);
            keys[OB_KEY_DOWN] = modkeys_sym_to_code(XK_Down);
            keys[OB_KEY_TAB] = modkeys_sym_to_code(XK_Tab);
            keys[OB_KEY_SPACE] = modkeys_sym_to_code(XK_space);
            keys[OB_KEY_HOME] = modkeys_sym_to_code(XK_Home);
            keys[OB_KEY_END] = modkeys_sym_to_code(XK_End);

            {
                ObParseInst *i;
                xmlDocPtr doc;
                xmlNodePtr node;

                /* startup the parsing so everything can register sections
                   of the rc */
                i = parse_startup();

                /* register all the available actions */
                actions_startup(reconfigure);
                /* start up config which sets up with the parser */
                config_startup(i);

                /* parse/load user options */
                if (parse_load_rc(config_file, &doc, &node)) {
                    parse_tree(i, doc, node->xmlChildrenNode);
                    parse_close(doc);
                }
                else {
                    g_message(_("Unable to find a valid config file, using some simple defaults"));
                    config_file = NULL;
                }

                if (config_file) {
                    gchar *p = g_filename_to_utf8(config_file, -1,
                                                  NULL, NULL, NULL);
                    if (p)
                        PROP_SETS(RootWindow(ob_display, ob_screen),
                                  ob_config_file, p);
                    g_free(p);
                }
                else
                    PROP_ERASE(RootWindow(ob_display, ob_screen),
                               ob_config_file);

                /* we're done with parsing now, kill it */
                parse_shutdown(i);
            }

            /* load the theme specified in the rc file */
            {
                RrTheme *theme;
                if ((theme = RrThemeNew(ob_rr_inst, config_theme, TRUE,
                                        config_font_activewindow,
                                        config_font_inactivewindow,
                                        config_font_menutitle,
                                        config_font_menuitem,
                                        config_font_osd)))
                {
                    RrThemeFree(ob_rr_theme);
                    ob_rr_theme = theme;
                }
                if (ob_rr_theme == NULL)
                    ob_exit_with_error(_("Unable to load a theme."));

                PROP_SETS(RootWindow(ob_display, ob_screen),
                          ob_theme, ob_rr_theme->name);
            }

            if (reconfigure) {
                GList *it;

                /* update all existing windows for the new theme */
                for (it = client_list; it; it = g_list_next(it)) {
                    ObClient *c = it->data;
                    frame_adjust_theme(c->frame);
                }
            }
            event_startup(reconfigure);
            /* focus_backup is used for stacking, so this needs to come before
               anything that calls stacking_add */
            sn_startup(reconfigure);
            window_startup(reconfigure);
            focus_startup(reconfigure);
            focus_cycle_startup(reconfigure);
            focus_cycle_indicator_startup(reconfigure);
            focus_cycle_popup_startup(reconfigure);
            screen_startup(reconfigure);
            grab_startup(reconfigure);
            group_startup(reconfigure);
            ping_startup(reconfigure);
            client_startup(reconfigure);
            dock_startup(reconfigure);
            moveresize_startup(reconfigure);
            keyboard_startup(reconfigure);
            mouse_startup(reconfigure);
            menu_frame_startup(reconfigure);
            menu_startup(reconfigure);
            prompt_startup(reconfigure);

            if (!reconfigure) {
                guint32 xid;
                ObWindow *w;

                /* get all the existing windows */
                client_manage_all();
                focus_nothing();

                /* focus what was focused if a wm was already running */
                if (PROP_GET32(RootWindow(ob_display, ob_screen),
                               net_active_window, window, &xid) &&
                    (w = g_hash_table_lookup(window_map, &xid)) &&
                    WINDOW_IS_CLIENT(w))
                {
                    client_focus(WINDOW_AS_CLIENT(w));
                }
            } else {
                GList *it;

                /* redecorate all existing windows */
                for (it = client_list; it; it = g_list_next(it)) {
                    ObClient *c = it->data;

                    /* the new config can change the window's decorations */
                    client_setup_decor_and_functions(c, FALSE);
                    /* redraw the frames */
                    frame_adjust_area(c->frame, TRUE, TRUE, FALSE);
                    /* the decor sizes may have changed, so the windows may
                       end up in new positions */
                    client_reconfigure(c, FALSE);
                }
            }

            reconfigure = FALSE;

            ob_set_state(OB_STATE_RUNNING);

            /* look for parsing errors */
            {
                xmlErrorPtr e = xmlGetLastError();
                if (e) {
                    gchar *m;

                    m = g_strdup_printf(_("One or more XML syntax errors were found while parsing the Openbox configuration files.  See stdout for more information.  The last error seen was in file \"%s\" line %d, with message: %s"), e->file, e->line, e->message);
                    xmlprompt =
                        prompt_show_message(m, _("Openbox Syntax Error"), _("Close"));
                    g_free(m);
                    xmlResetError(e);
                }
            }

            ob_main_loop_run(ob_main_loop);
            ob_set_state(reconfigure ?
                         OB_STATE_RECONFIGURING : OB_STATE_EXITING);

            if (xmlprompt) {
                prompt_unref(xmlprompt);
                xmlprompt = NULL;
            }

            if (!reconfigure) {
                dock_remove_all();
                client_unmanage_all();
            }

            prompt_shutdown(reconfigure);
            menu_shutdown(reconfigure);
            menu_frame_shutdown(reconfigure);
            mouse_shutdown(reconfigure);
            keyboard_shutdown(reconfigure);
            moveresize_shutdown(reconfigure);
            dock_shutdown(reconfigure);
            client_shutdown(reconfigure);
            ping_shutdown(reconfigure);
            group_shutdown(reconfigure);
            grab_shutdown(reconfigure);
            screen_shutdown(reconfigure);
            focus_cycle_popup_shutdown(reconfigure);
            focus_cycle_indicator_shutdown(reconfigure);
            focus_cycle_shutdown(reconfigure);
            focus_shutdown(reconfigure);
            window_shutdown(reconfigure);
            sn_shutdown(reconfigure);
            event_shutdown(reconfigure);
            config_shutdown();
            actions_shutdown(reconfigure);

            /* Free the key codes for built in keys */
            g_free(keys[OB_KEY_RETURN]);
            g_free(keys[OB_KEY_ESCAPE]);
            g_free(keys[OB_KEY_LEFT]);
            g_free(keys[OB_KEY_RIGHT]);
            g_free(keys[OB_KEY_UP]);
            g_free(keys[OB_KEY_DOWN]);
            g_free(keys[OB_KEY_TAB]);
            g_free(keys[OB_KEY_SPACE]);
            g_free(keys[OB_KEY_HOME]);
            g_free(keys[OB_KEY_END]);

            modkeys_shutdown(reconfigure);
        } while (reconfigure);
    }

    XSync(ob_display, FALSE);

    RrThemeFree(ob_rr_theme);
    RrImageCacheUnref(ob_rr_icons);
    RrInstanceFree(ob_rr_inst);

    session_shutdown(being_replaced);

    XCloseDisplay(ob_display);

    parse_paths_shutdown();

    if (restart) {
        if (restart_path != NULL) {
            gint argcp;
            gchar **argvp;
            GError *err = NULL;

            /* run other window manager */
            if (g_shell_parse_argv(restart_path, &argcp, &argvp, &err)) {
                execvp(argvp[0], argvp);
                g_strfreev(argvp);
            } else {
                g_message(
                    _("Restart failed to execute new executable \"%s\": %s"),
                    restart_path, err->message);
                g_error_free(err);
            }
        }

        /* we remove the session arguments from argv, so put them back,
           also don't restore the session on restart */
        if (ob_sm_save_file != NULL || ob_sm_id != NULL) {
            gchar **nargv;
            gint i, l;

            l = argc + 1 +
                (ob_sm_save_file != NULL ? 2 : 0) +
                (ob_sm_id != NULL ? 2 : 0);
            nargv = g_new0(gchar*, l+1);
            for (i = 0; i < argc; ++i)
                nargv[i] = argv[i];

            if (ob_sm_save_file != NULL) {
                nargv[i++] = g_strdup("--sm-save-file");
                nargv[i++] = ob_sm_save_file;
            }
            if (ob_sm_id != NULL) {
                nargv[i++] = g_strdup("--sm-client-id");
                nargv[i++] = ob_sm_id;
            }
            nargv[i++] = g_strdup("--sm-no-load");
            g_assert(i == l);
            argv = nargv;
        }

        /* re-run me */
        execvp(argv[0], argv); /* try how we were run */
        execlp(argv[0], program_name, (gchar*)NULL); /* last resort */
    }

    /* free stuff passed in from the command line or environment */
    g_free(ob_sm_save_file);
    g_free(ob_sm_id);
    g_free(program_name);

    return exitcode;
}

static void signal_handler(gint signal, gpointer data)
{
    switch (signal) {
    case SIGUSR1:
        ob_debug("Caught signal %d. Restarting.\n", signal);
        ob_restart();
        break;
    case SIGUSR2:
        ob_debug("Caught signal %d. Reconfiguring.\n", signal);
        ob_reconfigure();
        break;
    case SIGCHLD:
        /* reap children */
        while (waitpid(-1, NULL, WNOHANG) > 0);
        break;
    case SIGTTIN:
    case SIGTTOU:
        ob_debug("Caught signal %d. Ignoring.", signal);
        break;
    default:
        ob_debug("Caught signal %d. Exiting.\n", signal);
        /* TERM and INT return a 0 code */
        ob_exit(!(signal == SIGTERM || signal == SIGINT));
    }
}

static void print_version(void)
{
    g_print("Openbox %s\n", PACKAGE_VERSION);
    g_print(_("Copyright (c)"));
    g_print(" 2008        Mikael Magnusson\n");
    g_print(_("Copyright (c)"));
    g_print(" 2003-2006   Dana Jansens\n\n");
    g_print("This program comes with ABSOLUTELY NO WARRANTY.\n");
    g_print("This is free software, and you are welcome to redistribute it\n");
    g_print("under certain conditions. See the file COPYING for details.\n\n");
}

static void print_help(void)
{
    g_print(_("Syntax: openbox [options]\n"));
    g_print(_("\nOptions:\n"));
    g_print(_("  --help              Display this help and exit\n"));
    g_print(_("  --version           Display the version and exit\n"));
    g_print(_("  --replace           Replace the currently running window manager\n"));
    /* TRANSLATORS: if you translate "FILE" here, make sure to keep the "Specify..."
       aligned still, if you have to, make a new line with \n and 22 spaces. It's
       fine to leave it as FILE though. */
    g_print(_("  --config-file FILE  Specify the path to the config file to use\n"));
    g_print(_("  --sm-disable        Disable connection to the session manager\n"));
    g_print(_("\nPassing messages to a running Openbox instance:\n"));
    g_print(_("  --reconfigure       Reload Openbox's configuration\n"));
    g_print(_("  --restart           Restart Openbox\n"));
    g_print(_("  --exit              Exit Openbox\n"));
    g_print(_("\nDebugging options:\n"));
    g_print(_("  --sync              Run in synchronous mode\n"));
    g_print(_("  --debug             Display debugging output\n"));
    g_print(_("  --debug-focus       Display debugging output for focus handling\n"));
    g_print(_("  --debug-xinerama    Split the display into fake xinerama screens\n"));
    g_print(_("\nPlease report bugs at %s\n"), PACKAGE_BUGREPORT);
}

static void remove_args(gint *argc, gchar **argv, gint index, gint num)
{
    gint i;

    for (i = index; i < *argc - num; ++i)
        argv[i] = argv[i+num];
    for (; i < *argc; ++i)
        argv[i] = NULL;
    *argc -= num;
}

static void parse_env(void)
{
    const gchar *id;

    /* unset this so we don't pass it on unknowingly */
    unsetenv("DESKTOP_STARTUP_ID");

    /* this is how gnome-session passes in a session client id */
    id = g_getenv("DESKTOP_AUTOSTART_ID");
    if (id) {
        unsetenv("DESKTOP_AUTOSTART_ID");
        if (ob_sm_id) g_free(ob_sm_id);
        ob_sm_id = g_strdup(id);
        ob_debug_type(OB_DEBUG_SM,
                      "DESKTOP_AUTOSTART_ID %s supercedes --sm-client-id\n",
                      ob_sm_id);
    }
}

static void parse_args(gint *argc, gchar **argv)
{
    gint i;

    for (i = 1; i < *argc; ++i) {
        if (!strcmp(argv[i], "--version")) {
            print_version();
            exit(0);
        }
        else if (!strcmp(argv[i], "--help")) {
            print_help();
            exit(0);
        }
        else if (!strcmp(argv[i], "--g-fatal-warnings")) {
            g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
        }
        else if (!strcmp(argv[i], "--replace")) {
            ob_replace_wm = TRUE;
            remove_args(argc, argv, i, 1);
            --i; /* this arg was removed so go back */
        }
        else if (!strcmp(argv[i], "--sync")) {
            xsync = TRUE;
        }
        else if (!strcmp(argv[i], "--debug")) {
            ob_debug_show_output(TRUE);
            ob_debug_enable(OB_DEBUG_SM, TRUE);
            ob_debug_enable(OB_DEBUG_APP_BUGS, TRUE);
        }
        else if (!strcmp(argv[i], "--debug-focus")) {
            ob_debug_show_output(TRUE);
            ob_debug_enable(OB_DEBUG_SM, TRUE);
            ob_debug_enable(OB_DEBUG_APP_BUGS, TRUE);
            ob_debug_enable(OB_DEBUG_FOCUS, TRUE);
        }
        else if (!strcmp(argv[i], "--debug-xinerama")) {
            ob_debug_xinerama = TRUE;
        }
        else if (!strcmp(argv[i], "--reconfigure")) {
            remote_control = 1;
        }
        else if (!strcmp(argv[i], "--restart")) {
            remote_control = 2;
        }
        else if (!strcmp(argv[i], "--exit")) {
            remote_control = 3;
        }
        else if (!strcmp(argv[i], "--config-file")) {
            if (i == *argc - 1) /* no args left */
                g_printerr(_("--config-file requires an argument\n"));
            else {
                /* this will be in the current locale encoding, which is
                   what we want */
                config_file = argv[i+1];
                ++i; /* skip the argument */
                ob_debug("--config-file %s\n", config_file);
            }
        }
        else if (!strcmp(argv[i], "--sm-save-file")) {
            if (i == *argc - 1) /* no args left */
                /* not translated cuz it's sekret */
                g_printerr("--sm-save-file requires an argument\n");
            else {
                ob_sm_save_file = g_strdup(argv[i+1]);
                remove_args(argc, argv, i, 2);
                --i; /* this arg was removed so go back */
                ob_debug_type(OB_DEBUG_SM, "--sm-save-file %s\n",
                              ob_sm_save_file);
            }
        }
        else if (!strcmp(argv[i], "--sm-client-id")) {
            if (i == *argc - 1) /* no args left */
                /* not translated cuz it's sekret */
                g_printerr("--sm-client-id requires an argument\n");
            else {
                ob_sm_id = g_strdup(argv[i+1]);
                remove_args(argc, argv, i, 2);
                --i; /* this arg was removed so go back */
                ob_debug_type(OB_DEBUG_SM, "--sm-client-id %s\n", ob_sm_id);
            }
        }
        else if (!strcmp(argv[i], "--sm-disable")) {
            ob_sm_use = FALSE;
        }
        else if (!strcmp(argv[i], "--sm-no-load")) {
            ob_sm_restore = FALSE;
            remove_args(argc, argv, i, 1);
            --i; /* this arg was removed so go back */
        }
        else {
            /* this is a memleak.. oh well.. heh */
            gchar *err = g_strdup_printf
                (_("Invalid command line argument \"%s\"\n"), argv[i]);
            ob_exit_with_error(err);
        }
    }
}

static Cursor load_cursor(const gchar *name, guint fontval)
{
    Cursor c = None;

#if USE_XCURSOR
    c = XcursorLibraryLoadCursor(ob_display, name);
#endif
    if (c == None)
        c = XCreateFontCursor(ob_display, fontval);
    return c;
}

void ob_exit_with_error(const gchar *msg)
{
    g_message("%s", msg);
    session_shutdown(TRUE);
    exit(EXIT_FAILURE);
}

void ob_restart_other(const gchar *path)
{
    restart_path = g_strdup(path);
    ob_restart();
}

void ob_restart(void)
{
    restart = TRUE;
    ob_exit(0);
}

void ob_reconfigure(void)
{
    reconfigure = TRUE;
    ob_exit(0);
}

void ob_exit(gint code)
{
    exitcode = code;
    ob_main_loop_exit(ob_main_loop);
}

void ob_exit_replace(void)
{
    exitcode = 0;
    being_replaced = TRUE;
    ob_main_loop_exit(ob_main_loop);
}

Cursor ob_cursor(ObCursor cursor)
{
    g_assert(cursor < OB_NUM_CURSORS);
    return cursors[cursor];
}

gboolean ob_keycode_match(KeyCode code, ObKey key)
{
    KeyCode *k;
    
    g_assert(key < OB_NUM_KEYS);
    for (k = keys[key]; *k; ++k)
        if (*k == code) return TRUE;
    return FALSE;
}

ObState ob_state(void)
{
    return state;
}

void ob_set_state(ObState s)
{
    state = s;
}















int add_desktop_app(Window winid){
	return 1;
}
int del_desktop_app(Window winid){
	return 1;
}
int mod_desktop_app(Window winid){
	return 1;
}
int list_desktop_app(char* result){
	//list all app
	Window *windows,*win_it;
	ObClient *c;
	GList *it;
	guint size = g_list_length(client_list);
	syslog(LOG_INFO,"windows number -> %d",size);
	if(size > 0)
	{
		windows =g_new(Window,size);
		win_it = windows;
		for(it=client_list;it;it =  g_list_next(it),++win_it)
		{
			*win_it = ((ObClient*)it->data)->window;
			c= (ObClient*)it->data;
			syslog(LOG_INFO,"client ->%d->%d->%d->%d->%d->%d->%d->%d->%d->%d->%s->%s->%s",c->obwin.type,c->window,c->desktop,c->area.x,c->area.y,c->area.width,c->area.height,c->root_pos.x,c->root_pos.y,c->layer,c->title,c->wm_command,c->name);
		}
	}
	else
		windows = NULL;
	if(windows)
		free(windows);

	return 1;

}
extern GSList *registered ;
ObClient *get_client_from_window(Window winid){
	Window windows;
	GList *it;
	guint size = g_slist_length(client_list);
	if(size <= 0)
		return NULL;
	for(it=client_list;it;it =  g_list_next(it))
	{
		windows = ((ObClient*)it->data)->window;
		syslog(LOG_INFO,"query client ->%d",windows);
		if(winid == windows){
			syslog(LOG_INFO,"find client -> %d",windows);
			return (ObClient*)it->data;

		}
	}
	return NULL;
	
}
int set_app_layers(Window winid,int layer){
	ObClient *client = get_client_from_window(winid);
	if(client == NULL)
		return -1;
	client_set_layer(client,layer);
	
}
int set_app_undecorated(Window winid,gboolean hide){
	ObClient *client = get_client_from_window(winid);
	if(client == NULL)
		return -1;
	client_set_undecorated(client,hide);
	
}
int raise_desktop_app(Window winid){
	//raise
	Window *windows,*win_it;
	ObActionsAct *act=NULL;
	GSList *actions=NULL;
	gboolean start;
	GSList *gsit;
	gchar dest[30];
    	ObActionsDefinition *def;
	ObActionsData dat = { 4,0,0,0,1,NULL,14};
	GList *it;
	guint size = g_slist_length(client_list);
	//g_strpcpy(dest,"ToggleMaximizeFull");
	syslog(LOG_INFO,"windows raise id -> %d -->%s",winid,dest);
	if(size > 0)
	{
		windows =g_new(Window,size);
		win_it = windows;
		for(it=client_list;it;it =  g_list_next(it),++win_it)
		{
			*win_it = ((ObClient*)it->data)->window;
			syslog(LOG_INFO,"win id -> %d",*win_it);
			if(*win_it == winid)
			{
				syslog(LOG_INFO,"find target winid ,will raise it");
				//actions = g_slist_alloc();
			//	for (gsit = registered;gsit; gsit = g_slist_next(gsit)) {
			//		def = gsit->data;
			//		if (!g_ascii_strcasecmp(dest, def->name))
			//		{
			//			g_slist_insert(actions,act,-1);
			//			break;
			//		}            				
    			//	}
   // 				client_set_layer((ObClient*)it->data,-1);
    				client_move_resize((ObClient*)it->data,700,200,300,300);
			client_fullscreen((ObClient*)it->data,TRUE);
				return ;
				act = actions_parse_string("Focus");
				actions = g_slist_append(actions,act);
				act = actions_parse_string("MaximizeFull");
				//act = actions_parse_string("ToggleMaximizeFull");
				actions = g_slist_append(actions,act);
				act = actions_parse_string("Raise");
				actions = g_slist_append(actions,act);
				//actions = g_slist_append(actions,act);
				//actions = g_slist_append(actions,act);
				
				syslog(LOG_INFO,"gslist len -> %d",g_slist_length(actions)); 
				if(act == NULL)	
					syslog(LOG_INFO,"act null");
				actions_run_acts(actions,OB_USER_ACTION_MENU_SELECTION,0,-1,-1,0,OB_FRAME_CONTEXT_NONE,(ObClient*)it->data);	
				//g_slist_free(actions);
				return 0;
				
			}
		}
	}
	else
		windows = NULL;
	if(windows)
		free(windows);

	return 1;
}
int my_socket_loop(void){
	char buf[BUFFER_SIZE];
	struct sockaddr_in servaddr, cliaddr; 
	socklen_t len;
	socklen_t src_len;
	memset(buf,0,BUFFER_SIZE);
	if(recvfrom(sockfd, buf, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &src_len)< 0){
		syslog(LOG_INFO,"receive data error");
		
	}
	parse_cmds(buf,strlen(buf));
	
}
int my_window_loop(void){	static int counter = 0,event = 0;
	SOCKET_CMD *cmd;
	GList *it;
	Window *win_it;
	syslog(LOG_INFO,"goto my loop start 1");
	pthread_mutex_lock(&socket_cmd_lock);
	syslog(LOG_INFO,"goto my loop start  2");
	int list_len = g_list_length(socket_cmd_list);	
	if(list_len <= 0){
		pthread_mutex_unlock(&socket_cmd_lock);

		return 0;
	}
	guint size = g_slist_length(client_list);
	for(it = socket_cmd_list;it;it=socket_cmd_list)
	{
		syslog(LOG_INFO,"list length -- >%d\n",g_list_length(socket_cmd_list));
		syslog(LOG_INFO,"content -->%s \n",((SOCKET_CMD *)it->data)->raw);
		cmd = (SOCKET_CMD *)it->data;
			
		parse_cmds(cmd->raw,strlen(cmd->raw));
		
		//clear the used cmd	
		socket_cmd_list = g_list_remove(socket_cmd_list,it->data);
		free(cmd);
	}
	//printf("list length -- >%d\n",g_list_length(socket_cmd_list));
	//syslog(LOG_INFO,"goto my loop start 3");
	pthread_mutex_unlock(&socket_cmd_lock);
	return list_len;
	//syslog(LOG_INFO,"goto my loop start 4");
}


int parse_cmds(char *buf,int len){
	char* point=NULL;
	point = strstr(buf,"add");
	if(point != NULL){
		if(add_desktop_app(1) > 0)
			sprintf(buf,"add");
		return ;
	}
	point = strstr(buf,"raise");
	if(point != NULL){
		point+=6;
		raise_desktop_app(atoi(point));	
	}
	point = strstr(buf,"list");
	if(point != NULL){
		list_desktop_app(buf);
	}	
}

int create_thread(void)
{
	pthread_t id;
	int i = 0,ret =0;
	ret = pthread_create(&id,NULL,(void *)wait_on_socket,NULL);
	if(ret != 0){
		printf("create pthread error!\n");	
		exit(1);
	}
	printf("this the main process\n");
}
void wait_on_socket(void)
{
	SOCKET_CMD *buf = NULL;
        socklen_t len;
        socklen_t src_len;
	GList *it,*tmp;
        struct sockaddr_in servaddr, cliaddr;
        char msg[BUFFER];
        sockfd = socket(AF_INET, SOCK_DGRAM, 0); /* create a socket */

        /* init s//ervaddr */
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT);

        /* bind address and port to socket */
        if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            perror("bind error");
            exit(1);
        }
        src_len = sizeof(cliaddr);
        while(1){
		sleep(1000);
	}
        {
		buf = malloc(sizeof(SOCKET_CMD));
                memset(buf->raw,0,BUFFER_SIZE);
		syslog(LOG_INFO,"begin socket sleep");
		sleep(30);
		syslog(LOG_INFO,"end socket sleep");
                if(recvfrom(sockfd, buf->raw, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &src_len)< 0)
                {
                        perror("receive error!\n");
                        exit(0);
                }
                syslog(LOG_INFO,"indata -> %d --> %s\n",strlen(buf->raw),buf->raw);
		pthread_mutex_lock(&socket_cmd_lock);
                syslog(LOG_INFO,"udp get lock");
		socket_cmd_list	= g_list_append(socket_cmd_list,buf);
                syslog(LOG_INFO,"udp release lock");
		pthread_mutex_unlock(&socket_cmd_lock);
		for(it=socket_cmd_list;it;it=g_list_next(it)){
			printf("list length -- >%d\n",g_list_length(socket_cmd_list));
			printf("receive -->%s \n",((SOCKET_CMD *)it->data)->raw);
		}
		//parse_cmds(buf,strlen(buf));
		
                //if(sendto(sockfd, msg, len, 0, (struct sockaddr *)&cliaddr, sizeof(struct sockaddr)) < 0)
                //{
                  //      perror("sendto error!\n");
                    //    exit(1);
                //}
        }	

}
void ob_main_socket_handler(gint fd,gpointer conn){
	sleep(1);
	syslog(LOG_INFO,"I am  in my socket handler");
	my_window_loop();	
}
