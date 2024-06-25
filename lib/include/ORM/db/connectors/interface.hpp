#pragma once

#include <concepts>
#include "../../details/fundamental_type.hpp"

namespace webframe::ORM
{
	namespace details
	{
		class DBDataType
		{
		};
		class IDB
		{
		};
	} // namespace details

	template <typename T>
	concept is_db = std::derived_from<T, details::IDB>;

	template <typename T>
	concept is_db_data_type = std::derived_from<T, details::DBDataType>;

	template <typename DB_type, typename CPP_type> class Connector;

	template <typename _DB_type, details::is_inheritable CPP_type> class Connector<_DB_type, CPP_type> : public details::DBDataType, public CPP_type
	{
		public:
		using DB_type = _DB_type;
		using type = CPP_type;

		Connector(const _DB_type value)
		{
			*this = value;
		}
		Connector(_DB_type& value)
		{
			*this = value;
		}

		operator _DB_type() const;
		operator _DB_type&();
		const _DB_type& operator=(const _DB_type& _value);
	};

	template <typename _DB_type, details::is_not_inheritable CPP_type> class Connector<_DB_type, CPP_type> : public details::DBDataType
	{
		protected:
		CPP_type value;

		public:
		using DB_type = _DB_type;
		using type = CPP_type;

		Connector(const CPP_type value) : value(value)
		{
		}
		Connector(CPP_type& value) : value(value)
		{
		}

		Connector(const _DB_type value)
		{
			*this = value;
		}
		Connector(_DB_type& value)
		{
			*this = value;
		}

		Connector() : value()
		{
		}

		inline operator CPP_type() const
		{
			return value;
		}
		inline operator CPP_type&()
		{
			return value;
		}

		CPP_type operator=(const CPP_type& _value)
		{
			return value = _value;
		}

		operator _DB_type() const;
		operator _DB_type&();
		const _DB_type& operator=(const _DB_type& _value);
	};

} // namespace webframe::ORM
