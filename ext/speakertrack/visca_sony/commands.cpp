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

#include "commands.h"


const ViscaCommand& ViscaCommand::operator =(const ViscaCommand& a) {
  for(int t=0; t<MAXCOMMANDLENGTH; ++t)
    pos[t]=a.pos[t];
  return *this;
}

int ViscaCommand::GetLength() const {
  int ret = 0;
  while(ret != MAXCOMMANDLENGTH) {
    ++ret;
    if(pos[ret-1] == 0xff)
      break;
  }
  return ret;
}

// all of these commands broadcast
// See the visca docs for command specific adjustments 
const ViscaCommand ViscaCommandAddressSet      = {0x88, 0x30, 0x01, 0xff};
const ViscaCommand ViscaCommandIF_Clear        = {0x88, 0x01, 0x00, 0x01, 0xff};
const ViscaCommand ViscaCommandCommandCancel   = {0x88, 0x20, 0xff};

const ViscaCommand ViscaCommandCAM_Zoom        = {0x88, 0x01, 0x04, 0x47, 0x00, 0x00, 0x00, 0x00, 0xff};
const ViscaCommand ViscaInquiryCAM_Zoom        = {0x88, 0x09, 0x04, 0x47, 0xff};

const ViscaCommand ViscaCommandCAM_FocusFar    = {0x88, 0x01, 0x04, 0x08, 0x02, 0xff};
const ViscaCommand ViscaCommandCAM_FocusNear   = {0x88, 0x01, 0x04, 0x08, 0x03, 0xff};
const ViscaCommand ViscaCommandCAM_FocusAuto   = {0x88, 0x01, 0x04, 0x38, 0x02, 0xff};
const ViscaCommand ViscaCommandCAM_FocusManual = {0x88, 0x01, 0x04, 0x38, 0x03, 0xff};
const ViscaCommand ViscaCommandCAM_Focus       = {0x88, 0x01, 0x04, 0x48, 0x00, 0x00, 0x00, 0x00, 0xff};
const ViscaCommand ViscaInquiryCAM_Focus       = {0x88, 0x09, 0x04, 0x48, 0xff};
const ViscaCommand ViscaInquiryCAM_FocusMode   = {0x88, 0x09, 0x04, 0x38, 0xff};

const ViscaCommand ViscaCommandCAM_WBAuto      = {0x88, 0x01, 0x04, 0x35, 0x00, 0xff};
const ViscaCommand ViscaCommandCAM_WBIndoor    = {0x88, 0x01, 0x04, 0x35, 0x01, 0xff};
const ViscaCommand ViscaCommandCAM_WBOutdoor   = {0x88, 0x01, 0x04, 0x35, 0x02, 0xff};

const ViscaCommand ViscaCommandCAM_BacklightOn = {0x88, 0x01, 0x04, 0x33, 0x02, 0xff};
const ViscaCommand ViscaCommandCAM_BacklightOff= {0x88, 0x01, 0x04, 0x33, 0x02, 0xff};
const ViscaCommand ViscaInquiryCAM_Backlight   = {0x88, 0x09, 0x04, 0x33, 0xff};

const ViscaCommand ViscaCommandCAM_KeyLockOn   = {0x88, 0x01, 0x04, 0x17, 0x00, 0xff};
const ViscaCommand ViscaCommandCAM_KeyLockOff  = {0x88, 0x01, 0x04, 0x17, 0x02, 0xff};
const ViscaCommand ViscaInquiryCAM_KeyLock     = {0x88, 0x09, 0x04, 0x17, 0xff};

const ViscaCommand ViscaInquiryVideoSystem     = {0x88, 0x09, 0x06, 0x23, 0xff};

const ViscaCommand ViscaCommandIR_ReceiveOn    = {0x88, 0x01, 0x06, 0x08, 0x02, 0xff};
const ViscaCommand ViscaCommandIR_ReceiveOff   = {0x88, 0x01, 0x06, 0x08, 0x03, 0xff};

const ViscaCommand ViscaCommandCAM_Power       = {0x88, 0x01, 0x04, 0x00, 0x02, 0xff};
const ViscaCommand ViscaInquiryCAM_Power       = {0x88, 0x09, 0x04, 0x00, 0xff};

const ViscaCommand ViscaCommandPanTiltHome     = {0x88, 0x01, 0x06, 0x04, 0xff};
const ViscaCommand ViscaCommandPanTiltReset    = {0x88, 0x01, 0x06, 0x05, 0xff};
const ViscaCommand ViscaCommandPanTiltPos      = {0x88, 0x01, 0x06, 0x02, 0x18, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
const ViscaCommand ViscaInquiryPanTiltPos      = {0x88, 0x09, 0x06, 0x12, 0xff};
