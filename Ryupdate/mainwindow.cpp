#include "mainwindow.hpp"
#include "aobscan.hpp"
#include "json.hpp"
#include "signature_export.hpp"
#include "ryupdate.hpp"

#include <functional>
#include <string>
#include <random>
#include <chrono>
#include <fstream>
#include <thread>

#include <shlwapi.h>
#include <Shlobj.h>

#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QStatusBar>
#include <QStandardPaths>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>

#include <QFileDialog>
#include <QColorDialog>

#include <QApplication>
#include <QClipboard>

#include "ryupdate.hpp"

using namespace nlohmann;

mainwindow::mainwindow()
	: tablewidget(std::make_unique<QTableWidget>(this)),
	  statuslabel(std::make_unique<QLabel>(this)),
	  progressbar(std::make_unique<QProgressBar>(this)),
	  settings(std::make_unique<settingswindow>(this))
{
	this->tablewidget->setAlternatingRowColors(true);
	this->tablewidget->setColumnCount(6);
	this->tablewidget->setHorizontalHeaderLabels(QStringList({"Name", "Type", "Signature", "Result", "Scanned Data", "Comments"}));
	this->tablewidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	this->tablewidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	this->tablewidget->horizontalHeader()->setHighlightSections(false);
	this->tablewidget->verticalHeader()->setHighlightSections(false);
	this->tablewidget->verticalHeader()->setDefaultSectionSize(18);
	this->tablewidget->setSortingEnabled(true);
	this->tablewidget->setContextMenuPolicy(Qt::CustomContextMenu);
	this->tablewidget->setFont(QFont("Segoe UI", 8));

	this->progressbar->setMaximumHeight(20);
	this->progressbar->setMaximumWidth(200);
	this->progressbar->setValue(0);
	this->progressbar->setMaximum(1);
	this->progressbar->setTextVisible(true);

	this->set_menubar();
	this->set_statusbar();

	this->set_properties();
	this->set_messagehandler();

	this->ryupdate_path = []() -> std::wstring {
		wchar_t **app_data = reinterpret_cast<wchar_t **>(CoTaskMemAlloc(MAX_PATH));
		if (app_data == 0)
		{
			return std::wstring();
		}

		if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, app_data) != S_OK)
		{
			CoTaskMemFree(app_data);
			return std::wstring();
		}

		PathCombine(*app_data, *app_data, L"Ryupdate");

		std::wstring ryupdate_path(*app_data);

		CoTaskMemFree(app_data);

		return ryupdate_path;
	}();

	if (!PathFileExists(this->ryupdate_path.c_str()))
	{
		CreateDirectory(this->ryupdate_path.c_str(), 0);
	}
}

mainwindow::~mainwindow()
{
}

bool mainwindow::insert_item()
{
	std::function<std::string(size_t)> rand_string = [](size_t length) {
		static std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

		std::mt19937 mt{std::random_device{}()};
		std::uniform_int_distribution<std::string::size_type> dist(0, characters.length() - 1);

		std::string s;

		s.reserve(length);

		while (length--)
		{
			s += characters[dist(mt)];
		}

		while (isdigit(s.at(0)))
		{
			s.at(0) = characters[dist(mt)];
		}

		return s;
	};

	std::string default_string = settings->get_newsignature_randomstring() ? rand_string(10) : "";

	bool ok = false;
	QString text = QInputDialog::getText(this, "Ryupdate: Insert Signature Item", "Signature Name: ", QLineEdit::Normal, QString::fromStdString(default_string), &ok);

	if (ok && !text.isEmpty())
	{
		if (isdigit(text.at(0).toLatin1()))
		{
			text = "n" + text;
		}

		return this->insert_item(std::make_shared<signatureitem>(this, text.toStdString()));
	}

	return false;
}

bool mainwindow::insert_item(const std::shared_ptr<signatureitem> &signature)
{
	if (isdigit(signature->name.at(0)))
	{
		signature->name = "n" + signature->name;
	}

	//1 if exists, 0 if does not exist
	if (signatures.count(signature->name) > 0)
	{
		return false;
	}

	signatures[signature->name] = signature;

	int32_t r = this->tablewidget->rowCount();
	this->tablewidget->insertRow(r);

	this->tablewidget->setItem(r, 0, signature->namewidget);
	this->tablewidget->setCellWidget(r, 1, signature->typewidget.get());
	this->tablewidget->setItem(r, 2, signature->signaturewidget);
	this->tablewidget->setCellWidget(r, 3, signature->resultwidget.get());
	this->tablewidget->setItem(r, 4, signature->datawidget);
	this->tablewidget->setItem(r, 5, signature->commentswidget);

	this->progressbar->setValue(0);
	this->progressbar->setMaximum(this->tablewidget->rowCount());

	return true;
}

void mainwindow::remove_selected_item()
{
	QList<QTableWidgetItem *> items = tablewidget->selectedItems();
	if (items.size() / tablewidget->columnCount() == tablewidget->rowCount())
	{
		this->clear();
		return;
	}

	for (const QTableWidgetItem *p : items)
	{
		if (p->column() == tablewidget->columnCount() - 1)
		{
			int32_t row = p->row();
			delete p;
			tablewidget->removeRow(row);
		}
		else if (p->column() == 0)
		{
			signatures.erase(tablewidget->item(p->row(), 0)->text().toStdString());
		}
		else
		{
			delete p;
		}
	}

	this->progressbar->setValue(0);
}

void mainwindow::clear()
{
	this->tablewidget->clearSelection();

	for (int32_t i = 0; i < tablewidget->rowCount(); ++i)
	{
		for (int32_t j = 0; j < tablewidget->columnCount(); ++j)
		{
			delete tablewidget->item(i, j);
		}
	}

	this->progressbar->setValue(0);
	this->tablewidget->setRowCount(0);

	this->signatures.clear();
}

void mainwindow::update_data(const std::string &name)
{
	signatures[name]->update_data(this->region());
}

void mainwindow::update_all_address()
{
	this->progressbar->setValue(0);
	for (const std::pair<std::string, std::shared_ptr<signatureitem>> &p : signatures)
	{
		p.second->update_data(this->region());
		this->progressbar->setValue(this->progressbar->value() + 1);
	}
}

void mainwindow::insert_json(const std::string &filepath)
{
	std::ifstream fs(filepath);
	std::stringstream ss;
	ss << fs.rdbuf();

	if (ss.str().empty())
	{
		return;
	}

	json j = json::parse(ss);

	for (const basic_json<> &n : j)
	{
		this->insert_item(std::make_shared<signatureitem>(this,
														  n["name"],
														  n["type"],
														  n["signature"],
														  n["result"],
														  n["data"],
														  n["comments"]));
	}
}

void mainwindow::export_json(const std::string &filepath)
{
	json j;
	for (const std::pair<std::string, std::shared_ptr<signatureitem>> &p : signatures)
	{
		j[p.first]["name"] = p.second->name;
		j[p.first]["type"] = p.second->type;
		j[p.first]["signature"] = p.second->signature;
		j[p.first]["result"] = p.second->result;
		j[p.first]["data"] = p.second->data;
		j[p.first]["comments"] = p.second->comments;
	}

	std::ofstream fs(filepath);
	if (fs.is_open())
	{
		fs << j.dump();
		fs.close();
	}
}

void mainwindow::set_stylesheet(const std::tuple<byte, byte, byte> &color)
{
	//a blue color: <0, 120, 210>;
	//a red color: <230, 80, 120>;
	//a purple color: <155, 50, 205>;
	QString main_background_color = "rgb(28, 30, 38)";
	QString secondary_background_color = "rgb(18, 20, 28)";
	QString theme_primary_color = QString("rgb(%1, %2, %3)").arg(QString::number(std::get<0>(color)), QString::number(std::get<1>(color)), QString::number(std::get<2>(color)));
	QString theme_secondary_color = QString("rgb(%1, %2, %3)").arg(QString::number(std::get<0>(color)), QString::number(std::get<1>(color) + 10 > 0xff ? 0xff : std::get<1>(color) + 10), QString::number(std::get<2>(color) + 30 > 0xff ? 0xff : std::get<2>(color) + 30));
	QString theme_tertiary_color = QString("rgb(%1, %2, %3)").arg(QString::number(std::get<0>(color)), QString::number(std::get<1>(color) + 30 > 0xff ? 0xff : std::get<1>(color) + 30), QString::number(std::get<2>(color) + 30 > 0xff ? 0xff : std::get<2>(color) + 30));

	QString default_gray_color = "rgb(165, 165, 165)";

	QString stylesheet = QString(
							 "QMainWindow { background: %1; }"
							 "QDialog { background: %1; }"

							 "QDockWidget { color: white; background-color: %2; }"

							 "QTabWidget::pane { border: 2px solid %3; }"
							 "QTabBar::tab { color: white; background: %1; }"
							 "QTabBar::tab:selected { color: black; background: %3; }"
							 "QTabBar::tab:hover { background: %4; border: 2px solid %4; }"

							 //"QTabBar::close-button { image: url(:/resources/close_tab_white.png); }"
							 //"QTabBar::close-button:selected { image: url(:/resources/close_tab.png); }"
							 //"QTabBar::close-button:hover { image: url(:/resources/close_tab_whitegray.png); }"

							 "QTableView { border: 2px solid white; color: white; background-color: %1; alternate-background-color: %2; selection-color: white; }"
							 "QTableWidget { border: 2px solid white; color: white; background-color: %1; alternate-background-color: %2; selection-color: white; }"
							 "QTableView::item { selection-background-color: %3; }"
							 "QTableWidget::item { selection-background-color: %3; }"

							 "QGroupBox { border: 2px solid %3; color: white; }"
							 "QGroupBox::title { color: white; background-color: %3; padding-left: 7px; padding-right: 16777215px; }"

							 "QTreeView { color: white; background-color: %2; }"
							 "QTreeWidget { color: white; background-color: %2; }"
							 //"QHeaderView::section { background-color: transparent; }"
							 //"QTreeView::branch { background-color: white; }"
							 //"QTreeView::branch:open { image: url(://resources/sort_down-48.png); }"
							 //"QTreeView::branch:closed:has-children { image: url(://resources/sort_right-48.png); }"

							 //"QTreeView::branch:open { image: url(://resources/expand2-48.png); border: 3px solid transparent }"
							 //"QTreeView::branch:closed:has-children { image: url(://resources/right_4-48.png); border: 3px solid transparent }"

							 "QLineEdit { color: white; background-color: %1; border: 1px solid %3; }"

							 "QTextEdit { color: %6; background-color: %1; border: 1px solid %3; }"

							 "QSpinBox{ background: %1; color: %6; }"
							 //"QSpinBox::up-arrow { image: url(:/resources/sort_up-48.png); }"
							 //"QSpinBox::down-arrow { image: url(:/resources/sort_down-48.png); }"

							 "QSplitter::handle { background: %1; }"

							 "QComboBox { color: white; background-color: black; }"
							 "QComboBox QAbstractItemView { border: 1px solid %3; background-color: %1; color: white; selection-background-color: %3; selection-color: black; }"
							 "QComboBox::down-arrow { image: url(:/resources/sort_down-48.png); }"

							 "QMenu { color: white; background: %1; }"
							 "QMenu::item::selected { color: black; background: %4; }"

							 "QMenuBar { color: white; background: %1; }"
							 "QMenuBar::item:selected { color: black; background: %4; }"

							 "QPushButton { color: white; border: 2px solid %3; background-color: %3; }"
							 "QPushButton::focus::pressed { color: black; background-color: %5; }"
							 "QPushButton::hover { border: 2px solid %5; background-color: %3; }"

							 "QCheckBox  { color: %6; }"
							 //"QCheckBox::indicator:unchecked { image: url(://Resources/checkbox_unchecked.png); width: 20px; height: 20px; }"
							 //QCheckBox::indicator:unchecked:hover { image: url(:/images/checkbox_unchecked_hover.png); }
							 //QCheckBox::indicator:unchecked:pressed { image: url(:/images/checkbox_unchecked_pressed.png); }
							 //"QCheckBox::indicator:checked { image: url(://Resources/checkbox_checked.png); width: 20px; height: 20px; }"
							 //QCheckBox::indicator:checked:hover { image: url(:/images/checkbox_checked_hover.png); }
							 //QCheckBox::indicator:checked:pressed { image: url(:/images/checkbox_checked_pressed.png); }
							 //QCheckBox::indicator:indeterminate:hover { image: url(:/images/checkbox_indeterminate_hover.png); }
							 //QCheckBox::indicator:indeterminate:pressed { image: url(:/images/checkbox_indeterminate_pressed.png); }

							 "QLabel  { color: white; }"

							 "QRadioButton  { color: %6; }"

							 "QProgressBar { color: white; }"

							 "QStatusBar::item{ border: none; }")
							 .arg(
								 main_background_color,
								 secondary_background_color,
								 theme_primary_color,
								 theme_secondary_color,
								 theme_tertiary_color,
								 default_gray_color);

	this->setStyleSheet(stylesheet);
}

void mainwindow::set_properties()
{
	this->setWindowTitle("Ryupdate");
	this->resize(1300, 700);

	this->setCentralWidget(this->tablewidget.get());

	this->setAcceptDrops(true);
}

void mainwindow::set_messagehandler()
{
	connect(this->tablewidget.get(), &QTableWidget::customContextMenuRequested, [this](const QPoint &) {
		std::unique_ptr<QMenu> menu = std::make_unique<QMenu>(this);

		QAction *pinsert = menu->addAction("Insert Signature Item");
		QAction *premove = menu->addAction("Remove Signature Item");
		QAction *pduplicate = menu->addAction("Duplicate Signature Item");
		menu->addSeparator();
		QAction *pgeneratesignature = menu->addAction("Generate Signature");
		menu->addSeparator();
		QAction *pupdate = menu->addAction("Update Signature");
		QAction *pupdateall = menu->addAction("Update All Signatures");
		menu->addSeparator();
		QAction *pcopysignaturedata = menu->addAction("Copy Signature Data");
		menu->addSeparator();
		QAction *pclear = menu->addAction("Clear");

		QAction *p = menu->exec(QCursor::pos());
		if (p == pinsert)
		{
			this->insert_item();
		}
		else if (p == premove)
		{
			this->remove_selected_item();
		}
		else if (p == pclear)
		{
			this->clear();
		}
		else if (p == pduplicate)
		{
			if (this->tablewidget->currentRow() != -1)
			{
				std::shared_ptr<signatureitem> item = signatures.at(this->tablewidget->item(this->tablewidget->currentRow(), 0)->text().toStdString());

				bool ok = false;
				QString text = QInputDialog::getText(this, "Ryupdate: Insert Signature Item", "Signature Name: ", QLineEdit::Normal, QString::fromStdString(item->name + "_"), &ok);

				if (ok && !text.isEmpty())
				{
					this->insert_item(std::make_shared<signatureitem>(this, text.toStdString(), item->type, item->signature, item->result, item->data, item->comments));
				}
			}
		}
		else if (p == pgeneratesignature)
		{
			//we need address and size

			QString qtext = "BAADF00D";

			//ensure hex;

			if (QApplication::clipboard()->text().size() == 8)
			{
				int32_t n = 0;
				for (n = 0; n < qtext.size() && isxdigit(qtext.at(n).toLatin1()); ++n)
					;
				if (n == qtext.size())
				{
					qtext = QApplication::clipboard()->text();
				}
			}

			bool ok = false;
			std::string text = QInputDialog::getText(this, "Ryupdate: Generate Signature", "Signature Data: (address:size)", QLineEdit::Normal, qtext + ":12", &ok).toStdString();

			if (ok && !text.empty())
			{
				size_t x = text.find_first_of(':');

				uint32_t address = std::stoi(text.substr(0, x), nullptr, 16);
				size_t size = std::stoi(text.substr(x + 1));

				this->tablewidget->item(this->tablewidget->currentRow(), 2)->setText(QString::fromStdString(z.byte_to_string(z.readmemory(address, size))));
			}
		}
		else if (p == pupdate)
		{
			QList<QTableWidgetItem *> items = this->tablewidget->selectedItems();

			for (const QTableWidgetItem *p : items)
			{
				if (p->column() == 0)
				{
					this->update_data(p->text().toStdString());
				}
			}
		}
		else if (p == pupdateall)
		{
			this->update_all_address();
		}
		else if (p == pcopysignaturedata)
		{
			std::string text = "";

			QList<QTableWidgetItem *> items = this->tablewidget->selectedItems();

			for (const QTableWidgetItem *p : items)
			{
				if (p->column() == 0)
				{
					text += this->get_signature_data(p->row()) + "\n";
				}
			}

			QApplication::clipboard()->setText(QString::fromStdString(text));
		}
	});

	connect(this->tablewidget.get(), &QTableWidget::itemChanged, [this](QTableWidgetItem *item) {
		for (auto it = signatures.begin(); it != signatures.end(); ++it)
		{
			if (item == it->second->namewidget)
			{
				std::string newname = item->text().toStdString();
				if (newname.empty() || isdigit(newname.at(0)))
				{
					item->setText(QString::fromStdString(it->second->name));
					break;
				}

				//does not exist
				if (signatures.count(newname) == 0)
				{
					std::shared_ptr<signatureitem> item = it->second;
					item->name = newname;

					signatures.erase(it);
					signatures[newname] = item;
				}
				else
				{
					//revert back to old name
					item->setText(QString::fromStdString(it->second->name));
				}

				break;
			}
			if (item == it->second->signaturewidget)
			{
				QString newsignature = item->text();
				for (int32_t n = 0; n < newsignature.size(); ++n)
				{
					if (newsignature.at(n).toLatin1() != ' ' &&
						newsignature.at(n).toLatin1() != '?' &&
						!isxdigit(newsignature.at(n).toLatin1()))
					{
						item->setText(QString::fromStdString(it->second->signature));
						break;
					}
				}

				try
				{
					std::string pattern = aobscan(item->text().toStdString()).get_pattern();
					std::string signature = "";
					for (size_t n = 0; n < pattern.size(); ++n)
					{
						signature += pattern.at(n);

						if (n % 2 == 1 && n + 1 != pattern.size())
						{
							signature += ' ';
						}
					}

					it->second->signature = signature;
				}
				catch (std::exception &e)
				{
					it->second->signature = "";
				}

				item->setText(QString::fromStdString(it->second->signature));
				break;
			}
			if (item == it->second->commentswidget)
			{
				it->second->comments = item->text().toStdString();
				break;
			}
			if (item == it->second->datawidget)
			{
				it->second->data = item->text().toStdString();
				break;
			}
		}
	});

	connect(this->tablewidget.get(), &QTableWidget::itemSelectionChanged, [this]() {
		this->statuslabel->setText(QString::fromStdString(this->get_signature_data(this->tablewidget->currentRow())));
	});
}

void mainwindow::set_menubar()
{
	QMenu *pfilemenu = this->menuBar()->addMenu("File");
	QMenu *peditmenu = this->menuBar()->addMenu("Edit");
	QMenu *pviewmenu = this->menuBar()->addMenu("View");
	QMenu *ptoolsmenu = this->menuBar()->addMenu("Tools");
	QMenu *phelpmenu = this->menuBar()->addMenu("Help");

	//filemenu
	pfilemenu->addAction("New", this, [this]() {
		this->clear();
	});
	pfilemenu->addSeparator();
	pfilemenu->addAction("Open", this, [this]() {
		QStringList list = QFileDialog::getOpenFileNames(this, QString(), QString::fromStdWString(this->ryupdate_path), "json file (*.json)");
		if (!list.empty())
		{
			this->clear();
		}
		for (int32_t n = 0; n < list.size(); ++n)
		{
			this->insert_json(list.at(n).toStdString());
		}
	});
	pfilemenu->addAction("Open (Append)", this, [this]() {
				 QStringList list = QFileDialog::getOpenFileNames(this, QString(), QString::fromStdWString(this->ryupdate_path), "json file (*.json)");
				 for (int32_t n = 0; n < list.size(); ++n)
				 {
					 this->insert_json(list.at(n).toStdString());
				 }
			 })
		->setShortcut(Qt::CTRL + Qt::Key_O);
	pfilemenu->addSeparator();
	pfilemenu->addAction("Save As...", this, [this]() {
				 std::string filedialogpath = QFileDialog::getSaveFileName(this, QString(), QString::fromStdWString(this->ryupdate_path), "json file (*.json)").toStdString();
				 if (!filedialogpath.empty())
				 {
					 this->export_json(filedialogpath);
				 }
			 })
		->setShortcut(Qt::CTRL + Qt::Key_S);
	pfilemenu->addSeparator();
	pfilemenu->addAction("Export to C", this, [this]() {
		bool ok = false;
		std::string text = QInputDialog::getText(this, "Ryupdate: Export to C", "Prefix (optional): ", QLineEdit::Normal, "", &ok).toStdString();

		if (ok)
		{
			std::string filedialogpath = QFileDialog::getSaveFileName(this, QString(), QString::fromStdWString(this->ryupdate_path), "c/c++ header file (*.h)").toStdString();
			if (!filedialogpath.empty())
			{
				signature_export::save(filedialogpath, signature_export::make_header(this->signatures, text));

				std::unique_ptr<QMessageBox> messagebox = std::make_unique<QMessageBox>(this);
				messagebox->setText("Exported. \n\nHeader: " + QString::fromStdString(filedialogpath));
				messagebox->setWindowTitle("Ryupdate: Export to C");
				messagebox->exec();
			}
		}
	});

	pfilemenu->addAction("Export to C++", this, [this]() {
		bool ok = false;
		std::string text = QInputDialog::getText(this, "Ryupdate: Export to C++", "Class Name: ", QLineEdit::Normal, QString::fromStdString("addresses"), &ok).toStdString();

		if (ok)
		{
			std::function<bool(const std::string &)> is_directory = [](const std::string &directory) -> bool {
				if (directory.empty())
				{
					return false;
				}

				DWORD dw = GetFileAttributesA(directory.c_str());
				if (dw == INVALID_FILE_ATTRIBUTES)
				{
					return false;
				}

				return dw & FILE_ATTRIBUTE_DIRECTORY;
			};

			std::string directory = QFileDialog::getExistingDirectory(this, "Ryupdate: Export to C++", QString::fromStdWString(this->ryupdate_path), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdString();

			if (is_directory(directory))
			{
				signature_export code(this->signatures, text);

				std::string header_file = text + ".hpp";
				std::string source_file = text + ".cpp";

				std::unique_ptr<char[]> header_path = std::make_unique<char[]>(MAX_PATH);
				std::unique_ptr<char[]> source_path = std::make_unique<char[]>(MAX_PATH);

				PathCombineA(header_path.get(), directory.c_str(), header_file.c_str());
				PathCombineA(source_path.get(), directory.c_str(), source_file.c_str());

				code.save_header(header_path.get());
				code.save_source(source_path.get());

				std::unique_ptr<QMessageBox> messagebox = std::make_unique<QMessageBox>(this);
				messagebox->setText("Exported. \n\nHeader: " + QString::fromStdString(header_path.get()) + "\nSource: " + QString::fromStdString(source_path.get()));
				messagebox->setWindowTitle("Ryupdate: Export to C++");
				messagebox->exec();
			}
		}
	});
	pfilemenu->addSeparator();
	pfilemenu->addAction("Exit", this, [this]() {
				 this->close();
			 })
		->setShortcut(Qt::ALT + Qt::Key_F4);

	//peditmenu
	peditmenu->addAction("Insert Signature Item...", this, [this]() {
				 this->insert_item();
			 })
		->setShortcut(Qt::CTRL + Qt::Key_N);

	//
	pviewmenu->addAction("Change Theme...", this, [this]() {
		QColor color = QColorDialog::getColor(QColor(0, 120, 210), this);
		if (!(color.red() == 0 && color.green() == 0 && color.blue() == 0))
		{
			this->set_stylesheet(std::make_tuple<byte, byte, byte>(color.red(), color.green(), color.blue()));
			this->repaint();
		}
	});

	//
	ptoolsmenu->addAction("Update Signature Item", this, [this]() {
		QList<QTableWidgetItem *> items = this->tablewidget->selectedItems();

		for (const QTableWidgetItem *p : items)
		{
			if (p->column() == 0)
			{
				this->update_data(p->text().toStdString());
			}
		}
	});
	ptoolsmenu->addAction("Update All Signature Item", this, [this]() {
		this->update_all_address();
	});

	ptoolsmenu->addSeparator();

	ptoolsmenu->addAction("Options...", this, [this]() {
		this->settings->exec();
	});

	phelpmenu->addAction("About Qt", this, [this]() {
		QMessageBox::aboutQt(this);
	});

	phelpmenu->addAction("About Ryupdate", this, [this]() {
		std::unique_ptr<QMessageBox> messagebox = std::make_unique<QMessageBox>(this);
		messagebox->setWindowIcon(QIcon(":/resources/app.png"));
		messagebox->setIconPixmap(QPixmap(":/resources/app.png"));
		messagebox->setText("Ryupdate\t\t2.0.0.0\n\nCreated by Iciclez");
		messagebox->setWindowTitle("About Ryupdate");
		messagebox->exec();
	});

	//DEBUG MENU
	/*QMenu *pdebugmenu = this->menuBar()->addMenu("Debug");
	pdebugmenu->addAction("Enumerate Resources", this, [this]()
		{
			QDirIterator it(":", QDirIterator::Subdirectories);
			while (it.hasNext()) {
				printf("%s\n", it.next().toStdString().c_str());
			}
		});*/
	/*pdebugmenu->addAction("Print Set<SignatureName>", this, [this]()
	{
		printf("\n");
		for (const std::string &n : signaturenames)
		{
			printf("%s\n", n.c_str());
		}
	});

	pdebugmenu->addAction("Print Map<>", this, [this]()
	{
		printf("\n");
		for (auto &n : signatures)
		{
			printf("%s\t%d\t%s\t%d\t%s\t%s\n", n.first.c_str(), n.second->type, n.second->signature.c_str(), n.second->result, n.second->data.c_str(), n.second->comments.c_str());
		}
	});

	pdebugmenu->addAction("EnumModules", this, [this]()
	{
		printf("Random String: %d\n", settings->get_newsignature_randomstring());

		std::vector<std::pair<unsigned long, size_t>> regions = settings->get_selected_regions();
		for (const std::pair<unsigned long, size_t> n : regions)
		{
			printf("%.08X->%.08X\n", n.first, n.first + n.second);
		}
	});*/
}

void mainwindow::set_statusbar()
{
	this->statusBar()->insertWidget(0, statuslabel.get());
	this->statusBar()->addPermanentWidget(progressbar.get());
}

std::string mainwindow::get_signature_data(int32_t row)
{
	std::shared_ptr<signatureitem> x = signatures[this->tablewidget->item(row, 0)->text().toStdString()];

	std::string text = x->name;

	if (!x->data.empty())
	{
		text += ": " + x->data;
	}

	text += " //" + x->signature;

	if (!x->comments.empty())
	{
		text += " {" + x->comments + "}";
	}

	text += " [Result: " + std::to_string(x->result) + "]";

	return text;
}

std::pair<unsigned long, size_t> mainwindow::region()
{
	std::vector<std::pair<unsigned long, size_t>> regions = settings->get_selected_regions();
	return regions.empty() ? std::make_pair(0, 0) : regions.at(0);
}

void mainwindow::showEvent(QShowEvent *showevent)
{
	QMainWindow::showEvent(showevent);

	//resources are fucking case-sensitive
	this->set_stylesheet(std::make_tuple<byte, byte, byte>(230, 80, 120));
	this->setWindowIcon(QIcon(":/resources/app.png"));
}

void mainwindow::closeEvent(QCloseEvent *closeevent)
{
	ryupdate::destroy();
}
