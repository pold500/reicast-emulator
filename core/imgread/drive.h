#pragma once
#include "types.h"
#include "Disc.h"

std::unique_ptr<Disc> chd_parse(const std::string& fileName);
std::unique_ptr<Disc> gdi_parse(const std::string& fileName);
std::unique_ptr<Disc> cdi_parse(const std::string& fileName);

#if HOST_OS==OS_WINDOWS
std::unique_ptr<Disc> ioctl_parse(const std::string& fileName);
#endif

class Drive
{
	bool InitDrive_(const std::string& fileName);
	bool InitDrive(u32 fileflags/*=0*/);
	void TermDrive();
	u32 CreateTrackInfo(u32 ctrl, u32 addr, u32 fad);
	u32 CreateTrackInfo_se(u32 ctrl, u32 addr, u32 tracknum);
	void GetDriveSessionInfo(u8* to, u8 session);
	void printtoc(TocInfo* toc, SessionInfo* ses);
	DiscType GuessDiscType(bool m1, bool m2, bool da);
	
	bool ConvertSector(u8* in_buff, u8* out_buff, int from, int to, int sector);
	bool InitDrive(u32 fileflags = 0);
	void TermDrive();
	bool DiscSwap(u32 fileflags = 0);


	void PatchRegion_0(u8* sector, int size);
	void PatchRegion_6(u8* sector, int size);
	void ConvToc(u32* to, TocInfo* from);
	void GetDriveToc(u32* to, DiskArea area);
	void GetDriveSector(u8 * buff, u32 StartSector, u32 SectorCount, u32 secsz);

	void GetDriveSessionInfo(u8* to, u8 session);
	int GetFile(std::string& diskFileName);
	int msgboxf(wchar* text, unsigned int type, ...);
	void printtoc(TocInfo* toc, SessionInfo* ses);
	u8 q_subchannel[96];
	DiscType m_NullDriveDiscType;
	std::unique_ptr<Disc> m_disk;
	
};