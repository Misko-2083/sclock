#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

/*  gcc sclockx.c -o sclockx $(pkg-config --cflags --libs cairo-xlib-xrender x11)*/

void draw(cairo_t *g, char *date);

int main (int argc, char *argv[]) {
  Display *disp = XOpenDisplay(NULL);
  if(disp == NULL) {
      printf("Display open failed.\n");
      return 1;
  }
  cairo_t *g, *bg;
  cairo_surface_t *ximg,*img;
  int s;

  img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,180,26);
  bg = cairo_create(img);
  s = DefaultScreen(disp);

  XVisualInfo vinfo;
  XMatchVisualInfo(disp, DefaultScreen(disp), 32, TrueColor, &vinfo);

  XSetWindowAttributes attr;
  attr.override_redirect = False;
  attr.event_mask = ExposureMask | KeyPressMask |
                    VisibilityChangeMask | ButtonPressMask;
  attr.colormap = XCreateColormap(disp, DefaultRootWindow(disp), vinfo.visual, AllocNone);
  attr.border_pixel = 0;
  attr.background_pixel = 0;

  int w =  XCreateWindow(disp, DefaultRootWindow(disp), 0, 0, 180, 26, 0,
                         vinfo.depth, InputOutput, vinfo.visual,
                         CWEventMask | CWColormap | CWBorderPixel | CWBackPixel , &attr);
  XStoreName(disp, w, "sclockx");
  XSelectInput(disp, w, 
               ExposureMask | KeyPressMask | ClientMessage | 
               ButtonPressMask | ButtonReleaseMask);
  XSizeHints sh;
  sh.width = sh.min_width = 150;
  sh.height = sh.min_height = 24;
  sh.max_width = 220;
  sh.max_height = 40;
  sh.win_gravity = 0;
  sh.flags = PSize | PMinSize | PMaxSize | PWinGravity;
  XSetWMNormalHints(disp, w, &sh);

  Atom motif_hints;
  long hints[5] = {2, 0, 0, 0, 0};
  motif_hints = XInternAtom(disp, "_MOTIF_WM_HINTS", False);
  XChangeProperty(disp, w, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)&hints, 5);

  Atom window_state = XInternAtom(disp, "_NET_WM_STATE", False);
  long wmStateBelow = XInternAtom(disp, "_NET_WM_STATE_BELOW", False);
  long wmStateSticky = XInternAtom(disp, "_NET_WM_STATE_STICKY", False);
  long wmStateSkipTaskbar = XInternAtom(disp, "_NET_WM_STATE_SKIP_TASKBAR", False);
  long wmStateSkipPager = XInternAtom(disp, "_NET_WM_STATE_SKIP_PAGER", False);
  XChangeProperty(disp, w, window_state, XA_ATOM, 32, PropModeReplace, (unsigned char *) &wmStateBelow, 1);
  XChangeProperty(disp, w, window_state, XA_ATOM, 32, PropModeAppend, (unsigned char *) &wmStateSticky, 1);
  XChangeProperty(disp, w, window_state, XA_ATOM, 32, PropModeAppend, (unsigned char *) &wmStateSkipTaskbar, 1);
  XChangeProperty(disp, w, window_state, XA_ATOM, 32, PropModeAppend, (unsigned char *) &wmStateSkipPager, 1);

  XFlush(disp);
  XMapWindow(disp,w);


  Atom wm_delete_window = XInternAtom(disp,"WM_DELETE_WINDOW",False);
  XSetWMProtocols(disp,w,&wm_delete_window,1);

  ximg = cairo_xlib_surface_create(disp,w,vinfo.visual,180,26);
  cairo_xlib_surface_set_size(ximg,180,26);
  g = cairo_create(ximg);
  cairo_set_source_surface(g,img,0,0);

  int x11_fd;
  fd_set in_fds;
  int nfds = x11_fd + 1;
  struct timeval tv;
  XEvent ev;
  x11_fd = ConnectionNumber(disp);
  static char *date = "%r";
  for(;;) {
      FD_ZERO(&in_fds);
      FD_SET(x11_fd, &in_fds);
      // Set timer to one second
      tv.tv_usec = 0;
      tv.tv_sec = 1;
      // Wait for X Event or a Timer
      int ret;
      if (!XPending(disp))
          ret = select(nfds, &in_fds, NULL, NULL, &tv);
      if (ret == -1 && errno == EINTR)
          continue;
      if (ret == -1)
          goto shutdown;
      if (ret == 0)
          draw(g, date);

      if (XPending(disp)) {
          XNextEvent(disp, &ev);
          if (XFilterEvent(&ev, w))
              continue;
          if (ret > 0) {
              switch (ev.type) {
              case ClientMessage:
              if (ev.xclient.data.l[0] == wm_delete_window) 
                  goto shutdown;
                  case ButtonPress:
                      switch(ev.xbutton.button){
                      case Button1:
                      if (date == "%r")
                          date = "%R:%S";
                      else if (date == "%R:%S")
                          date = "%a %d %b";
                      else if (date == "%a %d %b")
                          date = "      %Y";
                      else
                          date = "%r";
                      continue;
                      case Button3:
                           goto shutdown;
                      }
              default:
                  break;
              }
          }
          else if (ret == 0) 
              draw(g, date);
          else
              printf("An error occured!\n");
      }
  }
  shutdown: printf("Exit\n");
  cairo_destroy(g);
  cairo_surface_destroy(ximg);
  cairo_destroy(bg);
  cairo_surface_destroy(img);
  XDestroyWindow(disp,w);
  XCloseDisplay(disp);
  return 1;
}


void draw(cairo_t *g, char *date) {
  char buffer[64];
  time_t timer;

  struct tm* tm_info;
  time (&timer);
  tm_info = localtime (&timer);
  strftime (buffer, 64, date, tm_info);

  cairo_save (g);
  cairo_set_source_rgba (g,0,0,0,1);
  cairo_set_operator (g, CAIRO_OPERATOR_CLEAR);
  cairo_paint (g);
  cairo_restore (g);
  cairo_set_operator (g, CAIRO_OPERATOR_OVER);
  cairo_save (g);

  if (date == "%r")
      cairo_set_source_rgb (g, 0.0, 1.0, 0.0);
  else if (date == "%R:%S")
      cairo_set_source_rgb (g, 1.0, 0.0, 0.0);
  else if (date == "%a %d %b")
      cairo_set_source_rgb (g, 1.0, 1.0, 1.0);
  else
      cairo_set_source_rgb (g, 0.9, 0.7, 0.0);

  cairo_select_font_face(g,"Sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(g,24);
  cairo_move_to (g, 0.0, 20.0);
  cairo_show_text(g,buffer);
  cairo_restore (g);
  cairo_paint (g);
}
