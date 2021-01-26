#define NOMINMAX

#include "settingswindow.hpp"

#include <QHeaderView>
#include <QMenu>
#include <QEvent>

#include <shlwapi.h>
#include <dbghelp.h>
#include <Psapi.h>

#include "signature_item.hpp"
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

	auto modules_information = getmodulesinformation();
	std::unordered_map<std::string, std::pair<std::string, std::pair<address_t, size_t>>> regions;
	regions.reserve(modules_information.size());

	for (const auto& p : modules_information)
	{
		std::string region_key = std::string(PathFindFileName(p.first.c_str())) + " (" + signature_item::uint_to_string<address_t>(p.second.first) + "->" + signature_item::uint_to_string<address_t>(p.second.first + p.second.second) + ")";
		regions[region_key] = std::make_pair(p.first, p.second);
		region_selection[region_key] = false;
	}

	std::vector<treeviewitem_t> treeview_items;

	treeview_items.push_back(treeviewitem_t("General",
										   {{"random string when inserting new signature", item_value_t([this](bool b) { newsignature_randomstring = b; }, {})}}));

	std::unordered_map<std::string, item_value_t> item_values;
	for (const std::pair<std::string, bool> &p : region_selection)
	{
		item_values[p.first] = item_value_t([p, this](bool b) {
			region_selection[p.first] = b;

			if (b)
			{
				for (std::unordered_map<std::string, bool>::iterator it = region_selection.begin(); it != region_selection.end(); ++it)
				{
					if (it->first.compare(p.first)) //does not equal
					{
						it->second = false;

						for (const auto &handler_pair : check_treewidgetitem_handler)
						{
							if (handler_pair.first->checkState(0) == Qt::Checked && !it->first.compare(handler_pair.first->text(0).toStdString()))
							{
								handler_pair.first->setCheckState(0, Qt::Unchecked);
								break;
							}
						}
					}
				}
			}
		},
										  {});
	}

	treeview_items.push_back(std::make_pair("Regions to Scan", item_values));

	this->insert(treeview_items);

	this->newsignature_randomstring = 0;
}

settingswindow::~settingswindow()
{
}

size_t settingswindow::insert(size_t tree_widget, const treeviewitem_t &item)
{
	if (tree_widgets.size() >= tree_widget)
	{
		QTreeWidgetItem *pitem = new QTreeWidgetItem({QString::fromStdString(item.first)});

		std::shared_ptr<QTreeWidget> ptreewidget = tree_widgets.at(tree_widget);

		for (const auto &p : item.second)
		{
			QTreeWidgetItem *treewidgetitem = check_treewidgetitem(p.first, p.second.first);
			pitem->addChild(treewidgetitem);

			for (const auto &q : p.second.second)
			{
				ptreewidget->setItemWidget(treewidgetitem, q.first, q.second);
			}

			ptreewidget->setColumnCount(std::max(p.second.second.size() + 1, static_cast<size_t>(ptreewidget->columnCount())));

			ptreewidget->header()->setStretchLastSection(false);
			ptreewidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		}

		ptreewidget->addTopLevelItem(pitem);
	}

	return tree_widget;
}

size_t settingswindow::insert(size_t tree_widget, const std::vector<treeviewitem_t> &items)
{
	if (tree_widgets.size() >= tree_widget)
	{
		for (size_t n = 0; n < items.size(); ++n)
		{
			this->insert(tree_widget, items.at(n));
		}
	}

	return tree_widget;
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

	connect(ptreewidget.get(), &QTreeWidget::itemChanged, [this](QTreeWidgetItem *item, int column) -> void 
	{
		if (check_treewidgetitem_handler.count(item))
		{
			switch (item->checkState(column))
			{
			case Qt::Checked:
				this->check_treewidgetitem_handler.at(item)(true);
				break;

			case Qt::Unchecked:
				this->check_treewidgetitem_handler.at(item)(false);
				break;
			}
		}
	});

	ptreewidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ptreewidget.get(), &QTreeWidget::customContextMenuRequested, [=](const QPoint &) 
	{
		if (ptreewidget->selectedItems().empty())
		{
			return;
		}

		QTreeWidgetItem *item = ptreewidget->selectedItems().at(0);
		std::unique_ptr<QMenu> menu = std::make_unique<QMenu>(this);
		QAction *check_group_action = nullptr, *uncheck_group_action = nullptr;

		//does not contain
		if (!this->check_treewidgetitem_handler.count(item))
		{
			check_group_action = menu->addAction("Check " + item->text(0));
			uncheck_group_action = menu->addAction("Uncheck " + item->text(0));

			menu->addSeparator();
		}

		QAction *check_all_action = menu->addAction("Check All");
		QAction *uncheck_all_action = menu->addAction("Uncheck All");

		menu->addSeparator();

		QAction *expand_all_action = menu->addAction("Expand All");
		QAction *collapse_all_action = menu->addAction("Collapse All");

		QAction *performed_action = menu->exec(QCursor::pos());
		if (performed_action == check_group_action || performed_action == uncheck_group_action)
		{
			Qt::CheckState state = performed_action == check_group_action ? Qt::Checked : Qt::Unchecked;
			for (int32_t i = 0; i < item->childCount(); ++i)
			{
				item->child(i)->setCheckState(0, state);
			}
		}
		else if (performed_action == check_all_action || performed_action == uncheck_all_action)
		{
			Qt::CheckState state = performed_action == check_all_action ? Qt::Checked : Qt::Unchecked;
			for (int32_t i = 0; i < ptreewidget->topLevelItemCount(); ++i)
			{
				for (int32_t j = 0; j < ptreewidget->topLevelItem(i)->childCount(); ++j)
				{
					ptreewidget->topLevelItem(i)->child(j)->setCheckState(0, state);
				}
			}
		}
		else if (performed_action == expand_all_action || performed_action == collapse_all_action)
		{
			for (int32_t i = 0; i < ptreewidget->topLevelItemCount(); ++i)
			{
				ptreewidget->topLevelItem(i)->setExpanded(expand_all_action == performed_action);
			}
		}
	});

	layout->addWidget(ptreewidget.get());

	tree_widgets.push_back(ptreewidget);

	return tree_widgets.size() - 1;
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
	QTreeWidgetItem *ptr = new QTreeWidgetItem({QString::fromStdString(n.c_str())});
	ptr->setCheckState(0, Qt::CheckState::Unchecked);
	check_treewidgetitem_handler[ptr] = handler;
	return ptr;
}

bool settingswindow::get_newsignature_randomstring()
{
	return newsignature_randomstring;
}

std::vector<std::pair<address_t, size_t>> settingswindow::get_selected_regions()
{
	std::vector<std::pair<address_t, size_t>> whitelisted_regions;

	for (const std::pair<std::string, bool> &p : region_selection)
	{
		if (p.second)
		{
			whitelisted_regions.push_back(regions[p.first].second);
		}
	}

	return whitelisted_regions;
}
