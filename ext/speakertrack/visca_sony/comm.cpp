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

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "connectionimp.h"
#include "commands.h"


/* isERR decides whether or not a given reply is a response
   to a given command, and returns true if the given reply
   is an error.
*/
// isERR is local to this file
bool isERR(const class ViscaCommand *cmd, const class ViscaCommand *reply) {
  if( (reply->pos[0]>>4)-8 != (0xf & cmd->pos[0]) )
    return false; // reply not a response from cmd

  if( reply->pos[1]==0x60 &&
      reply->pos[3]==0xff && (
      reply->pos[2]==0x02 ||
      reply->pos[2]==0x03 ||
      reply->pos[2]==0x04 ||
      reply->pos[2]==0x05 ||
      reply->pos[2]==0x41 ))
    return true;
  return false;
}

/* printIfERR returns whether or not the given ViscaCommand
   is an error message, and also prints out to stderr
*/
// printIfERR is local to this file
bool printIfERR(const class ViscaCommand *cmd) {
  if(cmd->pos[1]==0x60 && cmd->pos[2]==0x02 && cmd->pos[3]==0xff) {
    fprintf(stderr,"Visca ERROR camera [%d]: Syntax Error.\n",(cmd->pos[0]>>4)-8);
    return true;
  }
  if(cmd->pos[1]==0x60 && cmd->pos[2]==0x03 && cmd->pos[3]==0xff) {
    fprintf(stderr,"Visca ERROR camera [%d]: Command Buffer Full.\n",(cmd->pos[0]>>4)-8);
    return true;
  }
  if(cmd->pos[1]==0x60 && cmd->pos[2]==0x04 && cmd->pos[3]==0xff) {
    fprintf(stderr,"Visca ERROR camera [%d]: Command Cancel.\n",(cmd->pos[0]>>4)-8);
    return true;
  }
  if(cmd->pos[1]==0x60 && cmd->pos[2]==0x05 && cmd->pos[3]==0xff) {
    fprintf(stderr,"Visca ERROR camera [%d]: No Sockets.\n",(cmd->pos[0]>>4)-8);
    return true;
  }
  if(cmd->pos[1]==0x60 && cmd->pos[2]==0x41 && cmd->pos[3]==0xff) {
    fprintf(stderr,"Visca ERROR camera [%d]: Command Not Executable.\n",(cmd->pos[0]>>4)-8);
    return true;
  }
  return false;
}

bool ViscaConnectionImp::Get(int timeoutSecs, class ViscaCommand *result) {
  return Get(fd,timeoutSecs, result);
}

bool ViscaConnectionImp::Get(int thefd, int timeoutSecs, class ViscaCommand *result) {
  result->pos[0]=0xff;
  result->pos[MAXCOMMANDLENGTH-1]=0xff;

  int retVal,thisTime=0;
  fd_set readFds;
  struct timeval timeout;

  FD_ZERO(&readFds);
  FD_SET(thefd, &readFds);
  timeout.tv_sec = timeoutSecs;
  timeout.tv_usec = 0;

  retVal = select(thefd+1, &readFds, NULL, NULL, &timeout);

  if(retVal <= 0)
    return false; // time is up or error


  int index = 0, reads = 0;

  do {
    retVal = read(thefd,&(result->pos[index]),1); // Read a byte
    if(retVal < 0)
      return false; // read error
    if(0 < retVal)
      index++;
    reads++;
    if(reads >= 100) // if we haven't read a full command after 100 tries,
      return false;  // crap, we're not reading a full command (this is a major problem)
  } while(index < MAXCOMMANDLENGTH && // end of buffer (if this is false, there is a major problem)
          ((result->pos[index-1] != 0xff) || (index == 0)));

  // print
  /*printf("result: ");
  for(int t=0; result->pos[t]!=0xff;t++)
    printf("%x ",result->pos[t]);
  printf("ff\n");
  */
  printIfERR(result);

  return result->pos[index-1]==0xff;
}


bool ViscaConnectionImp::Send(const class ViscaCommand *cmd,bool sync) {
  if(sync)
    return SendFin(cmd);
  return SendAck(cmd);
}

bool ViscaConnectionImp::Send(int thefd, const class ViscaCommand *cmd) {
  int written=0, wret, cmdlen=cmd->GetLength();

  while(true) {
    wret=write(thefd, &(cmd->pos[written]), cmdlen-written);
    if(wret < 0) return false;
    written += wret;
    if(written >= cmdlen) return true;
  }
}

bool ViscaConnectionImp::SendAck(const class ViscaCommand *cmd) {
  if(!Send(fd,cmd))
    return false;

  if((cmd->pos[0] & 0xf)==8) // no ACK message if were broadcasting
    return true;

  ViscaCommand ack;
  struct timeval now,end;
  gettimeofday(&end,0);
  end.tv_sec+=1;          // timeout in one second
  
  do {
    Get(fd, 1, &ack);

    if(isERR(cmd,&ack))
      return false;

    if( (ack.pos[0]>>4)-8 == (0xf & cmd->pos[0]) &&
        (ack.pos[1]&0xf0) == 0x40 &&
	ack.pos[2]       == 0xff )
      return true;

    gettimeofday(&now,0);
  } while(timercmp(&now,&end,<));

  return false;
}

bool ViscaConnectionImp::SendFin(const class ViscaCommand *cmd) {
  if( !Send(fd,cmd))
    return false;

  if( (cmd->pos[0]&0xf)==8 ) // no ACK or FIN message when were broadcasting
    return true;

  ViscaCommand ack,fin;
  struct timeval now,end;
  gettimeofday(&end,0);
  end.tv_sec+=5;          // timeout in five seconds

  // wait for the ack
  while(true) {
    Get(fd, 1, &ack);

    if(isERR(cmd,&ack))
      return false;

    if( (ack.pos[0]>>4)-8 == (0xf & cmd->pos[0]) &&
        (ack.pos[1]&0xf0) == 0x40 &&
	ack.pos[2]       == 0xff )
      break;

    gettimeofday(&now,0);
    if(timercmp(&now,&end,>))
      return false; // timeout
  }

  int socket = ack.pos[1]&0xf;

  do {
    Get(fd, 1, &fin);

    if(isERR(cmd,&fin))
      return false;

    if( (fin.pos[0]>>4)-8 == (0xf & cmd->pos[0]) &&
        (fin.pos[1]&0xf0) == 0x50 &&
        (fin.pos[1]&0x0f) == socket &&
	fin.pos[2]       == 0xff )
      return true;

    gettimeofday(&now,0);
  } while(timercmp(&now,&end,<));

  return false;
}

bool ViscaConnectionImp::Inquiry(const class ViscaCommand *inq, class ViscaCommand *reply) {
  if( (inq->pos[0]&0xf)==8) // were broadcasting
    return false;

  if( !Send(fd,inq) )
    return false;
  
  struct timeval now,end;
  gettimeofday(&end,0);
  end.tv_sec+=1;          // timeout in one second
  
  do {
    Get(fd, 1, reply);

    if(isERR(inq,reply))
      return false;

    if( (reply->pos[0]>>4)-8 == (0xf & inq->pos[0]) &&
	reply->pos[1]       == 0x50)
      return true;

    gettimeofday(&now,0);
  } while(timercmp(&now,&end,<));

  return false;
}
