#include "signatureitem.hpp"
#include "aobscan.hpp"

#include "disassembler.hpp"

#include "ryupdate.hpp"
#include <functional>
#include <dbghelp.h>
#include <Psapi.h>

signatureitem::signatureitem(QWidget *parent, const std::string &name, item_type type, const std::string &signature, uint32_t result, const std::string &data, const std::string &comments)
	: QObject(parent)
{
	this->name = name;
	this->type = type;
	this->signature = signature;
	this->result = result;
	this->data = data;
	this->comments = comments;

	namewidget = new QTableWidgetItem(QString::fromStdString(name));

	typewidget = std::move(std::make_unique<QComboBox>(parent));
	typewidget->insertItems(0, QStringList({"Address", "Operator", "Operand 1", "Operand 2", "Operand 3"}));
	typewidget->setFont(QFont("Segoe UI", 8));
	typewidget->setCurrentIndex(this->type - 1);

	signaturewidget = new QTableWidgetItem(QString::fromStdString(aobscan(signature).get_pattern()));
	signaturewidget->setFont(QFont("Consolas", 8));

	resultwidget = std::move(std::make_unique<QSpinBox>(parent));
	resultwidget->setMinimum(1);
	resultwidget->setValue(this->result);
	resultwidget->setAlignment(Qt::AlignCenter);

	datawidget = new QTableWidgetItem(QString::fromStdString(data));
	datawidget->setTextAlignment(Qt::AlignCenter);

	commentswidget = new QTableWidgetItem(QString::fromStdString(comments));

	connect(typewidget.get(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int) 
	{
		this->type = static_cast<item_type>(typewidget->currentIndex() + 1);
	});

	connect(resultwidget.get(), static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) 
	{
		this->result = resultwidget->value();
	});
}

signatureitem::~signatureitem()
{
}

void signatureitem::update_data(const std::pair<unsigned long, size_t> &scanregion)
{
	void *pbase = reinterpret_cast<void *>(scanregion.first);
	size_t size = scanregion.second;

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
		this->datawidget->setText("ERROR");
		return;
	}

	unsigned long address = aobscan(this->signature, pbase, size, this->result).address<unsigned long>();

	if (!address)
	{
		this->datawidget->setText("ERROR");
		return;
	}

	if (type == type_address)
	{
		this->datawidget->setText(QString::fromStdString(uint_to_string<unsigned long>(address)));
	}
	else
	{
		disassembler disassemble(static_cast<uint64_t>(address), z.readmemory(address, 12));
		std::vector<instruction> instructions = disassemble.get_instructions();

		if (type == type_operator)
		{
			this->datawidget->setText(QString::fromStdString(instructions.at(0).mnemonic).toUpper());
		}
		else
		{
			cs_x86 x = instructions.at(0).detail->x86;

			//operand 1 = 3, if type == 3, then 3 - 3 = 0
			size_t operand_index = static_cast<size_t>(type - type_operand_1);

			if (x.op_count < operand_index + 1)
			{
				this->datawidget->setText("ERROR");
			}
			else
			{
				std::function<QString(const cs_x86_op &, uint32_t)> get_operand_data = [&](const cs_x86_op &operand, uint32_t operand_index) -> QString {
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
							this->commentswidget->setText(QString::fromUtf8(cs_reg_name(disassemble.handle, static_cast<x86_reg>(mem.base))).toUpper() + "+" + QString::fromStdString(hexadecimal_to_string(mem.disp)));
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
				this->datawidget->setText(get_operand_data(x.operands[operand_index], operand_index));
			}
		}
	}
}
#ifdef X64
std::string signatureitem::hexadecimal_to_string(qword value)
#else
std::string signatureitem::hexadecimal_to_string(unsigned long value)
#endif
{
	if (value <= 0xff)
	{
		return uint_to_string<byte>(value);
	}

	if (value <= 0xffff)
	{
		return uint_to_string<unsigned short>(value);
	}

#ifdef X64
	if (value <= 0xffffffff)
	{
		return uint_to_string<unsigned long>(value);
	}

	return uint_to_string<qword>(value);
#else
	return uint_to_string<unsigned long>(value);
#endif
}
