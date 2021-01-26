#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>

class implementation
{
public:
	implementation();
	~implementation();

	void write(const std::string &text);
	void write_line(const std::string &text);

	std::string get_code();

private:
	std::string code;
};

class function
{
public:
	enum class access_specifier
	{
		public_access = 1,
		protected_access,
		private_access
	};

	//second arg is function name with function return
	function(access_specifier access, const std::pair<std::string, std::string> &decl);
	~function();

	//arg is parameter name with parameter type
	void insert_parameter(std::pair<std::string, std::string> &param);

	void set_implementation(const implementation &impl);

	implementation &get_implementation();
	std::vector<std::pair<std::string, std::string>> &get_parameters();
	std::pair<std::string, std::string> &get_decl();
	access_specifier &get_access_specifier();

	std::string get_header();
	std::string get_source(const std::string class_name);

private:
	std::pair<std::string, std::string> decl;
	access_specifier access;

	std::vector<std::pair<std::string, std::string>> parameters;
	implementation impl;
};

class code_generator
{
public:
	enum class access_specifier
	{
		public_access = 1,
		protected_access,
		private_access
	};

	//do something about constructors and destructors
	code_generator(const std::string &class_name, const implementation &constructor_impl = implementation(), const implementation &destructor_impl = implementation());
	~code_generator();

	void set_constructor(const implementation &constructor_impl);
	void set_destructor(const implementation &destructor_impl);

	void insert_function(const function &func);
	void insert_field(access_specifier access, const std::string &name);

	std::string get_header();
	std::string get_source();

	std::vector<std::string> include_header;
	std::vector<std::string> include_source;

private:
	std::string class_name;

	std::vector<function> class_function;
	std::vector<std::pair<access_specifier, std::string>> class_field;
	implementation constructor;
	implementation destructor;
};
