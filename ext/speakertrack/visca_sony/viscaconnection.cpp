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


#include <stdio.h>
#include <stdlib.h>
#include "visca.h"
#include "connectionimp.h"


ViscaConnection::ViscaConnection(const char *device) : pimpl(new ViscaConnectionImp) {
  pimpl->cameraCount=0;
  pimpl->fd=-1;
  pimpl->init(device);
}

ViscaConnection::ViscaConnection(const ViscaConnection& v) : pimpl(new ViscaConnectionImp) {
  pimpl->cameraCount=v.pimpl->cameraCount;
  pimpl->fd=v.pimpl->fd;
}

ViscaConnection::~ViscaConnection() {
  delete pimpl;
}

const ViscaConnection &ViscaConnection::operator=(const ViscaConnection& v) {
  delete pimpl;
  pimpl = new ViscaConnectionImp;

  pimpl->cameraCount=v.pimpl->cameraCount;
  pimpl->fd=v.pimpl->fd;

  return *this;
}

int ViscaConnection::GetCameraCount() const {
  return pimpl->cameraCount;
}

bool ViscaConnection::isValid() const {
  return 0<pimpl->fd;
}

ViscaCamera ViscaConnection::GetCamera(int camera) const {
  if(camera<0 || pimpl->cameraCount<=camera) {
    fprintf(stderr,"Bad ViscaConnection::GetCamera() argument: \"%d\" is not int the range 0-%d.\n",camera,pimpl->cameraCount-1);
    exit(-1); // if the camera parameter is out of ranger, there is really something wrong.
  }

  return (ViscaCamera)(pimpl->cameras[camera]);
}

ViscaCameraAdv ViscaConnection::GetCameraAdv(int camera) const {
  if(camera<0 || pimpl->cameraCount<=camera) {
    fprintf(stderr,"Bad ViscaConnection::GetCamera() argument: \"%d\" is not int the range 0-%d.\n",camera,pimpl->cameraCount-1);
    exit(-1); // if the camera parameter is out of ranger, there is really something wrong.
  }

  return pimpl->cameras[camera];
}

ViscaCamera ViscaConnection::GetBroadcastCamera() const {
  return pimpl->cameras[MAX_VISCA_CAMERAS];
}
