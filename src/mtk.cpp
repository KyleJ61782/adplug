/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999, 2000, 2001 Simon Peter, <dn.tlp@gmx.net>, et al.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * mtk.cpp - MPU-401 Trakker Loader by Simon Peter (dn.tlp@gmx.net)
 */

#include "mtk.h"

/*** public methods **************************************/

CPlayer *CmtkLoader::factory(Copl *newopl)
{
  CmtkLoader *p = new CmtkLoader(newopl);
  return p;
}

bool CmtkLoader::load(istream &f, const char *filename)
{
	struct {
		char id[18];
		unsigned short crc,size;
	} header;
	struct mtkdata {
		char songname[34],composername[34],instname[0x80][34];
		unsigned char insts[0x80][12],order[0x80],dummy,patterns[0x32][0x40][9];
	} *data;
	unsigned char *cmp,*org,i;
	unsigned long cmpsize,cmpptr=0,orgptr=0;
	unsigned short ctrlbits=0,ctrlmask=0,cmd,cnt,offs;

	// file validation section
	f.read((char *)&header,sizeof(header));
	if(strncmp(header.id,"mpu401tr\x92kk\xeer@data",18))
		return false;

	// load section
	f.seekg(0,ios::end);
	cmpsize = f.tellg(); cmpsize -= sizeof(header);
	f.seekg(sizeof(header));
	cmp = new unsigned char[cmpsize];
	org = new unsigned char[header.size];
	f.read((char *)cmp,cmpsize);

	while(cmpptr < cmpsize) {	// decompress
		ctrlmask >>= 1;
		if(!ctrlmask) {
			ctrlbits = *(unsigned short *) &cmp[cmpptr];
			cmpptr += 2;
			ctrlmask = 0x8000;
		}
		if(!(ctrlbits & ctrlmask)) {	// uncompressed data
			org[orgptr] = cmp[cmpptr];
			orgptr++; cmpptr++;
			continue;
		}

		// compressed data
		cmd = (cmp[cmpptr] >> 4) & 0x0f;
		cnt = cmp[cmpptr] & 0x0f;
		cmpptr++;
		switch(cmd) {
		case 0: cnt += 3; memset(&org[orgptr],cmp[cmpptr],cnt); cmpptr++; orgptr += cnt; break;
		case 1: cnt += (cmp[cmpptr] << 4) + 19; memset(&org[orgptr],cmp[++cmpptr],cnt); cmpptr++; orgptr += cnt; break;
		case 2: offs = (cnt+3) + (cmp[cmpptr] << 4); cnt = cmp[++cmpptr] + 16; cmpptr++;
				memcpy(&org[orgptr],&org[orgptr - offs],cnt); orgptr += cnt; break;
		default: offs = (cnt+3) + (cmp[cmpptr++] << 4); memcpy(&org[orgptr],&org[orgptr-offs],cmd); orgptr += cmd; break;
		}
	}
	delete [] cmp;
	data = (struct mtkdata *) org;

	// convert to HSC replay data
	memset(title,0,34); strncpy(title,data->songname+1,33);
	memset(composer,0,34); strncpy(composer,data->composername+1,33);
	memset(instname,0,0x80*34);
	for(i=0;i<0x80;i++)
		strncpy(instname[i],data->instname[i]+1,33);
	memcpy(instr,data->insts,0x80 * 12);
	memcpy(song,data->order,0x80);
	memcpy(patterns,data->patterns,header.size-6084);
	for (i=0;i<128;i++) {				// correct instruments
		instr[i][2] ^= (instr[i][2] & 0x40) << 1;
		instr[i][3] ^= (instr[i][3] & 0x40) << 1;
		instr[i][11] >>= 4;		// make unsigned
	}

	delete [] org;
	rewind(0);
	return true;
}