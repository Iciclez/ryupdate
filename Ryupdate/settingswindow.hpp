#pragma once

#include <QWidget>
#include <QDialog>
#include <QTreeWidget>
#include <QLayout>

#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

class settingswindow : public QDialog
{
	Q_OBJECT

public:
	typedef std::function<void(bool)> checkbox_function_t;
	typedef std::unordered_map<uint32_t, QWidget *> columnwidgets_t;
	typedef std::pair<checkbox_function_t, columnwidgets_t> itemdata_t;
	typedef std::unordered_map<std::string, itemdata_t> items_t;
	typedef std::pair<std::string, items_t> treeviewitem_t;

	explicit settingswindow(QWidget *parent = nullptr);
	~settingswindow();

	size_t insert(size_t treewidget, const treeviewitem_t &item);
	size_t insert(size_t treewidget, const std::vector<treeviewitem_t> &items);

	size_t insert(const std::vector<std::string> &n = std::vector<std::string>());
	size_t insert(const std::vector<treeviewitem_t> &items);
	size_t insert(const std::vector<std::string> &n, const std::vector<treeviewitem_t> &items);

private:
	QTreeWidgetItem *check_treewidgetitem(
		const std::string &n, const std::function<void(bool)> &handler = [](bool) {});

	std::unique_ptr<QHBoxLayout> layout;
	std::unordered_map<QTreeWidgetItem *, std::function<void(bool)>> check_treewidgetitem_handler;
	std::vector<std::shared_ptr<QTreeWidget>> treewidgets;

	std::unordered_map<std::string, std::pair<std::string, std::pair<unsigned long, size_t>>> regions;
	std::unordered_map<std::string, bool> regionselection;

private:
	bool newsignature_randomstring;

public:
	bool get_newsignature_randomstring();
	std::vector<std::pair<unsigned long, size_t>> get_selected_regions();
};
