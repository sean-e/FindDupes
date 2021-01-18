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


#pragma once

class LogElapsedTime
{
	std::chrono::system_clock::time_point mStartTime = std::chrono::system_clock::now();

public:
	LogElapsedTime() = default;

	~LogElapsedTime()
	{
		std::chrono::system_clock::time_point endTime(std::chrono::system_clock::now());
		LogDuration(endTime - mStartTime);
	}

private:
	void LogDuration(std::chrono::system_clock::duration dur)
	{
		long long secsCnt = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
		if (secsCnt < 120)
			printf("%lld seconds\n", secsCnt);
		else if (secsCnt < (2 * 60 * 60))
			printf("%.02f minutes\n", secsCnt / 60.0);
		else
			printf("%.02f hours\n", secsCnt / 60.0 / 60.0);
	}
};
