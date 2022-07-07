#include <stdio.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <pango/pango.h>
#include <cairo/cairo.h>

GdkRGBA color_bg;
GdkRGBA color_border;
static gchar *bgcolor = NULL;
static gchar *border_color = NULL;
static gchar *color = NULL;
static gchar *date = NULL;
static gchar *font = NULL;
static gboolean above = FALSE;
static gint border;
static gboolean center = FALSE;
static gboolean lock = FALSE;
static gboolean stick = FALSE;
static gint opacity;
static gint  posx = 0;
static gint  posy = 0;

static gboolean
on_timeout (gpointer user_data)
{
  GtkLabel *label = GTK_LABEL (user_data);
  GError *error = NULL;
  char *out = NULL;

  time_t timer;
  char buff[64];
  struct tm* tm_info;

  time (&timer);
  tm_info = localtime (&timer);
  strftime (buff, 64, date, tm_info);

  gchar *markup = g_strdup_printf ("<b><span font='%s' foreground='%s'>%s</span></b>",
                                   font, color, buff);
  if (pango_parse_markup (markup,
                         -1,
                          0,
                          NULL,
                          &out,
                          NULL,
                          &error) == FALSE)
  {
    g_printerr ("%s\nSwitching to default values\n", error->message);
    color = "red";
    date = "%r";
    font = "Sans Bold 20";

    markup = g_strdup_printf ("<span font='%s' foreground='%s'>%s</span>",
                               font, color, buff);

    if (out)
       free (out);
    g_error_free (error);
  }
  
  gtk_label_set_markup (GTK_LABEL (label), markup);

  g_free (markup);

  return G_SOURCE_CONTINUE;
}

void destroy_handler (GtkApplication* app,
                      gpointer data)
{
  g_application_quit(G_APPLICATION (data));
}

static void
toggle_lock_position (GtkWidget *menu_item, gpointer user_data)
{
  lock = !lock;
}

static gboolean
key_handler (GtkWidget *w,
             GdkEventKey *ev,
             gpointer data)
{
  GtkApplication *app = GTK_APPLICATION (data);
  switch (ev->keyval)
    {
    case GDK_KEY_Escape:
        destroy_handler(NULL, app);
        return TRUE;
    }
  return FALSE;
}

static gboolean
button_handler (GtkWidget *window,
                GdkEventButton *ev,
                gpointer data)
{
  if ((ev->button == 1) && (lock == FALSE))
    {
      gtk_window_begin_move_drag (GTK_WINDOW(window),
          ev->button,
          ev->x_root,
          ev->y_root,
          ev->time);
      return TRUE;
    }
  if (ev->button == 3)
    {
      GtkApplication *app = GTK_APPLICATION (data);
      GtkWidget *toggle_lock, *exit, *popup_menu;

      popup_menu = gtk_menu_new ();
      gtk_menu_set_reserve_toggle_size (GTK_MENU (popup_menu), FALSE);
      if (lock == TRUE)
          toggle_lock = gtk_menu_item_new_with_label ("Unlock");
      else
          toggle_lock = gtk_menu_item_new_with_label ("Lock");
      gtk_widget_show (toggle_lock);
      exit = gtk_menu_item_new_with_label ("Exit");
      gtk_widget_show (exit);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), toggle_lock);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), exit);
      g_signal_connect (G_OBJECT (toggle_lock), "activate",
                        G_CALLBACK (toggle_lock_position), NULL);
      g_signal_connect (G_OBJECT (exit), "activate",
                        G_CALLBACK (destroy_handler), app);
      gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), NULL);
      return TRUE;
    }
  return FALSE;
}

static void
geometry_hints (GtkWidget *widget,
                    gpointer data)
{
  GdkGeometry         hints;

  hints.min_width = 0;
  hints.max_width = gtk_widget_get_allocated_width (widget);
  hints.min_height = 0;
  hints.max_height = gtk_widget_get_allocated_height (widget);

  gtk_window_set_geometry_hints(
        GTK_WINDOW (widget),
        widget,
        &hints,
        (GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));
}

static void
draw_handler (GtkWidget *widget,
              cairo_t *cr,
              gpointer user_data)
{
  float alpha;
  float alpha_border;
  if (opacity) {
      alpha = alpha_border = (float) opacity / 100;
  }
  else {
      alpha = color_bg.alpha;
      alpha_border = color_border.alpha;
  }
  
  cairo_set_source_rgba (cr, color_bg.red, color_bg.green, color_bg.blue,  alpha);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE); 
  cairo_paint(cr);

  if (border) {
     if (border_color == NULL) {
         cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.7);
     }
     else {
         cairo_set_source_rgba (cr, color_border.red, color_border.green, color_border.blue,  alpha_border);
      }
      cairo_rectangle (cr, border / 2, border / 2, gtk_widget_get_allocated_width (widget) - border,
                                                gtk_widget_get_allocated_height (widget) - border);
      cairo_set_line_width (cr, border);
      cairo_set_line_join( cr, CAIRO_LINE_JOIN_ROUND); 
      cairo_stroke (cr); 
  }
}

static void
composited_changed (
  GtkWidget *widget,
  gpointer user_data)
{
  GdkScreen           *screen;
  GdkVisual           *visual;

  screen = gdk_screen_get_default ();
  visual = gdk_screen_get_rgba_visual (screen);
  if (visual != NULL && gdk_screen_is_composited (screen))
      gtk_widget_set_visual (widget, visual);
}

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget           *window;
  GtkWidget           *grid;
  GtkWidget           *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Simple Clock");
  gtk_window_set_default_size (GTK_WINDOW (window), 10, 10);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
  gtk_widget_set_app_paintable (window, TRUE);
  composited_changed (window, NULL);
  if (center)
      gtk_window_set_position (GTK_WINDOW (window),
                               GTK_WIN_POS_CENTER_ALWAYS);
  if (above) {
      gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  }
  else {
      gtk_window_set_keep_below (GTK_WINDOW (window), TRUE);
  }
  if (stick)
      gtk_window_stick (GTK_WINDOW (window));
  grid = gtk_grid_new ();
  gtk_grid_set_column_homogeneous (GTK_GRID (grid),
                                TRUE);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid),
                                FALSE);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);

  label = gtk_label_new (NULL);
  g_timeout_add (1000, on_timeout, label);
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (destroy_handler), app);
  g_signal_connect (G_OBJECT (window), "key-press-event",
                    G_CALLBACK (key_handler), app);
  g_signal_connect (G_OBJECT (window), "button-press-event",
                    G_CALLBACK (button_handler), app);
  g_signal_connect (G_OBJECT (window), "size-allocate",
                    G_CALLBACK (geometry_hints), NULL);
  g_signal_connect (G_OBJECT (window), "draw",
                    G_CALLBACK (draw_handler), NULL);
  g_signal_connect (G_OBJECT (window), "composited-changed",
                    G_CALLBACK (composited_changed), NULL);
  gtk_widget_show_all (window);

  if ((posx != 0) || (posy != 0))
     gtk_window_move (GTK_WINDOW (window), posx, posy);
}

static int
get_options (int argc,
             char *argv[])
{
  static
    GOptionEntry option_entries[] = {
    { "bgcolor",       0, 0, G_OPTION_ARG_STRING,  &bgcolor,
       "Specify window background color, black, #000, rgb(r,g,b), rgba(r,g,b,a)", "COLOR" },
    { "border",  0, 0, G_OPTION_ARG_INT,   &border,
       "Draw window border, same as bgcolor", "COLOR" },
    { "border-color",  0, 0, G_OPTION_ARG_STRING,   &border_color,
       "Set border color", "COLOR" },
    { "center",  0, 0, G_OPTION_ARG_NONE,   &center,
       "Place window at the center of the screen", NULL },
    { "color",       0, 0, G_OPTION_ARG_STRING,  &color,
       "Specify color of the label, white, #FFFFFF", "COLOR" },
    { "date",        0, 0, G_OPTION_ARG_STRING,  &date,
       "Specify display time, e.g. %a %b %d/%m/%Y %H:%M:%S but not 'now", "STRING" },
    { "font",        0, 0, G_OPTION_ARG_STRING,  &font,
       "Specify font to use", "FONT" },
    { "keep-above",  0, 0, G_OPTION_ARG_NONE,   &above,
       "Keep window above, default is to keep window below", NULL },
    { "lock",  0, 0, G_OPTION_ARG_NONE,   &lock,
       "Lock position", NULL },
    { "opacity",  0, 0, G_OPTION_ARG_INT,   &opacity,
       "Set window opacity, compositing must be enabled first", "INT" },
    { "posx",        0, 0, G_OPTION_ARG_INT,    &posx,
       "Specify X position to use", "INT" },
    { "posy",        0, 0, G_OPTION_ARG_INT,   &posy,
       "Specify Y position to use", "INT" },
    { "stick",  0, 0, G_OPTION_ARG_NONE,   &stick,
       "Show window on all desktops", NULL },
    { NULL }
  };
              
  GOptionContext *option_context;
  GError *error = NULL;

#if (!GLIB_CHECK_VERSION (2, 36, 0))
  g_type_init ();
#endif

  option_context = g_option_context_new (NULL);
  g_option_context_add_main_entries (option_context,
                                     option_entries,
                                     NULL);
  if (g_option_context_parse (option_context,
                              &argc,
                              &argv,
                              &error) == FALSE) {
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    return 1;
  }
  if (bgcolor != NULL) {
     if (gdk_rgba_parse (&color_bg, bgcolor) == FALSE) {
         g_printerr ("Unknown color: %s\n", bgcolor);
         return 1;
     }
  }
  if (border_color != NULL) {
      if (gdk_rgba_parse (&color_border, border_color) == FALSE) {
          g_printerr ("Unknown color: %s\n", border_color);
          return 1;
      }
  }
  if (opacity < 0 || opacity > 100) {
      g_printerr ("Opacity must be between 0 and 100: %d\n", opacity);
      return 1;
  }
  if (color == NULL)
      color = "red";
  if (date == NULL)
      date = "%r";
  if (font == NULL)
      font = "Sans Bold 20";
  g_option_context_free (option_context);

  return 0;
}

int
main (int argc,
      char **argv)
{
  int status;
  GtkApplication *app;
  gboolean opts;

  opts = get_options (argc, argv);

  if (opts)
     return 1;

  app = gtk_application_new ("org.gtk.simple_clock",
                             G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
