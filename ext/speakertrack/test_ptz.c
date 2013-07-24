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
    fprintf (stderr, "try: ./ptz /dev/ttyUSB0 \n");
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

  usleep (5000000);

  // pan left (degrees per seconds, degrees)
  gst_cam_controller_pan (ctrl, /*0.3, 0.25 */ 50, -30);
  usleep (5000000);

  // pan center
  gst_cam_controller_pan (ctrl, /*0.9, 0.5 */ 90, 0);
  usleep (5000000);

  // pan right
  gst_cam_controller_pan (ctrl, /*0.8, 1 - 0.25 */ 50, 30);
  usleep (5000000);

  // pan center
  gst_cam_controller_pan (ctrl, /*0.7, 0.5 */ 90, 0);
  usleep (5000000);

  // tilt up
  gst_cam_controller_tilt (ctrl, /*0.8, 0.25 */ 50, 45);
  usleep (5000000);
  gst_cam_controller_tilt (ctrl, /*0.8, 0.5 */ 90, 0);
  usleep (5000000);

  // tilt down
  gst_cam_controller_tilt (ctrl, /*0.8, 1-0.25 */ 50, -45);
  usleep (5000000);
  gst_cam_controller_tilt (ctrl, /*0.8, 0.5 */ 90, 0);
  usleep (5000000);

  // move
  gst_cam_controller_move (ctrl, 20, ctrl->pan_min, 50, ctrl->tilt_min);
  usleep (5000000);
  gst_cam_controller_move (ctrl, 100, 0, 100, 0);
  usleep (5000000);
  gst_cam_controller_move (ctrl, 20, ctrl->pan_max, 20, ctrl->tilt_max);
  usleep (5000000);
  gst_cam_controller_move (ctrl, 100, 0, 100, 0);
  usleep (5000000);

  gst_cam_controller_close (ctrl);
}

static int
run_canon (const char *name)
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
    fprintf (stderr, "try: %s /dev/ttyUSB0 \n", argv[0]);
    return -1;
  }
#if GLIB_MAJOR_VERSION <= 2 && GLIB_MINOR_VERSION <= 3 && GLIB_MICRO_VERSION <= 6
  g_type_init ();
#endif
  return run_canon (argv[1]);
}
