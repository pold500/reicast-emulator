#include "drive.h"

bool Drive::InitDrive_(const std::string& fn)
{
	TermDrive();

	//try all drivers
	m_disk = Disc::CreateDisc(fn);

	if (m_disk)
	{
		printf("gdrom: Opened image \"%s\"\n", fn);
		m_NullDriveDiscType = DiscType::Busy;
#ifndef NOT_REICAST
		libCore_gdrom_disc_change();
#endif
		//		Sleep(400); //busy for a bit // what, really ?
		return true;
	}
	else
	{
		printf("gdrom: Failed to open image \"%s\"\n", fn);
		m_NullDriveDiscType = DiscType::NoDisk;
	}
	return false;
}

bool Drive::InitDrive_(const std::string& fileName)
{

}

Drive::Drive():
	m_disk(nullptr)
{

}

#ifndef NOT_REICAST
bool Drive::InitDrive(u32 fileflags)
{
	if (settings.imgread.LoadDefaultImage)
	{
		printf("Loading default image \"%s\"\n", settings.imgread.DefaultImage);
		if (!InitDrive_(settings.imgread.DefaultImage.c_str()))
		{
			msgboxf("Default image \"%s\" failed to load", MBX_ICONERROR);
			return false;
		}
		else
			return true;
	}

	std::string diskImageFileName{ settings.imgread.LastImage };
#ifdef BUILD_DREAMCAST
	int gfrv = GetFile(diskImageFileName);
#else
	int gfrv = rv_error;
#endif
	if (gfrv != rv_ok)
	{
		return false;
	}

	// FIXME: Data loss if buffer is too small
	settings.imgread.LastImage = diskImageFileName;

	SaveSettings();

	if (!InitDrive_(diskImageFileName.c_str()))
	{
		//msgboxf("Selected image failed to load",MBX_ICONERROR);
		m_NullDriveDiscType = NoDisk;
		gd_setdisc();
		SetSnsCodes(0x29, 0x00, 0x06);
		return false;
	}
	return true;
}

void Drive::TermDrive()
{
}

u32 Drive::CreateTrackInfo(u32 ctrl, u32 addr, u32 fad)
{
	u8 p[4];
	p[0] = (ctrl << 4) | (addr << 0);
	p[1] = fad >> 16;
	p[2] = fad >> 8;
	p[3] = fad >> 0;

	return *(u32*)p;
}

u32 Drive::CreateTrackInfo_se(u32 ctrl, u32 addr, u32 tracknum)
{
	u8 p[4];
	p[0] = (ctrl << 4) | (addr << 0);
	p[1] = tracknum;
	p[2] = 0;
	p[3] = 0;
	return *(u32*)p;
}

void Drive::GetDriveSector(u8 * buff, u32 StartSector, u32 SectorCount, u32 secsz)
{
	//printf("GD: read %08X, %d\n",StartSector,SectorCount);
	if (disc)
	{
		disc->ReadSectors(StartSector, SectorCount, buff, secsz);
		if (disc->type == GdRom && StartSector == 45150 && SectorCount == 7)
		{
			PatchRegion_0(buff, secsz);
			PatchRegion_6(buff + 2048 * 6, secsz);
		}
	}
}

void Drive::GetDriveToc(u32* to, DiskArea area)
{
	if (!disc)
		return;
	memset(to, 0xFFFFFFFF, 102 * 4);

	//can't get toc on the second area on discs that don't have it
	verify(area != DoubleDensity || disc->type == GdRom);

	//normal CDs: 1 .. tc
	//GDROM: area0 is 1 .. 2, area1 is 3 ... tc

	u32 first_track = 1;
	u32 last_track = disc->tracks.size();
	if (area == DoubleDensity)
		first_track = 3;
	else if (disc->type == GdRom)
	{
		last_track = 2;
	}

	//Generate the TOC info

	//-1 for 1..99 0 ..98
	to[99] = CreateTrackInfo_se(disc->tracks[first_track - 1].CTRL, disc->tracks[first_track - 1].ADDR, first_track);
	to[100] = CreateTrackInfo_se(disc->tracks[last_track - 1].CTRL, disc->tracks[last_track - 1].ADDR, last_track);

	if (disc->type == GdRom)
	{
		//use smaller LEADOUT
		if (area == SingleDensity)
			to[101] = CreateTrackInfo(disc->LeadOut.CTRL, disc->LeadOut.ADDR, 13085);
	}
	else
	{
		to[101] = CreateTrackInfo(disc->LeadOut.CTRL, disc->LeadOut.ADDR, disc->LeadOut.StartFAD);
	}

	for (u32 i = first_track - 1; i < last_track; i++)
	{
		to[i] = CreateTrackInfo(disc->tracks[i].CTRL, disc->tracks[i].ADDR, disc->tracks[i].StartFAD);
	}
}

void Drive::GetDriveSessionInfo(u8* to, u8 session)
{
	if (!disc)
		return;
	to[0] = 2;//status, will get overwritten anyway
	to[1] = 0;//0's

	if (session == 0)
	{
		to[2] = disc->sessions.size();//count of sessions
		to[3] = disc->EndFAD >> 16;//fad is sessions end
		to[4] = disc->EndFAD >> 8;
		to[5] = disc->EndFAD >> 0;
	}
	else
	{
		to[2] = disc->sessions[session - 1].FirstTrack;//start track of this session
		to[3] = disc->sessions[session - 1].StartFAD >> 16;//fad is session start
		to[4] = disc->sessions[session - 1].StartFAD >> 8;
		to[5] = disc->sessions[session - 1].StartFAD >> 0;
	}
}

void Drive::printtoc(TocInfo* toc, SessionInfo* ses)
{
	printf("Sessions %d\n", ses->SessionCount);
	for (u32 i = 0; i < ses->SessionCount; i++)
	{
		printf("Session %d: FAD %d,First Track %d\n", i + 1, ses->SessionFAD[i], ses->SessionStart[i]);
		for (u32 t = toc->FistTrack - 1; t <= toc->LastTrack; t++)
		{
			if (toc->tracks[t].Session == i + 1)
			{
				printf("\tTrack %d : FAD %d CTRL %d ADR %d\n", t, toc->tracks[t].FAD, toc->tracks[t].Control, toc->tracks[t].Addr);
			}
		}
	}
	printf("Session END: FAD END %d\n", ses->SessionsEndFAD);
}

DiscType Drive::GuessDiscType(bool m1, bool m2, bool da)
{
	if ((m1 == true) && (da == false) && (m2 == false))
		return  CdRom;
	else if (m2)
		return  CdRom_XA;
	else if (da && m1)
		return CdRom_Extra;
	else
		return CdRom;
}

#endif



bool Drive::DiscSwap(u32 fileflags)
{
	if (settings.imgread.LoadDefaultImage)
	{
		printf("Loading default image \"%s\"\n", settings.imgread.DefaultImage);
		if (!InitDrive_(settings.imgread.DefaultImage.c_str()))
		{
			msgboxf("Default image \"%s\" failed to load", MBX_ICONERROR);
			return false;
		}
		else
			return true;
	}

	std::string diskImageFileName{ settings.imgread.LastImage };

#ifdef BUILD_DREAMCAST
	int gfrv = GetFile(diskImageFileName);
#else
	int gfrv = rv_error;
#endif
	if (gfrv == rv_ok)
	{
		m_NullDriveDiscType = Open;
		gd_setdisc();
		SetSnsCodes(0x28, 0x00, 0x06);
		return true;
	}
	else
	{
		SetSnsCodes(0x28, 0x00, 0x06); // to fix: we have same return codes as previous branch, looks suspicious
		return false;
	}


	settings.imgread.LastImage = diskImageFileName;

	SaveSettings();

	if (!InitDrive_(diskImageFileName.c_str()))
	{
		//msgboxf("Selected image failed to load",MBX_ICONERROR);
		m_NullDriveDiscType = Open;
		gd_setdisc();
		SetSnsCodes(0x28, 0x00, 0x06);
		return true;
	}
	else
	{
		SetSnsCodes(0x28, 0x00, 0x06);
		return false;
	}
}


void Drive::PatchRegion_0(u8* sector, int size)
{
#ifndef NOT_REICAST
	if (settings.imgread.PatchRegion == 0)
		return;
#else
	return;
#endif

	u8* usersect = sector;

	if (size != 2048)
	{
		printf("PatchRegion_0 -> sector size %d , skipping patch\n", size);
	}

	//patch meta info
	u8* p_area_symbol = &usersect[0x30];
	memcpy(p_area_symbol, "JUE     ", 8);
}
void Drive::PatchRegion_6(u8* sector, int size)
{
#ifndef NOT_REICAST
	if (settings.imgread.PatchRegion == 0)
		return;
#else
	return;
#endif

	u8* usersect = sector;

	if (size != 2048)
	{
		printf("PatchRegion_6 -> sector size %d , skipping patch\n", size);
	}

	//patch area symbols
	u8* p_area_text = &usersect[0x700];
	memcpy(&p_area_text[4], "For JAPAN,TAIWAN,PHILIPINES.", 28);
	memcpy(&p_area_text[4 + 32], "For USA and CANADA.         ", 28);
	memcpy(&p_area_text[4 + 32 + 32], "For EUROPE.                 ", 28);
}






