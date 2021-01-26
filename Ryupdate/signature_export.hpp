#pragma once
#include "signature_item.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class signature_export
{
public:
	signature_export(const std::unordered_map<std::string, std::shared_ptr<signature_item>> &signatures, const std::string &class_name = "");
	~signature_export();

	bool save_source(const std::string &path);
	bool save_header(const std::string &path);

	static bool save(const std::string &file, const std::string &data);

	static std::string make_header(const std::unordered_map<std::string, std::shared_ptr<signature_item>> &signatures, const std::string &prefix = "");

private:
	std::string class_name;
	std::string header;
	std::string source;
};
