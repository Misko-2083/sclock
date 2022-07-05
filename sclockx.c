#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <sys/select.h>

/*  gcc sclockx.c -o sclockx $(pkg-config --cflags --libs cairo-xlib-xrender x11)  */

struct xwindow {
  Display *disp;
  Window w;
  XVisualInfo vinfo;
  cairo_t *cr;
  cairo_t *bg;
  cairo_surface_t *img;
  cairo_surface_t *ximg;
};

struct options {
  int pos_x;
  int pos_y;
  int above;
  int splash;
  int start;
  char date;
};

struct colors {
    int r;
    int g;
    int b;
    int set;
};

static const char sclockx_usage[] =
"Usage: sclockx [options]\n\n"
"  -h, --help        Show help message and quit\n"
"  --above           Keep clock above other windows, default is below\n"
"  --color=          Set text color, e.g. fcfcfc, this option can be set multiple times\n"
"  --posx=           Set X position\n"
"  --posy=           Set Y position\n"
"  --splash          Set window type to splash\n"
"\n";

static void
draw (cairo_t *g, int mode,
      char *date, struct colors *col);

static void
set_source_color (cairo_t *g,
                  struct colors *col,
                  int color_index);

static void
x11_create_window (struct xwindow *ctx);

static void
x11_move_window (struct xwindow *ctx,
                 struct options *opt);
static void
set_cairo (struct xwindow *ctx);

static void
main_loop (struct xwindow *ctx,
           struct colors *col);

static void
set_hints (struct xwindow *ctx,
           struct options *opt);

static char *
set_above (int above);

static void usage (void);

int main (int argc, char **argv)
{
  struct colors col[4] = { {0, 255, 0, 1} }; 
  struct options opt = { 0 };
  /* initial values */
  opt.pos_x = -1;
  opt.pos_y = -1;
  opt.splash = 0;
  opt.start = 1;

  int c;
  int index = 0;
  static struct option long_options[] = {
      {"help",    no_argument,        0, 'h'},
      {"above",   no_argument,        0,  2 },
      {"color",   required_argument,  0,  0 },
      {"posx",    required_argument,  0, 'x'},
      {"posy",    required_argument,  0, 'y'},
      {"splash",  no_argument,        0,  1 },
      {"start",  required_argument,   0, 's'},
      {0,         0,                  0,  0 }
  };

  while ((c = getopt_long(argc, argv, "hx:y:0:12",
                          long_options, NULL)) != -1) {
      switch (c) {
          case 'x':
              if (atoi(optarg) > -1)
                  opt.pos_x = atoi(optarg);
              break;
          case 'y':
              if (atoi(optarg) > -1)
                  opt.pos_y = atoi(optarg);
              break;
          case 0:
              sscanf(optarg, "%02x%02x%02x", &col[index].r, &col[index].g, &col[index].b);
              if ((col[index].r > 255) || (col[index].r < 0) || 
                  (col[index].g > 255) || (col[index].g < 0) ||
                  (col[index].b > 255) || (col[index].b < 0)) {
                  printf("Invalid color %s\n", optarg);
                  usage();
              }
              sscanf("1", "%d", &col[index].set);
              index++;
              if (index > 4)
                  usage();
              break;
          case 1:
              opt.splash = 1;
              break;
          case 2:
              opt.above = 1;
              break;
          case 'h':
          default:
              usage();
      }
  }

  if (optind < argc)
      usage();

  struct xwindow ctx = { 0 };

  ctx.disp = XOpenDisplay(NULL);

  if (!ctx.disp) {
      printf("Display open failed.\n");
      return 1;
  }

  x11_create_window (&ctx);
  set_cairo (&ctx);
  set_hints (&ctx, &opt);

  XMapWindow(ctx.disp, ctx.w);
  x11_move_window (&ctx, &opt);
  XSync (ctx.disp, False);

  main_loop (&ctx, col);

  cairo_destroy (ctx.cr);
  cairo_surface_destroy (ctx.ximg);
  cairo_destroy (ctx.bg);
  cairo_surface_destroy(ctx.img);
  XDestroyWindow (ctx.disp, ctx.w);
  XCloseDisplay (ctx.disp);
  return 0;
}

static void
x11_create_window (struct xwindow *ctx)
{
  const char depths[] = { 32, 24, 8, 4, 2, 1, 0 };
  for (int i = 0; depths[i]; i++) {
      XMatchVisualInfo (ctx->disp,
                        DefaultScreen (ctx->disp),
                        32, TrueColor, &ctx->vinfo);
      if (ctx->vinfo.visual)
          break;
  }
  XSetWindowAttributes attr;
  attr.override_redirect = False;
  attr.event_mask =  KeyPressMask | ButtonPressMask;
  attr.colormap = XCreateColormap(ctx->disp,
                                  DefaultRootWindow (ctx->disp),
                                  ctx->vinfo.visual, AllocNone);
  attr.border_pixel = 0;
  attr.background_pixel = 0;
  ctx->w =  XCreateWindow(ctx->disp, DefaultRootWindow (ctx->disp), 0, 0, 180, 26, 0,
                         ctx->vinfo.depth, InputOutput, ctx->vinfo.visual,
                         CWEventMask | CWColormap | CWBorderPixel | CWBackPixel , &attr);
  XStoreName(ctx->disp, ctx->w, "sclockx");
  XSelectInput(ctx->disp, ctx->w, 
               KeyPressMask | ClientMessage | 
               ButtonPressMask | ButtonReleaseMask);
}

static void
x11_move_window (struct xwindow *ctx,
                 struct options *opt)
{
  if ((opt->pos_x > -1) && (opt->pos_y > -1))
       XMoveWindow (ctx->disp, ctx->w, opt->pos_x, opt->pos_y);
  if ((opt->pos_x > -1) && !(opt->pos_y > -1))
      XMoveWindow (ctx->disp, ctx->w, opt->pos_x, 0);
  if (!(opt->pos_x > -1) && (opt->pos_y > -1))
      XMoveWindow (ctx->disp, ctx->w, 0, opt->pos_y);
}

static void
set_cairo (struct xwindow *ctx)
{
  ctx->img = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 180, 26);
  ctx->bg = cairo_create (ctx->img);
  ctx->ximg = cairo_xlib_surface_create (ctx->disp, ctx->w,
                                    ctx->vinfo.visual, 180, 26);
  cairo_xlib_surface_set_size (ctx->ximg, 180, 26);
  ctx->cr = cairo_create (ctx->ximg);
  cairo_set_source_surface (ctx->cr, ctx->img, 0, 0);
}

static void
main_loop (struct xwindow *ctx,
           struct colors *col)
{
  int x11_fd;
  fd_set in_fds;
  /* getopts opens some fds so next needs to be x11_fd +4; 
     It's usually +1
  */
  struct timeval tv = { 0 };
  XEvent ev;
  x11_fd = ConnectionNumber (ctx->disp);
  int nfds = x11_fd + 4; 
  static char *date = "%r";
  Atom wm_delete_window = XInternAtom (ctx->disp, "WM_DELETE_WINDOW", False);
  XSetWMProtocols (ctx->disp, ctx->w, &wm_delete_window, 1);
  static int mode = 0;
  for (;;) {
      FD_ZERO(&in_fds);
      FD_SET(x11_fd, &in_fds);
      // Set timer to one second
      tv.tv_usec = 0;
      tv.tv_sec = 1;

      // Wait for X Event or a Timer
      int ret;
      if (!XPending(ctx->disp))
          ret = select (nfds, &in_fds, NULL, NULL, &tv);
      if (ret == -1 && errno == EINTR)
          continue;
      if (ret == -1)
          goto shutdown;
      if (ret == 0)
          draw(ctx->cr, mode, date, col);

      if (XPending (ctx->disp)) {
          XNextEvent (ctx->disp, &ev);
          if (XFilterEvent (&ev, ctx->w))
              continue;
          switch (ev.type) {
              case ButtonPress:
                  switch(ev.xbutton.button){
                      case Button1:
                      mode = mode == 3 ? 0 : mode + 1;
                      draw(ctx->cr, mode, date, col);
                      continue;
                      case Button3:
                           goto shutdown;
                      default:
                          break;
              }
              case ClientMessage:
                  if (ev.xclient.data.l[0] == wm_delete_window) 
                      goto shutdown;
          }
      }
      if (ret > 1)
          printf("An error occured!%d\n", ret);
  }
  shutdown: printf("Exit\n");
}

static void
draw (cairo_t *g, int mode,
      char *date, struct colors *col)
{
  char buffer[64];
  time_t timer;
  const char *fmt[] = {"%r", "%R:%S", "%a %d %b", "      %Y"};

  struct tm* tm_info;
  time (&timer);
  tm_info = localtime (&timer);
  strftime (buffer, 64, fmt[mode], tm_info);

  cairo_save (g);
  cairo_set_source_rgba (g,0,0,0,1);
  cairo_set_operator (g, CAIRO_OPERATOR_CLEAR);
  cairo_paint (g);
  cairo_restore (g);

  cairo_set_operator (g, CAIRO_OPERATOR_OVER);
  cairo_save (g);
  set_source_color(g, col, mode);
  cairo_select_font_face(g,"Sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(g,24);
  cairo_move_to (g, 0.0, 20.0);
  cairo_show_text(g,buffer);
  cairo_restore (g);
  cairo_paint (g);
}

static void
set_source_color (cairo_t *g, struct colors *col, int color_index)
{

 if (col[color_index].set != 1)
     color_index = 0;
 cairo_set_source_rgb (g, (float) col[color_index].r / 255,
                          (float) col[color_index].g / 255,
                          (float) col[color_index].b / 255);
}

static void
set_hints (struct xwindow *ctx,
           struct options *opt)
{
  XSizeHints sh;
  sh.width = sh.min_width = 150;
  sh.height = sh.min_height = 24;
  sh.max_width = 220;
  sh.max_height = 40;
  sh.win_gravity = 0;
  sh.flags = PSize | PMinSize | PMaxSize | PWinGravity;
  XSetWMNormalHints (ctx->disp, ctx->w, &sh);

  Atom motif_hints;
  long hints[5] = {2, 0, 0, 0, 0};
  motif_hints = XInternAtom(ctx->disp, "_MOTIF_WM_HINTS", False);
  XChangeProperty (ctx->disp, ctx->w, motif_hints, motif_hints, 32,
                   PropModeReplace, (unsigned char *)&hints, 5);

  Atom window_state = XInternAtom (ctx->disp, "_NET_WM_STATE", False);

  long wmStateBelow = XInternAtom (ctx->disp, set_above(opt->above), False);
  long wmStateSticky = XInternAtom (ctx->disp, "_NET_WM_STATE_STICKY", False);
  long wmStateSkipTaskbar = XInternAtom (ctx->disp, "_NET_WM_STATE_SKIP_TASKBAR", False);
  long wmStateSkipPager = XInternAtom (ctx->disp, "_NET_WM_STATE_SKIP_PAGER", False);
  XChangeProperty (ctx->disp, ctx->w, window_state, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *) &wmStateBelow, 1);
  XChangeProperty (ctx->disp, ctx->w, window_state, XA_ATOM, 32,
                   PropModeAppend, (unsigned char *) &wmStateSticky, 1);
  XChangeProperty (ctx->disp, ctx->w, window_state, XA_ATOM, 32,
                   PropModeAppend, (unsigned char *) &wmStateSkipTaskbar, 1);
  XChangeProperty (ctx->disp, ctx->w, window_state, XA_ATOM, 32,
                   PropModeAppend, (unsigned char *) &wmStateSkipPager, 1);

  if (opt->splash) {
      Atom window_type = XInternAtom (ctx->disp, "_NET_WM_WINDOW_TYPE", False);
      long wmWindowTypeSplash = XInternAtom (ctx->disp, "_NET_WM_WINDOW_TYPE_SPLASH", False);
      XChangeProperty (ctx->disp, ctx->w, window_type, XA_ATOM, 32,
                       PropModeAppend, (unsigned char *) &wmWindowTypeSplash, 1);
  }
}

static char *
set_above (int above)
{
  static char *ret;
  if (above)
      ret = "_NET_WM_STATE_ABOVE";
  else
      ret = "_NET_WM_STATE_BELOW";
  return ret;
}

static void
usage(void)
{
	printf("%s", sclockx_usage);
	exit(0);
}
