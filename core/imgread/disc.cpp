#include "Disc.h"

bool Disc::ReadSector(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
{
	for (size_t i = tracks.size(); i-- > 0;)
	{
		*subcode_type = SUBFMT_NONE;
		if (tracks[i].Read(FAD, dst, sector_type, subcode, subcode_type))
			return true;
	}

	return false;
}

void Disc::ReadSectors(u32 FAD, u32 count, u8* dst, u32 fmt)
{
	u8 temp[2352];
	SectorFormat secfmt;
	SubcodeFormat subfmt;

	while (count)
	{
		if (ReadSector(FAD, temp, &secfmt, q_subchannel, &subfmt))
		{
			//TODO: Proper sector conversions
			if (secfmt == SECFMT_2352)
			{
				ConvertSector(temp, dst, 2352, fmt, FAD);
			}
			else if (fmt == 2048 && secfmt == SECFMT_2336_MODE2)
				memcpy(dst, temp + 8, 2048);
			else if (fmt == 2048 && (secfmt == SECFMT_2048_MODE1 || secfmt == SECFMT_2048_MODE2_FORM1))
			{
				memcpy(dst, temp, 2048);
			}
			else if (fmt == 2352 && (secfmt == SECFMT_2048_MODE1 || secfmt == SECFMT_2048_MODE2_FORM1))
			{
				printf("GDR:fmt=2352;secfmt=2048\n");
				memcpy(dst, temp, 2048);
			}
			else
			{
				printf("ERROR: UNABLE TO CONVERT SECTOR. THIS IS FATAL.");
				//verify(false);
			}
		}
		else
		{
			printf("Sector Read miss FAD: %d\n", FAD);
		}
		dst += fmt;
		FAD++;
		count--;
	}
}

void Disc::FillGDSession()
{
	Session ses;

	//session 1 : start @ track 1, and its fad
	ses.FirstTrack = 1;
	ses.StartFAD = tracks[0].StartFAD;
	sessions.push_back(ses);

	//session 2 : start @ track 3, and its fad
	ses.FirstTrack = 3;
	ses.StartFAD = tracks[0].StartFAD;
	sessions.push_back(ses);

	//this isn't always true for gdroms, depends on area look @ the get-toc code
	type = GdRom;
	LeadOut.ADDR = 0;
	LeadOut.CTRL = 0;
	LeadOut.StartFAD = 549300;

	EndFAD = 549300;
}

void Disc::Dump(const string& path)
{
	for (u32 i = 0; i < tracks.size(); i++)
	{
		u32 fmt = tracks[i].CTRL == 4 ? 2048 : 2352;
		char fsto[1024];
		sprintf(fsto, "%s%s%d.img", path.c_str(), ".track", i);

		FILE* fo = fopen(fsto, "wb");

		for (u32 j = tracks[i].StartFAD; j <= tracks[i].EndFAD; j++)
		{
			u8 temp[2352];
			ReadSectors(j, 1, temp, fmt);
			fwrite(temp, fmt, 1, fo);
		}
		fclose(fo);
	}
}

std::unique_ptr<Disc> Disc::CreateDisc(const std::string& fileName)
{
	//GetFileType
	//create a factory which return right parser class
	//parse it and return a disc
	auto disk = chd_parse(fileName);
	if (!disk)
		disk = gdi_parse(fileName);
	if (!disk)
		disk = cdi_parse(fileName);
	if (!disk)
		disk = ioctl_parse(fileName);
	return disk;
}

RawTrackFile::RawTrackFile(core_file* file, u32 file_offs, u32 first_fad, u32 secfmt)
{
	verify(file != 0);
	this->file = file;
	this->offset = file_offs - first_fad * secfmt;
	this->fmt = secfmt;
	this->cleanup = true;
}

void RawTrackFile::Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
{
	//for now hackish
	if (fmt == 2352)
		*sector_type = SECFMT_2352;
	else if (fmt == 2048)
		*sector_type = SECFMT_2048_MODE2_FORM1;
	else if (fmt == 2336)
		*sector_type = SECFMT_2336_MODE2;
	else
	{
		verify(false);
	}

	core_fseek(file, offset + FAD * fmt, SEEK_SET);
	core_fread(file, dst, fmt);
}

RawTrackFile::~RawTrackFile()
{
	if (cleanup && file)
		core_fclose(file);
}

bool Disc::ConvertSector(u8* in_buff, u8* out_buff, int from, int to, int sector)
{
	//get subchannel data, if any
	if (from == 2448)
	{
		memcpy(q_subchannel, in_buff + 2352, 96);
		from -= 96;
	}
	//if no conversion
	if (to == from)
	{
		memcpy(out_buff, in_buff, to);
		return true;
	}
	switch (to)
	{
	case 2340:
	{
		verify((from == 2352));
		memcpy(out_buff, &in_buff[12], 2340);
	}
	break;
	case 2328:
	{
		verify((from == 2352));
		memcpy(out_buff, &in_buff[24], 2328);
	}
	break;
	case 2336:
		verify(from >= 2336);
		verify((from == 2352));
		memcpy(out_buff, &in_buff[0x10], 2336);
		break;
	case 2048:
	{
		verify(from >= 2048);
		verify((from == 2448) || (from == 2352) || (from == 2336));
		if ((from == 2352) || (from == 2448))
		{
			if (in_buff[15] == 1)
			{
				memcpy(out_buff, &in_buff[0x10], 2048); //0x10 -> mode1
			}
			else
				memcpy(out_buff, &in_buff[0x18], 2048); //0x18 -> mode2 (all forms ?)
		}
		else
			memcpy(out_buff, &in_buff[0x8], 2048);	//hmm only possible on mode2.Skip the mode2 header
	}
	break;
	case 2352:
		//if (from >= 2352)
	{
		memcpy(out_buff, &in_buff[0], 2352);
	}
	break;
	default:
		printf("Sector conversion from %d to %d not supported \n", from, to);
		break;
	}

	return true;
}

bool Track::Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
{
	if (FAD >= StartFAD && (FAD <= EndFAD || EndFAD == 0) && file)
	{
		file->Read(FAD, dst, sector_type, subcode, subcode_type);
		return true;
	}
	else
	{
		return false;
	}
}