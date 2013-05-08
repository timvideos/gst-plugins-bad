/*
Visca Driver for the Sony EVI-D30 (NSTC) and EVI-D31 (PAL) Cameras
Copyright (C) 2004 Kyle Kakligian small_art@hotmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cameraimp.h"
#include "connectionimp.h"
#include "visca.h"
#include "commands.h"

#define MAXTRIES 3

struct _SonyVisca
{
    ViscaConnection *visca;
    ViscaCamera camera;
};

void ViscaConnectionImp::init(const char *device) {
  int tries,devfd;
  ViscaCommand vs;

  // on error, we return early and don't want a valid fd
  fd = -1;

  fprintf(stderr,"Initializing Visca with device \"%s\"\n",device);

  // cameras are numbered 1 through 7, and 8 is the broadcast camera
  for(int cam=0; cam<MAX_VISCA_CAMERAS+1; cam++) {
    cameras[cam].pimpl->self=cam+1;
    cameras[cam].pimpl->connection=this;
  }

  // open port
  devfd=open(device,O_RDWR);
  if( devfd<0 ) {
    fprintf(stderr,"Visca ERROR: Couldn't open \"%s\"\n", device);
    return;
  }

  // Check if device is a terminal
  struct termios termData;
  if(isatty(devfd)==0) {
    fprintf(stderr,"Visca ERROR: \"%s\" is not a terminal.\n", device);
    close(devfd);
    return;
  }
  // Read data for terminal assc. to fd.
  if(tcgetattr(devfd, &termData) < 0) {
    fprintf(stderr, "Visca ERROR: tcgetattr.\n");
    close(devfd);
    return;
  }
  // Set up connection protocol.
  cfmakeraw(&termData); 
  termData.c_cc[VMIN] = 0;
  termData.c_cc[VTIME] = 1;
  if(cfsetispeed(&termData, B9600) < 0) {
    fprintf(stderr, "Visca ERROR: tcsetispeed.\n");
    close(devfd);
    return;
  }
  if(cfsetospeed(&termData, B9600) < 0) {
    fprintf(stderr, "Visca ERROR: tcsetospeed.\n");
    close(devfd);
    return;
  }
  if(tcsetattr(devfd, TCSANOW, &termData) < 0 ) {
    fprintf(stderr, "Visca ERROR: tcsetattr.\n");
    close(devfd);
    return;
  }

  // Assign addresses to cameras
  for(tries=0; tries<MAXTRIES; ++tries) {
    if( !Send(devfd,&ViscaCommandAddressSet) ) {
      fprintf(stderr, "Visca ERROR: sending AddressSet command.\n");
      close(devfd);
      return;
    }

    // Wait for an answer for 2 sec max
    ViscaCommand response;
    if( !Get(devfd,2, &response) ) {
      fprintf(stderr, "Visca ERROR: getting AddressSet response.\n");
      close(devfd);
      return;
    }

    // Check for the right answer
    if( response.pos[0]==0x88 && response.pos[1]==0x30) {
      cameraCount = response.pos[2]-1;
      break;
    }
  }
  if(tries==MAXTRIES) {
    fprintf(stderr, "Visca ERROR: AddressSet command did not receive a response.\n");
    close(devfd);
    return;
  }

  // Clear the cameras' interfaces
  for(tries=0; tries<MAXTRIES; ++tries) {
    if( !Send(devfd,&ViscaCommandIF_Clear) ) {
      fprintf(stderr, "Visca ERROR in sending IF_Clear command.\n");
      close(devfd);
      return;
    }

    // Wait for an answer for 5 sec max
    ViscaCommand response;
    if( !Get(devfd,5, &response) ) {
      fprintf(stderr, "Visca ERROR: getting IF-Clear response.\n");
      close(devfd);
      return;
    }
    // check for the right answer
    if( response.pos[0] == 0x88 &&
        response.pos[1] == 0x01 &&
        response.pos[2] == 0x00 &&
        response.pos[3] == 0x01)
      break;
  }
  if(tries==MAXTRIES) {
    fprintf(stderr, "Visca ERROR: AddressSet command did not get a response.\n");
    close(devfd);
    return;
  }

  // setup the fd variable
  fd = devfd;

  int camera;
  // send the cameras to their home position, and then reset them
  // (broadcasting seems not to ack/fin in these cases)
  for(camera=1;camera<=cameraCount;camera++) {
    vs = ViscaCommandPanTiltHome;
    vs.pos[0]=0x80 | camera&0xf;
    if( !SendFin(&vs) ) {
      fprintf(stderr, "Visca ERROR in sending PanTiltHome command to camera [%d].\n",camera);
      close(devfd);
      fd=-1;
      return;
    }
    
    vs = ViscaCommandPanTiltReset;
    vs.pos[0]=0x80 | camera&0xf;
    if( !SendFin(&vs) ) {
      fprintf(stderr, "Visca ERROR in sending PanTiltReset command to camera [%d].\n",camera);
      close(devfd);
      fd=-1;
      return;
    }

    // get the zoom value
    cameras[camera-1].GetZoom(& cameras[camera-1].pimpl->zoom);
  }

  cameras[MAX_VISCA_CAMERAS].pimpl->sync=false;

  fprintf(stderr,"Initializing Visca done. %d camera(s)\n",cameraCount);
}

extern "C" {

SonyVisca* sony_visca_new() {
    SonyVisca *sony = new SonyVisca;
    sony->visca = NULL;
    return sony;
}
void sony_visca_free(SonyVisca *sony) { return delete sony; }

void sony_visca_open(SonyVisca *sony, const char *device)
{
    if (sony->visca) {
	delete sony->visca;
    }

    sony->visca = new ViscaConnection(device);
    sony->camera = sony->visca->GetCamera(0);
}

void sony_visca_close(SonyVisca *sony)
{
    delete sony->visca;
    sony->visca = NULL;
    sony->camera = ViscaCamera();
}

void sony_visca_pan(SonyVisca *sony, double x)
{
    if (sony->visca) {
	sony->camera.SetPan(x);
    }
}

void sony_visca_tilt(SonyVisca *sony, double y)
{
    if (sony->visca) {
	sony->camera.SetTilt(y);
    }
}

void sony_visca_zoom(SonyVisca *sony, double z)
{
    if (sony->visca) {
	sony->camera.SetZoom(z);
    }
}

}

