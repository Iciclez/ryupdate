#include "signature_export.hpp"
#include "code_generator.hpp"
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

signature_export::signature_export(const std::unordered_map<std::string, std::shared_ptr<signatureitem>> &signatures, const std::string &classname)
{
	this->classname = classname;
	if (this->classname.empty())
	{
		this->classname = "addresses";
	}

	std::unordered_map<std::string, uint32_t> addresses;
	std::unordered_set<std::string> errors;
	std::unordered_map<std::string, std::string> otherdata;

	for (const std::pair<std::string, std::shared_ptr<signatureitem>> &p : signatures)
	{
		std::string n = p.second->data;
		size_t x = 0;
		for (x = 0; x < n.size() && isxdigit(n.at(x)); ++x)
			;

		if (x == n.size())
		{
			addresses[p.first] = std::stoi(n, nullptr, 16);
		}
		else if (!n.compare("ERROR"))
		{
			errors.insert(p.first);
		}
		else
		{
			otherdata[p.first] = n;
		}
	}

	code_generator s(this->classname);
	implementation constructor;

	for (const std::pair<std::string, uint32_t> &p : addresses)
	{
		s.insert_field(code_generator::access_specifier::private_access, "unsigned long " + p.first);

		constructor.write("\t//" + signatures.at(p.first)->signature + " [Result: " + std::to_string(signatures.at(p.first)->result) + "]");
		std::string comment = signatures.at(p.first)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p.first + " = 0x" + signatureitem::uint_to_string<unsigned long>(p.second) + ";\n");

		implementation impl;
		impl.write("\treturn this->" + p.first + ";");

		function f(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p.first, "unsigned long"));
		f.set_implementation(impl);

		s.insert_function(f);
	}

	for (const std::pair<std::string, std::string> &p : otherdata)
	{
		s.insert_field(code_generator::access_specifier::private_access, "std::string " + p.first);

		constructor.write("\t//" + signatures.at(p.first)->signature + " [Result: " + std::to_string(signatures.at(p.first)->result) + "]");
		std::string comment = signatures.at(p.first)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p.first + " = \"" + p.second + "\";\n");

		implementation impl;
		impl.write("\treturn this->" + p.first + ";");

		function f(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p.first, "std::string"));
		f.set_implementation(impl);

		s.insert_function(f);
	}

	for (const std::string &p : errors)
	{
		s.insert_field(code_generator::access_specifier::private_access, "unsigned long " + p);

		constructor.write("\t//" + signatures.at(p)->signature + " [Result: " + std::to_string(signatures.at(p)->result) + "]");
		std::string comment = signatures.at(p)->comments;
		constructor.write_line(comment.empty() ? "" : " {" + comment + "}");
		constructor.write_line("\tthis->" + p + " = SIGNATURE_ERROR;\n");

		implementation impl;
		impl.write("\treturn this->" + p + ";");

		function f(function::access_specifier::public_access, std::make_pair<std::string, std::string>("get_" + p, "unsigned long"));
		f.set_implementation(impl);

		s.insert_function(f);
	}

	s.set_constructor(constructor);

	//s.include_header.push_back("#include <cstdint>");
	s.include_header.push_back("#include <string>");
	s.include_source.push_back("#include \"" + this->classname + ".hpp\"");
	s.include_source.push_back("\n#define SIGNATURE_ERROR static_cast<unsigned long>(-1)");

	this->header = s.get_header();
	this->source = s.get_source();
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

bool signature_export::save(const std::string &filepath, const std::string &data)
{
	std::ofstream fs(filepath);
	if (fs.is_open())
	{
		fs << data;
		fs.close();

		return true;
	}

	return false;
}

std::string signature_export::make_header(const std::unordered_map<std::string, std::shared_ptr<signatureitem>> &signatures, const std::string &prefix)
{
	std::string signature_prefix = prefix;
	if (!signature_prefix.empty())
	{
		if (signature_prefix.at(signature_prefix.size() - 1) != '_')
		{
			signature_prefix += "_";
		}
	}

	std::unordered_set<std::string> definenames;
	std::string text = "#define SIGNATURE_ERROR -1\n\n";
	for (const std::pair<std::string, std::shared_ptr<signatureitem>> &p : signatures)
	{
		text += "//" + p.second->signature + " [Result: " + std::to_string(p.second->result) + "]";
		std::string comment = p.second->comments;
		text += comment.empty() ? "" : " {" + comment + "}";
		text += "\n";

		std::string define_name = signature_prefix + p.first;
		std::transform(define_name.begin(), define_name.end(), define_name.begin(), ::toupper);

		if (definenames.find(define_name) == definenames.end())
		{
			definenames.insert(define_name);
		}
		else
		{
			//contains
			while (definenames.find(define_name) != definenames.end())
			{
				define_name = "_" + define_name;
			}

			definenames.insert(define_name);
		}

		if (!p.second->data.compare("ERROR"))
		{
			text += "#define " + define_name + " SIGNATURE_ERROR";
		}
		else
		{
			std::string n = p.second->data;
			size_t x = 0;
			for (x = 0; x < n.size() && isxdigit(n.at(x)); ++x)
				;

			if (x == n.size())
			{
				//address
				text += "#define " + define_name + " 0x" + signatureitem::uint_to_string<unsigned long>(std::stoi(p.second->data, nullptr, 16));
			}
			else
			{
				//string
				text += "#define " + define_name + " \"" + p.second->data + "\"";
			}
		}

		text += "\n\n";
	}

	return text;
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