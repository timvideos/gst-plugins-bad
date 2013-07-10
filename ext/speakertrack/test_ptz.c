#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "gstcamcontrol_pana.c"
#include "gstcamcontrol_canon.c"
#include "gstcamcontrol_sony.c"
#include "gstcamcontrol.c"

static void
run (GstCamController * ctrl, const char *name)
{
  if (!gst_cam_controller_open (ctrl, name)) {
    fprintf (stderr, "can't open: %s\n", name);
    return;
  }
  //gst_cam_controller_move (ctrl, 0.8, x, y);

#if 0
  for (int n = 0; n < 100; ++n) {
    double x = sin ((double) n);
    double y = cos ((double) n);
    if (gst_cam_controller_move (ctrl, 0.8, x, y) != TRUE) {
      fprintf (stderr, "can't move: (%lf, %lf)\n", x, y);
    }
    usleep (100000);
  }
#endif

  gst_cam_controller_pan (ctrl, 0.8, -1);
  usleep (3000000);
  gst_cam_controller_pan (ctrl, 0.8, 0);
  usleep (100000);

  gst_cam_controller_pan (ctrl, 0.8, 1);
  usleep (3000000);
  gst_cam_controller_pan (ctrl, 0.8, 0);
  usleep (100000);

  gst_cam_controller_tilt (ctrl, 0.8, -1);
  usleep (3000000);
  gst_cam_controller_tilt (ctrl, 0.8, 0);
  usleep (100000);

  gst_cam_controller_tilt (ctrl, 0.8, 1);
  usleep (3000000);
  gst_cam_controller_tilt (ctrl, 0.8, 0);
  usleep (100000);

  gst_cam_controller_close (ctrl);
}

static int
run_visca (const char *name)
{
  GstCamControllerCanon *visca =
      GST_CAM_CONTROLLER_CANON (g_object_new (GST_TYPE_CAM_CONTROLLER_CANON,
          NULL));

  run ((GstCamController *) visca, name);

  g_object_unref (visca);
  return 0;
}

int
main (int argc, char **argv)
{
  if (argc < 2) {
    fprintf (stderr, "bad arguments\n");
    return -1;
  }
#if GLIB_MAJOR_VERSION <= 2 && GLIB_MINOR_VERSION <= 3 && GLIB_MICRO_VERSION <= 2
  g_type_init ();
#endif
  return run_visca (argv[1]);
}
