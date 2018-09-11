#pragma once
#include <functional>
#include <string>
#include <map>
#include <utility>
#include <type_traits>
#include <vector>
#include <utility>
#include <algorithm>
#include <list>
#include <locale>
#include <cctype>
#include <sstream>
#include <stack>
#include <optional>
#include <set>

namespace cfg
{

	template<typename F>
	struct function_traits
		: public function_traits<decltype(&F::operator())>
	{};
	template<typename C, typename R, typename... Args>
	struct function_traits<R(C::*)(Args...) const>
	{
		typedef R(*pointer)(Args...);
		typedef std::function<R(Args...)> function;
	};
	template<typename F>
	typename function_traits<F>::function
	f(F lambda)
	{
		return typename function_traits<F>::function(lambda);
	};

	static inline void ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](auto ch) {
			return !std::isspace(static_cast<unsigned char>(ch));
		}));
	}

	// trim from end (in place)
	static inline void rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](auto ch) {
			return !std::isspace(static_cast<unsigned char>(ch));
		}).base(), s.end());
	}

	// trim from both ends (in place)
	static inline void trim(std::string &s) {
		ltrim(s);
		rtrim(s);
	}

	enum class sopt
	{
		Unique
	};

	enum class popt
	{
		SectionKey, Many
	};

	//template <typename TP>
	struct preval
	{
		std::function<std::string(std::string)> v;
		template <typename T>
		preval(T t)
		{
			v = f(t);
		}
	};

	template <typename TP>
	struct postval
	{
		std::function<std::string(TP)> v;
	};

	class parambase
	{
	public:
		virtual void apply(void* obj, std::string_view value) = 0;
	};


	template <typename Object, typename TP>
	class param : public parambase
	{
		std::function<void(TP, Object*)> f;
		std::vector<std::pair<std::string, TP>> _m{};

		std::optional<std::function<std::string(TP)>> _postval{std::nullopt};
		std::optional<std::function<std::string(std::string)>> _preval{std::nullopt};
		std::set<popt> opts;
		//std::optional<std::function<TP(TP)>> m;
		//std::optional<std::function<TP(std::string)>> ct;
		template<typename ...O>
		void Option(popt o, O... r)
		{
			opts.insert(o);
			Option(r...);
		}

		template<typename ...O>
		void Option(postval<TP> pv, O... r)
		{
			_postval = pv.v;
			Option(r...);
		}
		template<typename ...O>
		void Option(preval pv, O... r)
		{
			_preval = pv.v;
			Option(r...);
		}
		template<typename ...O>
		void Option()
		{	
		}
	public:


		template<typename ...O>
		param(TP Object::* ptr, O... opts)
		{
			f = [ptr](TP x, Object* o)
			{
				o->*ptr = x;
			};
			Option(opts...);
		};

		template<typename ...O>
		param(void (Object::*fp)(TP), O... opts)
		{
			f = [fp](TP x, Object* o)
			{
				(o->*fp)(x);
			};
			Option(opts...);
		}

		template<typename ...O>
		param(std::function<void(TP, Object*)> fp, O... opts)
		{
			f = fp;
			Option(opts...);
		}

		template<typename ...O>
		param(TP Object::* ptr, std::vector<std::pair<std::string, TP>>&& m, O... opts)
		{
			_m = std::move(m);
			f = [ptr](TP x, Object* o)
			{
				o->*ptr = x;
			};
			Option(opts...);
		}


		//bool match(std::string_view name) const { return this->name == name; }
		void apply(void* obj, std::string_view value)
		{
			auto to = static_cast<Object*>(obj);
			if(_preval)
			{
				auto diag = (*_preval)(std::string(value));
			}
			if(!_m.empty())
			{
				auto r = std::find_if(_m.begin(), _m.end(), [value](auto& v) {return v.first == value; });
				if(r==_m.end())
				{
					auto errm = std::string{ "Absent option " } +std::string{ value.data() };
					throw std::range_error(errm.c_str());
				}
				f((*r).second, to);
			}
			else 
			{
				if constexpr (std::is_same<TP, std::string>().value)
				{
					f(std::string{ value }, to);
				}
				else if constexpr (std::is_integral<TP>().value)
				{
					f(std::stoi(std::string{ value }), to);
				}
				else if constexpr (std::is_floating_point<TP>().value)
				{
					f(std::stod(std::string{ value }), to);
				}
			}
		}


	};
	template <typename Object, typename TP> 
		param(TP Object::*)->param<Object, TP>;
	
	class section;

	template<typename, typename = std::void_t<>>
	struct ConfigurableClass : std::false_type {
		};
	template<typename T>
	struct ConfigurableClass<T, std::void_t<decltype(&T::config)>>
			: std::true_type {
		};

	class section
	{

		struct SCS
		{
			std::shared_ptr<section> ps;
			std::function<void*(void*)> f;
		};

		std::string _name;
		std::map<std::string, std::shared_ptr<parambase>> _pm{};

	public:
		std::map<std::string, SCS> _sc;
		section& s(std::string name)
		{
			_name = name;
			return *this;
		}

		template<typename T, typename... O>
		section& p(std::string key, T v, O... o)
		{
			parambase* p = new param(v, o...);
			_pm.emplace(std::make_pair(key, std::shared_ptr<parambase>(p)));
			return *this;
		}

		template<typename Tv, typename Object>
		section& e(std::string key, std::vector<std::pair<std::string, Tv>>&& m, Tv Object::* ptr)
		{
			parambase* p = new param(ptr, std::forward<std::vector<std::pair<std::string, Tv>>>(m));
			_pm.emplace(std::make_pair(key, std::shared_ptr<parambase>(p)));
			return *this;
		}

		template<typename T, typename Object>
		section& c(std::function<T*(Object*)> f)
		{

			static_assert(ConfigurableClass<typename std::decay<T>::type>::value, "Not configurable class");
			auto ps = std::make_shared<section>();
			T::config(*ps);
			_sc[ps->getName()] = SCS{ ps, [f](void* o) { Object* ob = static_cast<Object*>(o);  return f(ob); } };
			return *this;
		}
		//template<typename T>
		//section& c(T* o)
		//{
		//	static_assert(ConfigurableClass<typename std::decay<T>::type>::value, "Not configurable class");
		//	auto ps = std::make_shared<section>();
		//	T::config(*ps);
		//	void* p = static_cast<void*>(o);
		//	_sc[ps->getName()] = SCS{ ps, [p]() {return p; } };
		//	return *this;
		//}

		bool apply(void* obj, const std::string& name, const std::string& val)
		{
			if (auto p = _pm.find(name); p != _pm.end())
			{
				p->second->apply(obj, val);
				return true;
			}
			return false;
		}

		std::string getName()
		{
			return _name;
		}

		friend std::ostream& operator<< (std::ostream& ss, section& s)
		{
			ss << s._name << std::endl;
			for(auto p : s._pm)
			{
				ss << "  " << p.first.c_str() << std::endl;
			}
			
			return ss;
		}
	};


	enum class ContextState
	{
		Out, Section, Custom
	};




	class configurator
	{
		struct SC
		{
			std::shared_ptr<section> ps;
			std::function<void*()> f;
		};

		struct CSC
		{
			std::shared_ptr<section> ps;
			void* obj;
		};

		std::map<std::string, SC> _sc;
		std::map<std::string, std::function<void(std::string)>> _cs;

		std::string CurrentFileName;
		ContextState State;
		std::string NextSectionName;
		std::stringstream Custom;
		int CustomOb = 0;
		std::list<std::string> diag;

		std::stack<CSC> _sections{};

		std::string fmt(int lc, const char* msg)
		{
			std::stringstream ss;
			ss << msg << " [at line " << lc << " of file " << CurrentFileName << "]";
			return ss.str();
		}
		
		void except(int lc, const char* msg)
		{
			throw std::range_error(fmt(lc, std::forward<const char*>(msg)));
		}

		void PushSection(int lc)
		{
			if(State == ContextState::Out)
			{
				if(auto sc = _sc.find(NextSectionName); sc!=_sc.end())
				{
					auto obj = sc->second.f();
					CSC c;
					c.obj = obj;
					c.ps = sc->second.ps;
					_sections.push(c);
					NextSectionName = "";
					State = ContextState::Section;
				}
				else if (auto cs = _cs.find(NextSectionName); cs != _cs.end())
				{
					Custom.str("");
					State = ContextState::Custom;
					CustomOb = 0;
				}
				else
				{
					diag.push_back(fmt(lc, (std::string("Unknown section ") + NextSectionName).c_str()));
					Custom.str("");
					State = ContextState::Custom;
					CustomOb = 0;
				}
			} else
			{
				if (auto sc = _sections.top().ps->_sc.find(NextSectionName); sc != _sections.top().ps->_sc.end())
				{
					auto obj = sc->second.f(_sections.top().obj);
					CSC c;
					c.obj = obj;
					c.ps = sc->second.ps;
					_sections.push(c);
					NextSectionName = "";
					State = ContextState::Section;
				}
				else
				{
					except(lc, "Unknown subsection");
				}

			}
		}

		void PopSection(int lc)
		{
			if(State == ContextState::Custom)
			{
				if (auto cs = _cs.find(NextSectionName); cs != _cs.end())
				{
					(cs->second)(Custom.str());
				}
				State = ContextState::Out;
				NextSectionName = "";
				Custom.str("");
			}
			else if(State == ContextState::Section)
			{
				_sections.pop();
				if(_sections.empty())
					State = ContextState::Out;
			}
			else
			{
				except(lc, "Unexpected end of section");
			}

		}

		void Param(int lc, std::string p, std::string v)
		{
			auto s = _sections.top();
			try {
				s.ps->apply(s.obj, p, v);
			} catch(std::exception& e)
			{
				except(lc, e.what());
			}

		}

	public:
		template<typename T>
		configurator& c(std::function<T*()> f)
		{

			static_assert(ConfigurableClass<typename std::decay<T>::type>::value, "Not configurable class");
			auto ps = std::make_shared<section>();
			T::config(*ps);
			_sc[ps->getName()] = SC{ ps, [f]() {return f(); } };
			return *this;
		}
		template<typename T>
		configurator& c(T* o)
		{
			static_assert(ConfigurableClass<typename std::decay<T>::type>::value, "Not configurable class");
			auto ps = std::make_shared<section>();
			T::config(*ps);
			void* p = static_cast<void*>(o);
			_sc[ps->getName()] = SC{ ps, [p]() {return p; } };
			return *this;
		}

		configurator& c(const std::string& name, std::function<void(std::string)> f)
		{
			_cs[name] = f;
			return *this;
		}

		friend std::ostream& operator<<(std::ostream& ss, configurator& b)
		{
			for(auto& p : b._sc)
			{
				ss << p.first.c_str();
				ss << "  ->  " << *(p.second.ps) << std::endl;
			}

			return ss;
		}
		
		void config(std::istream& istr)
		{
			int lc = 0;
			std::string line;
			while(std::getline(istr, line))
			{
				lc++;
				trim(line);
				if(line.length() != 0 && line.find("//")!=0)
				{
					if(line.find("#include") == 0)
					{
						//incl
					}
					else
					{
						switch (State)
						{
						case ContextState::Out:
							if (line.find('=') != std::string::npos)
							{
								except(lc, "Parameter found out of section");
							}
							else if (line == ")")
							{
								except(lc, "Extra ')'");
							}
							else if (line == "(")
							{
								if (NextSectionName.length() != 0)
								{
									PushSection(lc);
								}
								else
								{
									except(lc, "Unexpected '('");
								}
							}
							else
							{
								if (NextSectionName.length() == 0)
								{
									NextSectionName = line;
								}
								else
								{
									except(lc, "Duplicate section name.");
								}
							}
							break;
						case ContextState::Section:
							if (auto ep=line.find('='); ep != std::string::npos)
							{
								Param(lc, line.substr(0, ep), line.substr(ep + 1, std::string::npos));
							}
							else if (line == ")")
							{
								PopSection(lc);
							}
							else if (line == "(")
							{
								if (NextSectionName.length() != 0)
								{
									PushSection(lc);
								}
								else
								{
									except(lc, "Unexpected '('");
								}
							}
							else
							{
								if (NextSectionName.length() == 0)
								{
									NextSectionName = line;
								}
								else
								{
									except(lc, "Duplicate section name.");
								}
							}
							break;
						case ContextState::Custom:
							if(line == ")")
							{
								if (CustomOb == 0)
								{
									PopSection(lc);
								}
								else
								{
									CustomOb--;
								}
							}
							else if(line == "(")
							{
								CustomOb++;
							}
							else
							{
								Custom << line;
							}
							break;
						default:;
						}
					}
				}
			}
		}

	};
}
