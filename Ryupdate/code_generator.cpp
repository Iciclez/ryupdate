#include "code_generator.hpp"

//std::multimap
#include <map>

implementation::implementation()
{
}

implementation::~implementation()
{
}

void implementation::write(const std::string &text)
{
	this->code += text;
}

void implementation::write_line(const std::string &text)
{
	write(text + "\n");
}

std::string implementation::get_code()
{
	return code;
}

function::function(access_specifier access, const std::pair<std::string, std::string> &decl)
{
	this->access = access;
	this->decl = decl;
}

function::~function()
{
}

void function::insert_parameter(std::pair<std::string, std::string> &param)
{
	parameters.push_back(param);
}

void function::set_implementation(const implementation &impl)
{
	this->impl = impl;
}

implementation &function::get_implementation()
{
	return impl;
}

std::vector<std::pair<std::string, std::string>> &function::get_parameters()
{
	return this->parameters;
}

std::pair<std::string, std::string> &function::get_decl()
{
	return this->decl;
}

function::access_specifier &function::get_access_specifier()
{
	return this->access;
}

std::string function::get_header()
{
	std::stringstream argument;
	for (size_t n = 0; n < parameters.size(); ++n)
	{
		argument << parameters.at(n).second << ' ' << parameters.at(n).first;

		argument << (n + 1 != parameters.size()) ? ", " : ");";
	}

	return decl.second + " " + decl.first + "(" + (parameters.size() == 0 ? ")" : argument.str());
}

std::string function::get_source(const std::string class_name)
{
	std::stringstream argument;
	for (size_t n = 0; n < parameters.size(); ++n)
	{
		argument << parameters.at(n).second << ' ' << parameters.at(n).first;

		argument << (n + 1 != parameters.size()) ? ", " : ");";
	}
	std::stringstream text;
	text << decl.second << ' ' << class_name << "::" << decl.first << '(' << (parameters.size() == 0 ? ")" : argument.str());
	text << "\n{\n"
		 << impl.get_code() << "\n}\n";
	return text.str();
}

code_generator::code_generator(const std::string &class_name, const implementation &constructor_impl, const implementation &destructor_impl)
{
	this->class_name = class_name;
	this->constructor = constructor_impl;
	this->destructor = destructor_impl;
}

code_generator::~code_generator()
{
}

void code_generator::set_constructor(const implementation &constructor_impl)
{
	this->constructor = constructor_impl;
}

void code_generator::set_destructor(const implementation &destructor_impl)
{
	this->destructor = destructor_impl;
}

void code_generator::insert_function(const function &func)
{
	class_function.push_back(func);
}

void code_generator::insert_field(access_specifier access, const std::string &name)
{
	class_field.push_back(std::make_pair(access, name));
}

std::string code_generator::get_header()
{
	std::stringstream header;

	for (const std::string &src : include_header)
	{
		header << src << '\n';
	}

	header << '\n';

	header << "class " << class_name << "\n{\n";

	std::multimap<function::access_specifier, function> function_graph;
	for (function &f : class_function)
	{
		function_graph.insert(std::make_pair(f.get_access_specifier(), f));
	}

	std::multimap<code_generator::access_specifier, std::string> field_graph;
	for (const std::pair<access_specifier, std::string> &p : class_field)
	{
		field_graph.insert(p);
	}

	for (int32_t i = 1; i < 4; ++i)
	{
		auto function_range = function_graph.equal_range(static_cast<function::access_specifier>(i));
		auto field_range = field_graph.equal_range(static_cast<code_generator::access_specifier>(i));

		if (function_range.first == function_range.second && field_range.first == field_range.second && i != 1)
		{
			continue;
		}

		switch (i)
		{
		case 1:
			header << "public:\n";
			//make constructor and destructor
			{
				header << '\t' << this->class_name << "();\n";
				header << "\t~" << this->class_name << "();\n\n";
			}
			break;
		case 2:
			header << "protected:\n";
			break;
		case 3:
			header << "private:\n";
			break;
		}

		for (auto it = function_range.first; it != function_range.second; ++it)
		{
			header << '\t' << it->second.get_header() << ";\n";
		}

		header << '\n';

		for (auto it = field_range.first; it != field_range.second; ++it)
		{
			header << '\t' << it->second << ";\n";
		}
	}

	header << "\n}\n";

	return header.str();
}

std::string code_generator::get_source()
{
	std::stringstream source;

	for (const std::string &src : include_source)
	{
		source << src << '\n';
	}
	source << '\n';

	source << this->class_name << "::" << this->class_name << "()\n{\n";
	source << constructor.get_code() << "\n}\n\n";

	source << this->class_name << "::~" << this->class_name << "()\n{";
	source << destructor.get_code() << "\n}\n\n";

	for (function &func : class_function)
	{
		source << func.get_source(this->class_name) << '\n';
	}

	return source.str();
}
