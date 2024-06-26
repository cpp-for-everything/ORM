#ifndef MOKA_UTIL_H
#define MOKA_UTIL_H

#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

namespace Moka
{
	// Representation helpers for types that print funny:
	template <class T> std::string rep(const T& t)
	{
		std::stringstream result;
		result << std::fixed << std::setprecision(7) << t;
		return result.str();
	}

	const char* rep(bool b)
	{
		return b ? "true" : "false";
	}

	const char* rep(const std::nullptr_t&)
	{
		return "nullptr";
	}

	template <class T> std::string rep(const std::vector<T>& vec)
	{
		if (vec.empty())
		{
			return "< >";
		}

		std::stringstream result;
		result << '<' << vec[0];
		for (unsigned int i = 1; i < vec.size(); ++i)
		{
			result << ", " << vec[i];
		}
		result << '>';
		return result.str();
	}

	namespace cli
	{
		template <class T>

#ifdef __linux__
		std::string color(const T& t, int c, bool bold = false)
		{
#endif
#ifndef __linux__
			std::string color(const T& t, int, bool = false)
			{
#endif
				std::stringstream result;
#ifdef __linux__
				result << "\033[" << c;
				if (bold)
				{
					result << ";1";
				}
				result << 'm';
#endif
				result << rep(t);
#ifdef __linux__
				result << "\033[0m";
#endif
				return result.str();
			}

			template <class T> std::string bold(const T& t)
			{
				return color<T>(t, 0, true);
			}

			template <class T> std::string g(const T& t, bool bold = false)
			{
				return color<T>(t, 32, bold);
			}

			template <class T> std::string r(const T& t, bool bold = false)
			{
				return color<T>(t, 31, bold);
			}

			template <class T> std::string y(const T& t, bool bold = false)
			{
				return color<T>(t, 33, bold);
			}
		}
	}

#endif
