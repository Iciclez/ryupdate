#define NOMINMAX

#include "settingswindow.hpp"

#include <QHeaderView>
#include <QMenu>
#include <QEvent>

#include <shlwapi.h>
#include <dbghelp.h>
#include <Psapi.h>

#include "signatureitem.hpp"
#include "zephyrus.hpp"

#include <string>

settingswindow::settingswindow(QWidget *parent)
	: QDialog(parent)
{
	this->setWindowTitle("Ryupdate: Settings");
	this->resize(800, 600);

	layout = std::move(std::make_unique<QHBoxLayout>(this));
	layout->setContentsMargins(2, 2, 2, 2);

	std::function<std::unordered_map<std::string, std::pair<address_t, size_t>>(void)> getmodulesinformation = []() -> std::unordered_map<std::string, std::pair<address_t, size_t>> {
		std::function<std::vector<HMODULE>(void)> getprocessmodules = []() -> std::vector<HMODULE> {
			std::vector<HMODULE> modules;

			HMODULE pmodules[1024];
			DWORD n = 0;

			if (K32EnumProcessModules(GetCurrentProcess(), pmodules, sizeof(pmodules), &n))
			{
				DWORD size = n / sizeof(HMODULE);
				modules.reserve(size);

				for (n = 0; n < size; ++n)
				{
					modules.push_back(pmodules[n]);
				}
			}
			return modules;
		};

		std::unordered_map<std::string, std::pair<address_t, size_t>> modulesinformation;
		std::vector<HMODULE> modules = getprocessmodules();

		modulesinformation.reserve(modules.size());

		for (const HMODULE m : modules)
		{
			std::unique_ptr<char[]> modulepath = std::make_unique<char[]>(MAX_PATH);
			if (K32GetModuleFileNameExA(GetCurrentProcess(), m, modulepath.get(), MAX_PATH))
			{
				MODULEINFO moduleinformation = {0};
				if (K32GetModuleInformation(GetCurrentProcess(), m, &moduleinformation, sizeof(moduleinformation)))
				{
					modulesinformation[std::string(modulepath.get())] = std::make_pair(reinterpret_cast<address_t>(moduleinformation.lpBaseOfDll), static_cast<size_t>(moduleinformation.SizeOfImage));
				}
			}
		}

		return modulesinformation;
	};

	regions = [](const std::unordered_map<std::string, std::pair<address_t, size_t>> &n) -> std::unordered_map<std::string, std::pair<std::string, std::pair<unsigned long, size_t>>> {
		std::unordered_map<std::string, std::pair<std::string, std::pair<unsigned long, size_t>>> r;
		r.reserve(n.size());

		for (const std::pair<std::string, std::pair<unsigned long, size_t>> &p : n)
		{
			std::string filename(PathFindFileNameA(p.first.c_str()));
			r[filename + " (" + signatureitem::uint_to_string<unsigned long>(p.second.first) + "->" + signatureitem::uint_to_string<unsigned long>(p.second.first + p.second.second) + ")"] = std::make_pair(p.first, p.second);
		}

		return r;
	}(getmodulesinformation());

	for (const std::pair<std::string, std::pair<std::string, std::pair<unsigned long, size_t>>> &p : regions)
	{
		regionselection[p.first] = false;
	}

	std::vector<treeviewitem_t> treeviewitems;

	treeviewitems.push_back(treeviewitem_t("General",
										   {{"random string when inserting new signature", itemdata_t([this](bool b) { newsignature_randomstring = b; }, {})}}));

	std::unordered_map<std::string, itemdata_t> itemdatamap;
	for (const std::pair<std::string, bool> &p : regionselection)
	{
		itemdatamap[p.first] = itemdata_t([p, this](bool b) {
			regionselection[p.first] = b;

			if (b)
			{
				for (std::unordered_map<std::string, bool>::iterator it = regionselection.begin(); it != regionselection.end(); ++it)
				{
					if (it->first.compare(p.first)) //does not equal
					{
						it->second = false;

						for (const std::pair<QTreeWidgetItem *, std::function<void(bool)>> &p3 : check_treewidgetitem_handler)
						{
							if (p3.first->checkState(0) == Qt::Checked && !it->first.compare(p3.first->text(0).toStdString()))
							{
								p3.first->setCheckState(0, Qt::Unchecked);
								break;
							}
						}
					}
				}
			}
		},
										  {});
	}

	treeviewitems.push_back(std::make_pair("Regions to Scan", itemdatamap));

	this->insert(treeviewitems);

	this->newsignature_randomstring = 0;
}

settingswindow::~settingswindow()
{
}

size_t settingswindow::insert(size_t treewidget, const treeviewitem_t &item)
{
	if (treewidgets.size() >= treewidget)
	{
		QTreeWidgetItem *pitem = new QTreeWidgetItem({QString::fromStdString(item.first)});

		std::shared_ptr<QTreeWidget> ptreewidget = treewidgets.at(treewidget);

		for (const std::pair<std::string, std::pair<std::function<void(bool)>, std::unordered_map<uint32_t, QWidget *>>> &p : item.second)
		{
			QTreeWidgetItem *treewidgetitem = check_treewidgetitem(p.first, p.second.first);
			pitem->addChild(treewidgetitem);

			for (const std::pair<uint32_t, QWidget *> &p2 : p.second.second)
			{
				ptreewidget->setItemWidget(treewidgetitem, p2.first, p2.second);
			}

			ptreewidget->setColumnCount(std::max(p.second.second.size() + 1, static_cast<uint32_t>(ptreewidget->columnCount())));

			ptreewidget->header()->setStretchLastSection(false);
			ptreewidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		}

		ptreewidget->addTopLevelItem(pitem);
	}

	return treewidget;
}

size_t settingswindow::insert(size_t treewidget, const std::vector<treeviewitem_t> &items)
{
	if (treewidgets.size() >= treewidget)
	{
		for (size_t n = 0; n < items.size(); ++n)
		{
			insert(treewidget, items.at(n));
		}
	}

	return treewidget;
}

size_t settingswindow::insert(const std::vector<std::string> &n)
{
	std::shared_ptr<QTreeWidget> ptreewidget = std::make_shared<QTreeWidget>(this);
	ptreewidget->setFont(QFont("Arial", 9));

	if (n.empty())
	{
		ptreewidget->header()->close();
	}
	else
	{
		for (size_t i = 0; i < n.size(); ++i)
		{
			ptreewidget->headerItem()->setText(i, QString::fromStdString(n.at(i)));
		}
	}

	connect(ptreewidget.get(), &QTreeWidget::itemChanged, [this](QTreeWidgetItem *item, int column) -> void {
		if (check_treewidgetitem_handler.find(item) != check_treewidgetitem_handler.end())
		{
			if (item->checkState(column) == Qt::Checked)
			{
				check_treewidgetitem_handler.at(item)(true);
			}
			else if (item->checkState(column) == Qt::Unchecked)
			{
				check_treewidgetitem_handler.at(item)(false);
			}
		}
	});

	ptreewidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ptreewidget.get(), &QTreeWidget::customContextMenuRequested, [=](const QPoint &) {
		if (ptreewidget->selectedItems().empty())
		{
			return;
		}

		QTreeWidgetItem *item = ptreewidget->selectedItems().at(0);
		std::unique_ptr<QMenu> menu = std::make_unique<QMenu>(this);
		QAction *checkgroup = nullptr, *uncheckgroup = nullptr;

		if (check_treewidgetitem_handler.find(item) == check_treewidgetitem_handler.end())
		{
			checkgroup = menu->addAction("Check " + item->text(0));
			uncheckgroup = menu->addAction("Uncheck " + item->text(0));

			menu->addSeparator();
		}

		QAction *checkall = menu->addAction("Check All");
		QAction *uncheckall = menu->addAction("Uncheck All");

		menu->addSeparator();

		QAction *expandall = menu->addAction("Expand All");
		QAction *collapseall = menu->addAction("Collapse All");

		QAction *result = menu->exec(QCursor::pos());
		if (result == checkgroup || result == uncheckgroup)
		{
			Qt::CheckState state = result == checkgroup ? Qt::Checked : Qt::Unchecked;
			for (int32_t i = 0; i < item->childCount(); ++i)
			{
				item->child(i)->setCheckState(0, state);
			}
		}
		else if (result == checkall || result == uncheckall)
		{
			Qt::CheckState state = result == checkall ? Qt::Checked : Qt::Unchecked;
			for (int32_t i = 0; i < ptreewidget->topLevelItemCount(); ++i)
			{
				for (int32_t j = 0; j < ptreewidget->topLevelItem(i)->childCount(); ++j)
				{
					ptreewidget->topLevelItem(i)->child(j)->setCheckState(0, state);
				}
			}
		}
		else if (result == expandall || result == collapseall)
		{
			for (int32_t i = 0; i < ptreewidget->topLevelItemCount(); ++i)
			{
				ptreewidget->topLevelItem(i)->setExpanded(expandall == result);
			}
		}
	});

	layout->addWidget(ptreewidget.get());

	treewidgets.push_back(ptreewidget);

	return treewidgets.size() - 1;
}

size_t settingswindow::insert(const std::vector<treeviewitem_t> &items)
{
	return this->insert(this->insert(), items);
}

size_t settingswindow::insert(const std::vector<std::string> &n, const std::vector<treeviewitem_t> &items)
{
	return this->insert(this->insert(n), items);
}

QTreeWidgetItem *settingswindow::check_treewidgetitem(const std::string &n, const std::function<void(bool)> &handler)
{
	QTreeWidgetItem *p = new QTreeWidgetItem({QString::fromStdString(n.c_str())});
	p->setCheckState(0, Qt::CheckState::Unchecked);
	check_treewidgetitem_handler[p] = handler;
	return p;
}

bool settingswindow::get_newsignature_randomstring()
{
	return newsignature_randomstring;
}

std::vector<std::pair<unsigned long, size_t>> settingswindow::get_selected_regions()
{
	std::vector<std::pair<unsigned long, size_t>> whitelisted_regions;

	for (const std::pair<std::string, bool> &p : regionselection)
	{
		if (p.second)
		{
			whitelisted_regions.push_back(regions[p.first].second);
		}
	}

	return whitelisted_regions;
}
