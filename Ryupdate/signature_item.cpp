#include "signature_item.hpp"
#include "aobscan.hpp"

#include "disassembler.hpp"

#include "ryupdate.hpp"
#include <functional>
#include <dbghelp.h>
#include <Psapi.h>

#undef max

signature_item::signature_item(QWidget *parent, const std::string &name, item_type type, const std::string &signature, size_t result, const std::string &data, const std::string &comments)
	: QObject(parent)
{
	this->name = name;
	this->type = type;
	this->signature = signature;
	this->result = result;
	this->data = data;
	this->comments = comments;

	name_widget = new QTableWidgetItem(QString::fromStdString(name));

	type_widget = std::move(std::make_unique<QComboBox>(parent));
	type_widget->insertItems(0, QStringList({"Address", "Operator", "Operand 1", "Operand 2", "Operand 3"}));
	type_widget->setFont(QFont("Segoe UI", 8));
	type_widget->setCurrentIndex(this->type - 1);

	signature_widget = new QTableWidgetItem(QString::fromStdString(aobscan(signature).get_pattern()));
	signature_widget->setFont(QFont("Consolas", 8));

	result_widget = std::move(std::make_unique<QSpinBox>(parent));
	result_widget->setMinimum(1);
	result_widget->setValue(this->result);
	result_widget->setAlignment(Qt::AlignCenter);

	data_widget = new QTableWidgetItem(QString::fromStdString(data));
	data_widget->setTextAlignment(Qt::AlignCenter);

	comments_widget = new QTableWidgetItem(QString::fromStdString(comments));

	connect(type_widget.get(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int) 
	{
		this->type = static_cast<item_type>(type_widget->currentIndex() + 1);
	});

	connect(result_widget.get(), static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) 
	{
		this->result = result_widget->value();
	});
}

signature_item::~signature_item()
{
}

void signature_item::update_data(const std::pair<address_t, size_t> &scan_region)
{
	void *pbase = reinterpret_cast<void *>(scan_region.first);
	size_t size = scan_region.second;

	if (pbase == 0 || size == 0)
	{
		auto getmodulesize = [](HMODULE module, void **pbase, size_t *psize) -> bool 
		{
			if (!module)
			{
				return false;
			}

			if (module == GetModuleHandle(NULL))
			{
				PIMAGE_NT_HEADERS pimage_nt_headers = ImageNtHeader(reinterpret_cast<void *>(module));

				if (pimage_nt_headers == NULL)
				{
					return false;
				}

				*pbase = reinterpret_cast<void *>(module);
				*psize = pimage_nt_headers->OptionalHeader.SizeOfImage;
			}
			else
			{
				MODULEINFO moduleinfo;

				if (!GetModuleInformation(GetCurrentProcess(), module, &moduleinfo, sizeof(MODULEINFO)))
				{
					return false;
				}

				*pbase = moduleinfo.lpBaseOfDll;
				*psize = moduleinfo.SizeOfImage;
			}

			return true;
		};

		getmodulesize(GetModuleHandle(0), &pbase, &size);
	}

	if (this->signature.empty())
	{
		this->data_widget->setText("ERROR");
		return;
	}

	unsigned long address = aobscan(this->signature, pbase, size, this->result).address<unsigned long>();

	if (!address)
	{
		this->data_widget->setText("ERROR");
		return;
	}

	if (type == type_address)
	{
		this->data_widget->setText(QString::fromStdString(uint_to_string<address_t>(address)));
	}
	else
	{
		disassembler disassemble(static_cast<address_t>(address), z.readmemory(address, 12));
		std::vector<instruction> instructions = disassemble.get_instructions();

		if (type == type_operator)
		{
			this->data_widget->setText(QString::fromStdString(instructions.at(0).mnemonic).toUpper());
		}
		else
		{
			cs_x86 x = instructions.at(0).detail->x86;

			//operand 1 = 3, if type == 3, then 3 - 3 = 0
			size_t operand_index = static_cast<size_t>(type - type_operand_1);

			if (x.op_count < operand_index + 1)
			{
				this->data_widget->setText("ERROR");
			}
			else
			{
				std::function<QString(const cs_x86_op &, size_t)> get_operand_data = [&](const cs_x86_op &operand, size_t operand_index) -> QString {
					switch (operand.type)
					{
					case X86_OP_REG:
						return QString::fromUtf8(cs_reg_name(disassemble.handle, x.operands[operand_index].reg)).toUpper();

					case X86_OP_IMM:
						return QString::fromStdString(hexadecimal_to_string(x.operands[operand_index].imm));

					case X86_OP_MEM:
						x86_op_mem mem = x.operands[operand_index].mem;

						printf("OPERATION_MEMORY_DETECTED (%s)\nBase: %d\tDisp: %llX\tIndex: %d\tScale: %d\tSegment: %d\n\n", this->name.c_str(), mem.base, mem.disp, mem.index, mem.scale, mem.segment);

						if (mem.base != 0)
						{
							this->comments_widget->setText(QString::fromUtf8(cs_reg_name(disassemble.handle, static_cast<x86_reg>(mem.base))).toUpper() + "+" + QString::fromStdString(hexadecimal_to_string(mem.disp)));
						}
						if (mem.disp != 0)
						{
							return QString::fromStdString(hexadecimal_to_string(mem.disp));
						}
						if (mem.base != 0)
						{
							return QString::fromUtf8(cs_reg_name(disassemble.handle, static_cast<x86_reg>(mem.base))).toUpper();
						}

					default:
						return "ERROR";
					}
				};

				//
				this->data_widget->setText(get_operand_data(x.operands[operand_index], operand_index));
			}
		}
	}
}

std::string signature_item::hexadecimal_to_string(address_t value)
{
	if (value <= std::numeric_limits<uint8_t>::max())
	{
		return uint_to_string<uint8_t>(value);
	}

	if (value <= std::numeric_limits<uint16_t>::max())
	{
		return uint_to_string<uint16_t>(value);
	}

	if (value <= std::numeric_limits<uint32_t>::max())
	{
		return uint_to_string<uint32_t>(value);
	}

	return uint_to_string<uint64_t>(value);
}
