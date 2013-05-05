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
#include "gstcamcontrol_visca.c"
#include "gstcamcontrol.c"

static void
run (GstCamController * ctrl, const char *name)
{
  if (!gst_cam_controller_open (ctrl, name)) {
    fprintf (stderr, "can't open: %s", name);
    return;
  }

  for (int n = 0, x = 0, y = 0; n < 100; ++n) {
    x += 10;
    y += 10;
    if (gst_cam_controller_move (ctrl, x, y)) {
      fprintf (stderr, "can't move: (%d, %d)", x, y);
    }
    usleep (100000);
  }

  gst_cam_controller_close (ctrl);
}

static int
run_visca (const char *name)
{
  GstCamControllerVisca *visca =
      GST_CAM_CONTROLLER_VISCA (g_object_new (GST_TYPE_CAM_CONTROLLER_VISCA,
          NULL));

  run ((GstCamController *) visca, name);

  g_object_unref (visca);
  return 0;
}

int
main (int argc, char **argv)
{
  if (argc < 2) {
    fprintf (stderr, "bad arguments");
    return -1;
  }
  return run_visca (argv[1]);
}
