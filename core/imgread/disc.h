#pragma once
#include "types.h"
#include <memory>

struct SNSCodes
{
	int sns_asc;
	int sns_ascq;
	int sns_key;
};

enum class DiscType
{
	CdDA = 0x00,
	CdRom = 0x10,
	CdRom_XA = 0x20,
	CdRom_Extra = 0x30,
	CdRom_CDI = 0x40,
	GdRom = 0x80,

	NoDisk = 0x1,			//These are a bit hacky .. but work for now ...
	Open = 0x2,			//tray is open :)
	Busy = 0x3			//busy -> needs to be automatically done by gdhost
};

enum DiskArea
{
	SingleDensity,
	DoubleDensity
};

enum DriveEvent
{
	DiskChange = 1	//disk ejected/changed
};

struct TocTrackInfo
{
	u32 FAD;    //fad, Intel format
	u8 Control; //control info
	u8 Addr;    //address info
	u8 Session; //Session where the track belongs
};

struct TocInfo
{
	//0-98 ->1-99
	TocTrackInfo tracks[99];

	u8 FistTrack;
	u8 LastTrack;

	TocTrackInfo LeadOut; //session set to 0 on that one
};

struct SessionInfo
{
	u32 SessionsEndFAD;   //end of Disc (?)
	u8  SessionCount;      //must be at least 1
	u32 SessionStart[99]; //start track for session
	u32 SessionFAD[99];   //for sessions 1-99 ;)
};

enum SectorFormat
{
	SECFMT_2352,				//full sector
	SECFMT_2048_MODE1,			//2048 user byte, form1 sector
	SECFMT_2048_MODE2_FORM1,	//2048 user bytes, form2m1 sector
	SECFMT_2336_MODE2,			//2336 user bytes, 
};

enum SubcodeFormat
{
	SUBFMT_NONE,				//No subcode info
	SUBFMT_96					//raw 96-byte subcode info
};



struct Session
{
	u32 StartFAD;			//session's start fad
	u8 FirstTrack;			//session's first track
};

struct TrackFile
{
	virtual void Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type) = 0;
	virtual ~TrackFile() {};
};

struct Track
{
	std::unique_ptr<TrackFile> file;	//handler for actual IO
	u32 StartFAD = 0;		//Start FAD
	u32 EndFAD = 0;			//End FAD
	u8 CTRL = 0;
	u8 ADDR = 0;

public:

	Track()
	{
		file = 0;
		StartFAD = 0;
		EndFAD = 0;
		CTRL = 0;
		ADDR = 0;
	}
	bool Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type);
};

struct RawTrackFile : TrackFile
{
	core_file* file;
	s32 offset;
	u32 fmt;
	bool cleanup;

	RawTrackFile(core_file* file, u32 file_offs, u32 first_fad, u32 secfmt);

	virtual void Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type);
	virtual ~RawTrackFile();
};


class Disc
{
public:
	DiscType NullDriveDiscType;
	u8 q_subchannel[96];
	wstring path;
	std::vector<Session> sessions;	//info for sessions
	std::vector<Track> tracks;		//info for tracks
	Track LeadOut;				//info for lead out track (can't read from here)
	u32 EndFAD;					//Last valid disc sector
	DiscType type;
public:
	bool ReadSector(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type);

	void ReadSectors(u32 FAD, u32 count, u8* dst, u32 fmt);

	void FillGDSession();

	void Dump(const string& path);
	
	
public:
	virtual ~Disc() {}
	static std::unique_ptr<Disc> CreateDisc(const std::string& fileName);
	bool ConvertSector(u8* in_buff, u8* out_buff, int from, int to, int sector);
};