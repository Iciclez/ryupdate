#pragma once
#include "signatureitem.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class signature_export
{
public:
	signature_export(const std::unordered_map<std::string, std::shared_ptr<signatureitem>> &signatures, const std::string &classname = "");
	~signature_export();

	bool save_source(const std::string &path);
	bool save_header(const std::string &path);

	static bool save(const std::string &filepath, const std::string &data);

	static std::string make_header(const std::unordered_map<std::string, std::shared_ptr<signatureitem>> &signatures, const std::string &prefix = "");

private:
	std::string classname;
	std::string header;
	std::string source;
};
