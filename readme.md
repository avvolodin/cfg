
# Configure class objects from text file


## Configuration example
*Test.txt*
~~~~
Common
(
	Id=10
	Name=Test string
	Enum=SECOND
	Entity
	(
		Id=33
		Name=Internal1
	)
	Entity
	(
		Id=34
		Name=Internal2
	)
	Entity
	(
		Id=35
		Name=Internal3
	)
)

Entity
(
	Id=10
	Name=Test1
)

Entity
(
	Id= 11
	Name = Test2
)
~~~~
Configurable classes
```cpp

enum class TestEnum
{
	FirstOption, SecondOption, ThirdOption
};
class Common
{
	int Id;
	std::string Name;
	int Enum;
	TestEnum _en;


	std::list<std::shared_ptr<Entity>> entities;
public:
	void setName(std::string name);

	static void config(cfg::section& ps)
	{
		ps.s("Common")
			.p("Id", &Common::Id)
			.p("Name", &Common::setName, cfg::preval([](std::string s)
		{
			std::cout << "PreValidator: " << s.c_str();;
			return s;
		}))
			.e("Enum", { {"FIRST", TestEnum::FirstOption}, {"SECOND", TestEnum::SecondOption}, {"THIRD", TestEnum::ThirdOption} }, &Common::_en)
			.c(cfg::f([](Common* c)
		{
			auto sp = std::make_shared<Entity>();
			c->entities.push_back(sp);
			return sp.get();
		}));
	}
};
class Entity
{
public:
	int Id;
	std::string Name;
	static void config(cfg::section& ps)
	{
		ps.s("Entity")
			.p("Id", &Entity::Id)
			.p("Name", &Entity::Name);
	}

};
```

Perform configuration:

```cpp
int main()
{
	std::string s;
	Common c;
	try {
		std::ifstream f;
		f.open("Test.txt");
		if (f.is_open())
		{
			cfg::configurator cf;
			cf.c(&c);
			cf.config(f);
			f.close();
		}

	} catch (std::exception& e)
	{
		std::cout << e.what();
	}
}
```
