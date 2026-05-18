#pragma once
#include "defines.h"
#include <string>
#include <vector>
enum class SpecialFolderOption {
	None = 0,
	Create = 0x8000,
	DoNotVerify = 0x4000
};
enum class SpecialFolder {
	ApplicationData = 26,
	CommonApplicationData = 35,
	LocalApplicationData = 28,
	Cookies = 33,
	Desktop = 0,
	Favorites = 6,
	History = 34,
	InternetCache = 0x20,
	Programs = 2,
	MyComputer = 17,
	MyMusic = 13,
	MyPictures = 39,
	MyVideos = 14,
	Recent = 8,
	SendTo = 9,
	StartMenu = 11,
	Startup = 7,
	System = 37,
	Templates = 21,
	DesktopDirectory = 0x10,
	Personal = 5,
	MyDocuments = 5,
	ProgramFiles = 38,
	CommonProgramFiles = 43,
	AdminTools = 48,
	CDBurning = 59,
	CommonAdminTools = 47,
	CommonDocuments = 46,
	CommonMusic = 53,
	CommonOemLinks = 58,
	CommonPictures = 54,
	CommonStartMenu = 22,
	CommonPrograms = 23,
	CommonStartup = 24,
	CommonDesktopDirectory = 25,
	CommonTemplates = 45,
	CommonVideos = 55,
	Fonts = 20,
	NetworkShortcuts = 19,
	PrinterShortcuts = 27,
	UserProfile = 40,
	CommonProgramFilesX86 = 44,
	ProgramFilesX86 = 42,
	Resources = 56,
	LocalizedResources = 57,
	SystemX86 = 41,
	Windows = 36
};
class Environment {
public:
	static std::string CommandLine();
	static std::string CurrentDirectory();
	static void CurrentDirectory(std::string path);
	static std::string SystemDirectory();
	static std::string WindowsDirectory();
	static std::string MachineName();
	static int ProcessorCount();
	static int SystemPageSize();
	static long long WorkingSet();
	static std::string UserName();
	static std::vector<std::string> LogicalDrives();
	static std::string GetFolderPath(SpecialFolder folder, SpecialFolderOption option = SpecialFolderOption::None);
};

