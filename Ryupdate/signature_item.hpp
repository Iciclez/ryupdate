#pragma once
#include <QObject>
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>

#include <memory>
#include <sstream>
#include <iomanip>

#include "zephyrus.hpp"

class signature_item : public QObject
{
	Q_OBJECT

public:
	enum item_type : uint32_t
	{
		type_address = 1,
		type_operator,
		type_operand_1,
		type_operand_2,
		type_operand_3
	};

	explicit signature_item(
		QWidget *parent,
		const std::string &name,
		item_type type = type_address,
		const std::string &signature = "",
		size_t result = 1,
		const std::string &data = "",
		const std::string &comments = "");

	~signature_item();

	//should not write to, read only
	std::string name;
	item_type type;
	std::string signature;
	size_t result;
	std::string data;
	std::string comments;

	QTableWidgetItem *name_widget;
	std::unique_ptr<QComboBox> type_widget;
	QTableWidgetItem *signature_widget;
	std::unique_ptr<QSpinBox> result_widget;
	QTableWidgetItem *data_widget;
	QTableWidgetItem *comments_widget;

	void update_data(const std::pair<address_t, size_t> &scan_region);

	static std::string hexadecimal_to_string(address_t value);

	template <typename T>
	static std::string uint_to_string(T value);
};

template <typename T>
inline std::string signature_item::uint_to_string(T value)
{
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(sizeof(T) * 2) << std::uppercase << std::hex << static_cast<address_t>(value);
	return ss.str();
}
