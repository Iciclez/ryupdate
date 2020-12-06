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

#include "signatureitem.hpp"
#include "settingswindow.hpp"

class mainwindow : public QMainWindow
{
	Q_OBJECT

public:
	mainwindow();
	~mainwindow();

	bool insert_item();
	bool insert_item(const std::shared_ptr<signatureitem> &signature);
	void remove_selected_item();
	void clear();

	void update_data(const std::string &name);
	void update_all_address();

	void insert_json(const std::string &filepath);
	void export_json(const std::string &filepath);

	std::pair<unsigned long, size_t> region();

protected:
	void showEvent(QShowEvent* showevent);
	void closeEvent(QCloseEvent* closeevent);

private:
	void set_stylesheet(const std::tuple<byte, byte, byte> &color = std::make_tuple<byte, byte, byte>(0, 120, 210));
	void set_properties();
	void set_messagehandler();

	void set_menubar();
	void set_statusbar();

	std::string get_signature_data(int32_t row);

	std::unique_ptr<QTableWidget> tablewidget;
	std::unique_ptr<QLabel> statuslabel;
	std::unique_ptr<QProgressBar> progressbar;
	std::wstring ryupdate_path;

	std::unique_ptr<settingswindow> settings;

private:
	std::unordered_map<std::string, std::shared_ptr<signatureitem>> signatures;
};
