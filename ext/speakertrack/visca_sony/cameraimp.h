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

#if !defined(_CAMERAIMP_H__INCLUDED_)
#define _CAMERAIMP_H__INCLUDED_

#define INIT_PAN .5
#define INIT_TILT .5
#define INIT_ZOOM 0.0

struct ViscaCameraImp {
  int self;
  double pan,tilt,zoom;

  bool sync;

  struct ViscaConnectionImp *connection;
};

#endif // _CAMERAIMP_H__INCLUDED_
