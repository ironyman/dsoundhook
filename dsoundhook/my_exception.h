#pragma once
#include <stdexcept>
#include <string>
#include <sstream>

class my_exception : public std::runtime_error
{
	std::string msg;
public:
	template <typename T>
	my_exception(T const& arg, const char* file, int line, const char* fn)
		: std::runtime_error(arg)
	{
		std::ostringstream o;
		o << file << ":" << line << ": " << ":" << fn << ": " << arg;
		msg = o.str();
	}
	~my_exception() throw() {}
	const char *what() const throw()
	{
		return msg.c_str();
	}
	const wchar_t *wwhat() const throw()
	{
		std::wstring ws(msg.begin(), msg.end());
		return ws.c_str();
	}
};

#define my_throw(arg) throw my_exception(arg, __FILE__, __LINE__, __FUNCTION__);
