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
#include "visca.h"
#include "commands.h"
#include "cameraimp.h"
#include "connectionimp.h"

ViscaCamera::ViscaCamera() : pimpl(new ViscaCameraImp) {
  pimpl->self=-1;
  pimpl->connection=0;
  pimpl->sync=true;

  pimpl->pan=INIT_PAN;
  pimpl->tilt=INIT_TILT;
}

ViscaCamera::ViscaCamera(const ViscaCamera& v) : pimpl(new ViscaCameraImp) {
  pimpl->self=v.pimpl->self;
  pimpl->connection=v.pimpl->connection;
  pimpl->sync=v.pimpl->sync;

  pimpl->pan=v.pimpl->pan;
  pimpl->tilt=v.pimpl->tilt;
}

ViscaCamera::~ViscaCamera() {
  delete pimpl;
}

const ViscaCamera &ViscaCamera::operator=(const ViscaCamera &v) {
  delete pimpl;
  pimpl=new ViscaCameraImp;

  pimpl->self=v.pimpl->self;
  pimpl->connection=v.pimpl->connection;
  pimpl->sync=v.pimpl->sync;

  pimpl->pan=v.pimpl->pan;
  pimpl->tilt=v.pimpl->tilt;

  return *this;
}

bool ViscaCamera::SetToSyncMode() {
  pimpl->sync=true;
  pimpl->connection->cameras[pimpl->self-1].pimpl->sync=true;
  return true;
}

bool ViscaCamera::SetToASyncMode() {
  pimpl->sync=false;
  pimpl->connection->cameras[pimpl->self-1].pimpl->sync=false;
  return true;
}

bool ViscaCamera::GetPowerState(bool *ret) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_Power;
  inq.pos[0] = 0x80 | pimpl->self;

  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_Power inquiry.\n");
    return false;
  }

  *ret = (reply.pos[2] & 0x1) == 0;
  return true;
}

bool ViscaCamera::SetPowerState(bool on) {
  ViscaCommand cmd = ViscaCommandCAM_Power;
  cmd.pos[0]=0x80 | pimpl->self;

  if(on) cmd.pos[4]=0x02;
  else   cmd.pos[4]=0x03;

  if( !pimpl->connection->Send(&cmd,pimpl->sync) ) {
    fprintf(stderr, "ERROR in sending CAM_Power command.\n");
    return false;
  }
  return true;
}

bool ViscaCamera::GetPan(double *ret) {
  double temp;
  return GetPanTilt(ret,&temp);
}

bool ViscaCamera::SetPan(double newpan) {
  return SetPanTilt(newpan,pimpl->tilt);
}

bool ViscaCamera::MovePan(double panOffset) {
  return MovePanTilt(panOffset,0.0);
}

bool ViscaCamera::GetTilt(double *ret) {
  double temp;
  return GetPanTilt(&temp,ret);
}

bool ViscaCamera::SetTilt(double newtilt) {
  return SetPanTilt(pimpl->pan,newtilt);
}

bool ViscaCamera::MoveTilt(double tiltOffset) {
  return MovePanTilt(0.0,tiltOffset);
}

bool ViscaCamera::GetZoom(double *ret) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_Zoom;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_Zoom inquiry.\n");
    return false;
  }

  // zoom maps from 0x0000-0x03ff to 0.0 to 1.0
  int value = reply.pos[3]*0x100 + reply.pos[4]*0x10 + reply.pos[5];
  *ret = ((double)value)/1023.0;
  return true;
}

bool ViscaCamera::SetZoom(double value) {
  if(value < 0.0) value=0.0;
  if(1.0 < value) value=1.0;

  pimpl->zoom = value;

  ViscaCommand cmd = ViscaCommandCAM_Zoom;
  cmd.pos[0]= 0x80 | pimpl->self;
  // pimpl->zoom value ranges from 0.0 to 1.0
  // cam's  zoom value ranges from 0x0000 to 0x03ff
  int zoomValue = (int) ( value * 1023.0);

  cmd.pos[4]= 0xf & (zoomValue >> 12);
  cmd.pos[5]= 0xf & (zoomValue >> 8);
  cmd.pos[6]= 0xf & (zoomValue >> 4);
  cmd.pos[7]= 0xf & (zoomValue);

  if( !pimpl->connection->Send(&cmd, pimpl->sync) ) {
    fprintf(stderr, "ERROR in sending CAM_Zoom command.\n");
    return false;
  }
  return true;
}

bool ViscaCamera::MoveZoom(double zoomOffset) {
  return SetZoom( pimpl->zoom+zoomOffset );
}

bool ViscaCamera::GetPanTilt(double *pan, double *tilt) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryPanTiltPos;
  inq.pos[0]= 0x80 | pimpl->self;

  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending PanTiltPos inquiry.\n");
    return false;
  }

  int value;
  // pan maps from 0xFC90-0x0370 to 0.0-1.0
  value = reply.pos[2]*0x1000 +
          reply.pos[3]*0x100  +
          reply.pos[4]*0x10   +
          reply.pos[5];
  if(value&0x8000)
    value |= 0xffff8000; // sign extend
  *pan = value/1760.0+0.5;

  // tilt maps from 0xFED4-0x012C to 0.0-1.0
  value = reply.pos[6]*0x1000 +
          reply.pos[7]*0x100  +
          reply.pos[8]*0x10   +
          reply.pos[9];
  if(value&0x8000)
    value |= 0xffff8000; // sign extend
  *tilt = value/600.0+0.5;

  return true;
}

bool ViscaCamera::SetPanTilt(double p, double t) {
  if(p < 0.0) p=0.0;
  if(1.0 < p) p=1.0;
  if(t < 0.0) t=0.0;
  if(1.0 < t) t=1.0;

  pimpl->pan  = p;
  pimpl->tilt = t;
  pimpl->connection->cameras[pimpl->self-1].pimpl->pan=p;
  pimpl->connection->cameras[pimpl->self-1].pimpl->tilt=t;

  int panValue  = (int)(p*1760.0-880.0); // fc90-0370
  int tiltValue = (int)(t*600.0-300.0); // fed4-012c

  ViscaCommand cmd = ViscaCommandPanTiltPos;
  cmd.pos[ 0]= 0x80 | pimpl->self;
  cmd.pos[ 6]= (panValue >> 12) & 0xf;
  cmd.pos[ 7]= (panValue >> 8)  & 0xf;
  cmd.pos[ 8]= (panValue >> 4)  & 0xf;
  cmd.pos[ 9]= (panValue     )  & 0xf;
  cmd.pos[10]= (tiltValue >> 12) & 0xf;
  cmd.pos[11]= (tiltValue >>  8) & 0xf;
  cmd.pos[12]= (tiltValue >>  4) & 0xf;
  cmd.pos[13]= (tiltValue      ) & 0xf;

  if( !pimpl->connection->Send(&cmd, pimpl->sync) ) {
    fprintf(stderr, "ERROR in sending PanTiltPos command.\n");
    return false;
  }
  return true;
}

bool ViscaCamera::MovePanTilt(double panOffset, double tiltOffset) {
  return SetPanTilt( pimpl->pan+panOffset, pimpl->tilt+tiltOffset );
}

bool ViscaCamera::Stop() {
  // call cancel twice, once for each port
  ViscaCommand cmd = ViscaCommandCommandCancel;
  cmd.pos[0]=0x80 | pimpl->self;

  cmd.pos[1]=0x20;
  if( !pimpl->connection->Send(&cmd,true) ) {
    fprintf(stderr, "ERROR in sending CommandCancel command #0.\n");
    return false;
  }

  cmd.pos[1]=0x21;
  if( !pimpl->connection->Send(&cmd,true) ) {
    fprintf(stderr, "ERROR in sending CommandCancel command #1.\n");
    return false;
  }

  return true;
}



////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////



bool ViscaCameraAdv::GetFocusState(bool *manual) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_FocusMode;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_FocusMode inquiry.\n");
    return false;
  }

  *manual = (reply.pos[2]==0x02)?false:true;

  return true;
}

bool ViscaCameraAdv::SetFocusState(bool manual) {
}

bool ViscaCameraAdv::FocusNear() {
}

bool ViscaCameraAdv::FocusFar() {
}

bool ViscaCameraAdv::SetFocus(double focus) {
}

bool ViscaCameraAdv::GetFocus(double *focus) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_Focus;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_Focus inquiry.\n");
    return false;
  }

  // focus maps from 0x1000-0x9fff to 0.0 to 1.0
  int value = reply.pos[2]*0x1000 + reply.pos[3]*0x100 + reply.pos[4]*0x10 + reply.pos[5];
  *focus = ((double)(value-0x1000))/((double)0x8fff);
  return true;
}

bool ViscaCameraAdv::SetWBAuto() {
}

bool ViscaCameraAdv::SetWBIndoor() {
}

bool ViscaCameraAdv::SetWBOutdoor() {
}

bool ViscaCameraAdv::GetBacklightMode(bool *on) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_Backlight;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_Backlight inquiry.\n");
    return false;
  }

  *on = (reply.pos[2] == 0x02)?true:false;

  return true;
}

bool ViscaCameraAdv::SetBacklightMode(bool on) {
}

bool ViscaCameraAdv::GetKeyLock(bool *on) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryCAM_KeyLock;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending CAM_KeyLock inquiry.\n");
    return false;
  }

  *on = (reply.pos[2] == 0x02)?true:false;

  return true;
}

bool ViscaCameraAdv::SetKeyLock(bool on) {
}

bool ViscaCameraAdv::GetVideoSystem(bool *isNTSC) {
  ViscaCommand inq,reply;
  inq = ViscaInquiryVideoSystem;
  inq.pos[0]= 0x80 | pimpl->self;
  if( !pimpl->connection->Inquiry(&inq,&reply) ) {
    fprintf(stderr, "ERROR in sending VideoSystem inquiry.\n");
    return false;
  }

  *isNTSC = (reply.pos[2] == 0x00)?true:false;

  return true;
}

bool ViscaCameraAdv::SetIRReceive(bool on) {
}

