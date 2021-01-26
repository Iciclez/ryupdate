#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>

#include <tuple>
#include <memory>
#include <string>
#include <unordered_map>

#include <windows.h>

#include "signature_item.hpp"
#include "settingswindow.hpp"

#include "zephyrus.hpp"

class mainwindow : public QMainWindow
{
	Q_OBJECT

public:
	mainwindow();
	~mainwindow();

	bool insert_item();
	bool insert_item(const std::shared_ptr<signature_item> &signature);
	void remove_selected_item();
	void clear();

	void update_data(const std::string &name);
	void update_all_address();

	void insert_json(const std::string &file);
	void export_json(const std::string &file);

	std::pair<address_t, size_t> region();

protected:
	void showEvent(QShowEvent* qevent);
	void closeEvent(QCloseEvent*);

private:
	void set_style_sheet(const std::tuple<uint8_t, uint8_t, uint8_t> &color = std::make_tuple<uint8_t, uint8_t, uint8_t>(0, 120, 210));
	void set_properties();
	void set_message_handler();

	void set_menu_bar();
	void set_status_bar();

	std::string get_signature_data(size_t row);

	std::unique_ptr<QTableWidget> table_widget;
	std::unique_ptr<QLabel> status_label;
	std::unique_ptr<QProgressBar> progress_bar;
	std::string ryupdate_path;

	std::unique_ptr<settingswindow> settings;

private:
	std::unordered_map<std::string, std::shared_ptr<signature_item>> signatures;
};
