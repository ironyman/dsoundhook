#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <stdexcept>

class redirect_cerr
{
	std::fstream log_file;
	std::streambuf *const cerr_buf;
public:
	redirect_cerr(std::string const& path)
		: cerr_buf(std::cerr.rdbuf())
		, log_file(path.c_str(), std::fstream::out)
	{
		if (!log_file.is_open())
		{
			throw new std::runtime_error("Failed to open log file.");
		}
		log_file.rdbuf()->pubsetbuf(0, 0);
		std::cerr.rdbuf(log_file.rdbuf());
	}

	~redirect_cerr() {
		std::cerr.rdbuf(cerr_buf);
		log_file.close();
	}
};