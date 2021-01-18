/*
 * Author:
 * Sean Echevarria
 *
 * This software was written by Sean Echevarria in 2020.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2020 Sean Echevarria and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */


#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "LogElapsedTime.h"

extern "C"
{
	#include "md5.h"
};


namespace fs = std::filesystem;

struct HashType
{
	uint64_t mHash1;
	uint64_t mHash2;
};

inline bool operator<(const HashType& _Left, const HashType& _Right) noexcept 
{
	return _Left.mHash1 < _Right.mHash1
		|| (_Left.mHash1 == _Right.mHash1 && _Left.mHash2 < _Right.mHash2);
}

using FileInfo = std::wstring;
using Files = std::vector<FileInfo>;
using HashedFiles = std::map<HashType, Files>;

struct SingleFileOrHashedFiles
{
	// mFile is used to delay hashing of a file until we determine that the 
	// hash is actually required at which point, mFile is moved to mFiles and
	// mFile is no longer used
	FileInfo mFile;

	// collection of hashed files
	HashedFiles mFiles;
};

using FilesBySize = std::map<uintmax_t, SingleFileOrHashedFiles>;

HashType HashFile(std::wstring file);
void AddFile(FilesBySize &fbs, const fs::directory_entry &entry);
void FindFiles(std::wstring path, FilesBySize &fbs);
bool AreFilesIdentical(std::wstring file1, const FileInfo& file2);
void FindDupes(FilesBySize fbs, std::vector<FileInfo> &dupes, bool paranoidCheck, bool reportDupes);
void DeleteDupes(std::vector<FileInfo> &dupes, bool doDelete);
void DisplayHelp();

int
wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		DisplayHelp();
		return 0;
	}

	std::wstring path;
	bool doDelete = false;
	bool reportDupes = false;
	bool paranoidCheck = false;

	for (int i = 1; i < argc; ++i)
	{
		std::wstring tmp;
		switch (i)
		{
		case 1:
			path = argv[i];
			// #consider: support ';' delimited list of directories
			if (!fs::is_directory(path))
			{
				wprintf(L"invalid directory specified\n");
				DisplayHelp();
				return -1;
			}
			break;
		case 2:
		case 3:
		case 4:
			tmp = argv[i];
			if (tmp == L"-d" || tmp == L"/d")
				doDelete = true;
			else if (tmp == L"-p" || tmp == L"/p")
				paranoidCheck = true;
			else if (tmp == L"-r" || tmp == L"/r")
				reportDupes = true;
			else
			{
				wprintf(L"invalid argument: %ls\n", tmp.c_str());
				DisplayHelp();
				return -1;
			}
			break;
		default:
			wprintf(L"invalid argument\n");
			DisplayHelp();
			return -1;
		}
	}

	LogElapsedTime l;
	FilesBySize fbs;

	{
		LogElapsedTime l;
		printf("Processing files...\n");
		FindFiles(path, fbs);
		printf("  step completed in ");
	}

#ifdef HASH_IN_SEPARATE_PASS
	{
		LogElapsedTime l;
		printf("Processing files (2)...\n");
		HashFiles(fbs);
		printf("  step completed in ");
	}
#endif

	std::vector<FileInfo> dupes;

	{
		LogElapsedTime l;
		printf("Finding duplicates...\n");
		FindDupes(fbs, dupes, paranoidCheck, reportDupes);
		printf("  step completed in ");
	}

	if (!dupes.empty())
	{
		LogElapsedTime l;
		if (doDelete)
			printf("Deleting duplicates...\n");
		else
			printf("Reviewing duplicates...\n");

		DeleteDupes(dupes, doDelete);
		printf("  step completed in ");
	}

	std::cout << "Total operation time ";
	return 0;
}

void
DisplayHelp()
{
	printf("\nFindDupes usage:\n"
		"FindDupes 'directory' [-d] [-r] [-p]\n"
		"    -d : delete identified duplicates\n"
		"    -r : report duplicates\n"
		"    -p : paranoid content check (instead of file size + 128bit MD5 hash)\n"
	);
}

HashType
HashFile(std::wstring file)
{
	fs::directory_entry de(file);
	const unsigned long sz = (const unsigned long) de.file_size();
	auto buffer = std::unique_ptr<char[]>{ new char[sz] };

	{
		std::ifstream fin(file, std::ios::in | std::ios::binary);
		fin.read(buffer.get(), sz);
	}

	MD5_CTX ctx;
	MD5_Init(&ctx);
	MD5_Update(&ctx, buffer.get(), sz);

	unsigned char res[16];
	MD5_Final(res, &ctx);

	HashType h;
	h.mHash1 = (*(uint64_t*)&res[0]);
	h.mHash2 = (*(uint64_t*)&res[8]);
	return h;
}

void
AddFile(FilesBySize &fbs, const fs::directory_entry &entry)
{
	auto sizeBucket = fbs.find(entry.file_size());
	if (sizeBucket == fbs.end())
	{
		// bucket is empty; this is the first file of this size.
		// it can be saved without hashing it
		fbs[entry.file_size()] = { entry.path().native() };
	}
	else
	{
		if (sizeBucket->second.mFile.length())
		{
			// a second file was found with the same size as the first.
			// the first file now needs to be hashed and moved to the hashed files map.
			HashType h(HashFile(sizeBucket->second.mFile));
			sizeBucket->second.mFiles[h].push_back(sizeBucket->second.mFile);
			sizeBucket->second.mFile.clear();
		}
		else
			; // third+ file with the same size as the first two

		HashType h(HashFile(entry.path().native()));
		sizeBucket->second.mFiles[h].push_back({ entry.path().native() });
	}

	// wprintf(L"%s\n", entry.path().native().c_str());
}


void
FindFiles(std::wstring path, FilesBySize &fbs)
{
	int foundFileCnt = 0;
	for (const auto & entry : fs::recursive_directory_iterator(path))
	{
		if (entry.is_regular_file())
		{
			++foundFileCnt;
			AddFile(fbs, entry);
		}
// 		else if (entry.is_directory())
// 			FindFiles(entry.path().native(), fbs);
	}

	printf("Files found: %d\n", foundFileCnt);
}

#ifdef HASH_IN_SEPARATE_PASS
void HashFiles(FilesBySize &fbs);

void
HashFiles(FilesBySize &fbs)
{
	// #consider: consider hashing after population rather than during 
	// to make use of concurrency
}
#endif

bool
AreFilesIdentical(std::wstring file1, const FileInfo& file2)
{
	fs::directory_entry de1(file1);
	const unsigned long sz1 = (const unsigned long)de1.file_size();
	fs::directory_entry de2(file2);
	const unsigned long sz2 = (const unsigned long)de2.file_size();

	if (sz1 != sz2)
	{
		printf("logic error: FilesAreIdentical given files with different sizes\n");
		return false;
	}

	auto buffer1 = std::unique_ptr<char[]>{ new char[sz1] };

	{
		std::ifstream fin1(file1, std::ios::in | std::ios::binary);
		fin1.read(buffer1.get(), sz1);
	}

	auto buffer2 = std::unique_ptr<char[]>{ new char[sz2] };

	{
		std::ifstream fin2(file2, std::ios::in | std::ios::binary);
		fin2.read(buffer2.get(), sz2);
	}

	return !memcmp(buffer1.get(), buffer2.get(), sz1);
}

void
FindDupes(FilesBySize fbs, std::vector<FileInfo> &dupes, bool paranoidCheck, bool reportDupes)
{
	int uniquelySizedFiles = 0;
	uintmax_t filesizeSavings = 0;
	for (const auto & outer : fbs)
	{
		if (outer.second.mFiles.empty())
		{
			// single item in mFile rather collection in mFiles
			++uniquelySizedFiles;
			continue;
		}

		auto curSize = outer.first;
		for (const auto & inner : outer.second.mFiles)
		{
			auto curHash = inner.first;
			std::wstring fileInstanceToKeep;
			for (const auto & file : inner.second)
			{
				if (fileInstanceToKeep.empty())
				{
					// keep one file
					fileInstanceToKeep = file;
					continue;
				}

				if (paranoidCheck)
				{
					if (!AreFilesIdentical(fileInstanceToKeep, file))
					{
						wprintf(L"Hash collision: %s %s\n", fileInstanceToKeep.c_str(), file.c_str());
						continue;
					}
				}

				filesizeSavings += curSize;

				// #TODO: need a way to define preference criteria for selecting which dupe to keep

				// determine which dupe to keep (as firstFile)
				// unfiltered is higher delete priority than preferDelete
				bool retainCurrentInstanceToKeep = true;
				if (fileInstanceToKeep.find(L"unfiltered") != -1)
				{
					// keep this file instead
					retainCurrentInstanceToKeep = false;
				}
				else if (file.find(L"unfiltered") != -1)
				{
					// delete this one
				}
				else if (fileInstanceToKeep.find(L"preferDelete") != -1)
				{
					// keep this file instead
					retainCurrentInstanceToKeep = false;
				}
				else if (file.find(L"preferDelete") != -1)
				{
					// delete this one
				}
				else if (fileInstanceToKeep.compare(file) > 1)
				{
					// prefer Abc to Bcd (or 2017 to 2019)
					// keep this file instead
					retainCurrentInstanceToKeep = false;
				}

				if (!retainCurrentInstanceToKeep)
				{
					if (reportDupes)
						wprintf(L"dupe: %s\n  of: %s\n", fileInstanceToKeep.c_str(), file.c_str());

					dupes.push_back(fileInstanceToKeep);
					fileInstanceToKeep = file;
					continue;
				}

				dupes.push_back(file);

				if (reportDupes)
					wprintf(L"dupe: %s\n  of: %s\n", file.c_str(), fileInstanceToKeep.c_str());
			}
		}
	}

	printf("Files with unique sizes: %d\n", uniquelySizedFiles);
	if (dupes.empty())
		printf("No duplicates found\n");
	else
		printf("Duplicates ready to delete: %d for savings of %lld MB\n", dupes.size(), filesizeSavings / 1024 / 1024);
}

void
DeleteDupes(std::vector<FileInfo> &dupes, bool doDelete)
{
	for (const auto & f : dupes)
	{
		if (doDelete)
		{
			wprintf(L"deleting: %s\n", f.c_str());
			if (!fs::remove(f))
				printf("  error: delete failed\n");
		}
		else
			wprintf(L"delete preview: %s\n", f.c_str());
	}
}
