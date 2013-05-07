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

#ifndef _VISCA_H__INCLUDED_
#define _VISCA_H__INCLUDED_


// C style functions: false return value indecates error
// Cameras are indexed from 1 to 7
// Range of zoom, pan, and tilt values are 0.0-1.0
//
// example usage:
//
// ViscaConnection visca("/dev/ttyS0");
// ViscaCamera cam1 = visca.GetCamera(1);
// 
// cam1.SetPowerState(true);
// cam1.SetZoom(0.0);
// cam1.GetZoom(&value);

#if defined(__cplusplus)

struct ViscaCamera {
  bool GetPowerState(bool *on);
  bool SetPowerState(bool on=true);

  bool GetPan(double *ret);
  bool SetPan(double pan);
  bool MovePan(double panOffset);

  bool GetTilt(double *ret);
  bool SetTilt(double tilt);
  bool MoveTilt(double tiltOffset);

  bool GetZoom(double *ret);
  bool SetZoom(double zoom);
  bool MoveZoom(double zoomOffset);

  bool GetPanTilt(double *pan, double *tilt);
  bool SetPanTilt(double pan, double tilt);
  bool MovePanTilt(double panOffset, double tiltOffset);

  bool Stop();

  bool SetToSyncMode(); // Camera commands block until completion
  bool SetToASyncMode(); // Camera commands block until acknowledgment

  ViscaCamera();
  ViscaCamera(const ViscaCamera&);
  ~ViscaCamera();
  const ViscaCamera &operator=(const ViscaCamera &);

  struct ViscaCameraImp *pimpl; // private implementation
};

struct ViscaCameraAdv : public ViscaCamera {
  bool GetFocusState(bool *manual);
  bool SetFocusState(bool manual=false);
  bool FocusNear();
  bool FocusFar();
  bool SetFocus(double focus);
  bool GetFocus(double *focus);

  bool SetWBAuto();
  bool SetWBIndoor();
  bool SetWBOutdoor();

  // Auto exposure options omitted until requested
  // (bright, shutter, iris, gain

  bool GetBacklightMode(bool *on);
  bool SetBacklightMode(bool on=false);

  bool GetKeyLock(bool *on);
  bool SetKeyLock(bool on=false);

  bool GetVideoSystem(bool *isNTSC);

  bool SetIRReceive(bool on=true);
};

struct ViscaConnection {
  ViscaConnection(const char *device);
  ViscaConnection(const ViscaConnection&);
  ~ViscaConnection();
  const ViscaConnection &operator=(const ViscaConnection&);

  int GetCameraCount() const;
  bool isValid() const;

  ViscaCamera    GetCamera(int camera) const;
  ViscaCameraAdv GetCameraAdv(int camera) const;
  ViscaCamera    GetBroadcastCamera() const;

  struct ViscaConnectionImp *pimpl; // private implementation
};

#endif//__cplusplus

typedef struct _SonyVisca SonyVisca;

#ifdef __cplusplus
extern "C" {
#endif
    SonyVisca* sony_visca_new();
    void sony_visca_free(SonyVisca *sony);
    void sony_visca_open(SonyVisca *sony, const char *device);
    void sony_visca_close(SonyVisca *sony);
    void sony_visca_pan(SonyVisca *sony, int x);
    void sony_visca_tilt(SonyVisca *sony, int y);
    void sony_visca_zoom(SonyVisca *sony, int z);
#ifdef __cplusplus
}
#endif

#endif // _VISCA_H__INCLUDED_
