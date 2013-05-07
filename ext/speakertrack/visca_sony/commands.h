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

#if !defined(_COMMANDS_H__INCLUDED_)
#define _COMMANDS_H__INCLUDED_

/* All commands are in the form of a broadcast command.
   To direct a command to a single camera, change pos[0]
   to 0x8X
   see the visca docs
*/

// The maximum command length in bytes
#define MAXCOMMANDLENGTH 15 

struct ViscaCommand {
  unsigned char pos[MAXCOMMANDLENGTH];

  int GetLength() const;
  const ViscaCommand& operator =(const ViscaCommand&);
};

extern const ViscaCommand ViscaCommandAddressSet;
extern const ViscaCommand ViscaCommandIF_Clear;
extern const ViscaCommand ViscaCommandCommandCancel;

extern const ViscaCommand ViscaCommandCAM_Zoom;
extern const ViscaCommand ViscaInquiryCAM_Zoom;

extern const ViscaCommand ViscaCommandCAM_FocusFar;
extern const ViscaCommand ViscaCommandCAM_FocusNear;
extern const ViscaCommand ViscaCommandCAM_FocusAuto;
extern const ViscaCommand ViscaCommandCAM_FocusManual;
extern const ViscaCommand ViscaCommandCAM_Focus;
extern const ViscaCommand ViscaInquiryCAM_Focus;
extern const ViscaCommand ViscaInquiryCAM_FocusMode;

extern const ViscaCommand ViscaCommandCAM_WBAuto;
extern const ViscaCommand ViscaCommandCAM_WBIndoor;
extern const ViscaCommand ViscaCommandCAM_WBOutdoor;

extern const ViscaCommand ViscaCommandCAM_BacklightOn;
extern const ViscaCommand ViscaCommandCAM_BacklightOff;
extern const ViscaCommand ViscaInquiryCAM_Backlight;

extern const ViscaCommand ViscaCommandCAM_KeyLockOn;
extern const ViscaCommand ViscaCommandCAM_KeyLockOff;
extern const ViscaCommand ViscaInquiryCAM_KeyLock;

extern const ViscaCommand ViscaInquiryVideoSystem;
extern const ViscaCommand ViscaCommandIR_ReceiveOn;
extern const ViscaCommand ViscaCommandIR_ReceiveOff;

extern const ViscaCommand ViscaCommandCAM_Power;
extern const ViscaCommand ViscaInquiryCAM_Power;

extern const ViscaCommand ViscaCommandPanTiltHome;
extern const ViscaCommand ViscaCommandPanTiltReset;
extern const ViscaCommand ViscaCommandPanTiltPos;
extern const ViscaCommand ViscaInquiryPanTiltPos;


#endif // !defined(_COMMANDS_H__INCLUDED_)
