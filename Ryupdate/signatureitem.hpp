#pragma once
#include <QObject>
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>

#include <memory>
#include <sstream>
#include <iomanip>

class signatureitem : public QObject
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

	explicit signatureitem(
		QWidget *parent,
		const std::string &name,
		item_type type = type_address,
		const std::string &signature = "",
		uint32_t result = 1,
		const std::string &data = "",
		const std::string &comments = "");

	~signatureitem();

	//should not write to, read only
	std::string name;
	item_type type;
	std::string signature;
	uint32_t result;
	std::string data;
	std::string comments;

	QTableWidgetItem *namewidget;
	std::unique_ptr<QComboBox> typewidget;
	QTableWidgetItem *signaturewidget;
	std::unique_ptr<QSpinBox> resultwidget;
	QTableWidgetItem *datawidget;
	QTableWidgetItem *commentswidget;

	void update_data(const std::pair<unsigned long, size_t> &scanregion);

#ifdef X64
	static std::string hexadecimal_to_string(qword value);
#else
	static std::string hexadecimal_to_string(unsigned long value);
#endif

	template <typename T>
	static std::string uint_to_string(T value);
};

template <typename T>
inline std::string signatureitem::uint_to_string(T value)
{
	std::stringstream ss;
#ifdef X64
	ss << std::setfill('0') << std::setw(sizeof(T) * 2) << std::uppercase << std::hex << static_cast<qword>(value);
#else
	ss << std::setfill('0') << std::setw(sizeof(T) * 2) << std::uppercase << std::hex << static_cast<unsigned long>(value);
#endif
	return ss.str();
}
