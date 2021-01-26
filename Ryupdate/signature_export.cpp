#include "signature_export.hpp"
#include "code_generator.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

signature_export::signature_export(const std::unordered_map<std::string, std::shared_ptr<signature_item>> &signatures, const std::string &class_name)
{
	this->class_name = class_name;
	if (this->class_name.empty())
	{
		this->class_name = "addresses";
	}

	std::unordered_map<std::string, address_t> addresses;
	std::unordered_set<std::string> errors;
	std::unordered_map<std::string, std::string> tag;

	for (const std::pair<std::string, std::shared_ptr<signature_item>> &p : signatures)
	{
		std::string n = p.second->data;
		size_t x = 0;
		for (x = 0; x < n.size() && isxdigit(n.at(x)); ++x);

		if (x == n.size())
		{
			addresses[p.first] = std::stoull(n, nullptr, 16);
		}
		else if (!n.compare("ERROR"))
		{
			errors.insert(p.first);
		}
		else
		{
			tag[p.first] = n;
		}
	}

	code_generator generator(this->class_name);
	implementation constructor;

	for (const std::pair<std::string, address_t> &p : addresses)
	{
		generator.insert_field(code_generator::access_specifier::private_access, "unsigned long " + p.first);

		constructor.write("\t//" + signatures.at(p.first)->signature + " [Result: " + std::to_string(signatures.at(p.first)->result) + "]");
		std::string comment = signatures.at(p.first)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p.first + " = 0x" + signature_item::uint_to_string<address_t>(p.second) + ";\n");

		implementation impl;
		impl.write("\treturn this->" + p.first + ";");

		function func(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p.first, "unsigned long"));
		func.set_implementation(impl);

		generator.insert_function(func);
	}

	for (const std::pair<std::string, std::string> &p : tag)
	{
		generator.insert_field(code_generator::access_specifier::private_access, "std::string " + p.first);

		constructor.write("\t//" + signatures.at(p.first)->signature + " [Result: " + std::to_string(signatures.at(p.first)->result) + "]");
		std::string comment = signatures.at(p.first)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p.first + " = \"" + p.second + "\";\n");

		implementation impl;
		impl.write("\treturn this->" + p.first + ";");

		function func(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p.first, "std::string"));
		func.set_implementation(impl);

		generator.insert_function(func);
	}

	for (const std::string &p : errors)
	{
		generator.insert_field(code_generator::access_specifier::private_access, "unsigned long " + p);

		constructor.write("\t//" + signatures.at(p)->signature + " [Result: " + std::to_string(signatures.at(p)->result) + "]");
		std::string comment = signatures.at(p)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p + " = SIGNATURE_ERROR;\n");

		implementation impl;
		impl.write("\treturn this->" + p + ";");

		function func(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p, "unsigned long"));
		func.set_implementation(impl);

		generator.insert_function(func);
	}

	generator.set_constructor(constructor);

	//s.include_header.push_back("#include <cstdint>");
	generator.include_header.push_back("#include <string>");
	generator.include_source.push_back("#include \"" + this->class_name + ".hpp\"");
	generator.include_source.push_back("\n#define SIGNATURE_ERROR static_cast<unsigned long>(-1)");

	this->header = generator.get_header();
	this->source = generator.get_source();
}

signature_export::~signature_export()
{
}

bool signature_export::save_source(const std::string &path)
{
	return signature_export::save(path, this->source);
}

bool signature_export::save_header(const std::string &path)
{
	return signature_export::save(path, this->header);
}

bool signature_export::save(const std::string &file, const std::string &data)
{
	std::ofstream fs(file);
	if (fs.is_open())
	{
		fs << data;
		fs.close();

		return true;
	}

	return false;
}

std::string signature_export::make_header(const std::unordered_map<std::string, std::shared_ptr<signature_item>> &signatures, const std::string &prefix)
{
	std::string signature_prefix = prefix;
	if (!signature_prefix.empty())
	{
		if (signature_prefix.at(signature_prefix.size() - 1) != '_')
		{
			signature_prefix += "_";
		}
	}

	std::unordered_set<std::string> defined_names;
	std::stringstream ss;
	ss << "#define SIGNATURE_ERROR -1\n\n";
	for (const std::pair<std::string, std::shared_ptr<signature_item>> &p : signatures)
	{
		ss << "//" << p.second->signature << " [Result: " << std::to_string(p.second->result) << ']';
		std::string comment = p.second->comments;
		if (!comment.empty())
		{
			ss << " {" << comment << '}';
		}
		ss << '\n';

		std::string define_name = signature_prefix + p.first;
		std::transform(define_name.begin(), define_name.end(), define_name.begin(), ::toupper);

		if (!defined_names.count(define_name))
		{
			defined_names.insert(define_name);
		}
		else
		{
			//while set contains define_name
			while (defined_names.count(define_name))
			{
				define_name = "_" + define_name;
			}

			defined_names.insert(define_name);
		}

		if (!p.second->data.compare("ERROR"))
		{
			ss << "#define " << define_name << " SIGNATURE_ERROR";
		}
		else
		{
			std::string n = p.second->data;
			size_t x = 0;
			for (x = 0; x < n.size() && isxdigit(n.at(x)); ++x);

			if (x == n.size())
			{
				//address
				ss << "#define " << define_name << " 0x" << signature_item::uint_to_string<address_t>(std::stoull(p.second->data, nullptr, 16));
			}
			else
			{
				//string
				ss << "#define " << define_name << " \"" << p.second->data << "\"";
			}
		}

		ss << "\n\n";
	}

	return ss.str();
}

/*

int main()
{
std::unordered_map<std::string, uint32_t> addressmapping;
addressmapping["hack1"] = 0xdeadbeef;
addressmapping["hack2"] = 0xbaadf00d;
addressmapping["hack3"] = 0xbaadbeef;

code_generator s("CAddressManager");
implementation constructor;

for (const std::pair<std::string, uint32_t> & p : addressmapping)
{
s.insert_field(code_generator::private_access, "unsigned long " + p.first);

constructor.write_line("\tthis->" + p.first + " = " + std::to_string(p.second).c_str() + ";");

implementation impl;
impl.write("\treturn " + p.first + ";");

function f(function::public_access, std::make_pair<std::string, std::string>("get_" + p.first, "unsigned long"));
f.set_implementation(impl);

s.insert_function(f);
}

s.set_constructor(constructor);
s.include_header.push_back("#include <Windows.h>");
s.include_header.push_back("typedef DWORD unsigned long;");

s.include_source.push_back("#include \"AddressManager.h\"");


printf("Header: \n%s", s.get_header().c_str());
printf("\n\nSource: \n%s", s.get_source().c_str());

getchar();
return 0;
}

*/