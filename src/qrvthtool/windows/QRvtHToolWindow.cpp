/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.cpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ***************************************************************************/

#include "QRvtHToolWindow.hpp"

#include "librvth/rvth.hpp"
#include "RvtHModel.hpp"
#include "RvtHSortFilterProxyModel.hpp"

// Qt includes.
#include <QtWidgets/QFileDialog>

/** QRvtHToolWindowPrivate **/

#include "ui_QRvtHToolWindow.h"
class QRvtHToolWindowPrivate
{
	public:
		explicit QRvtHToolWindowPrivate(QRvtHToolWindow *q);
		~QRvtHToolWindowPrivate();

	protected:
		QRvtHToolWindow *const q_ptr;
		Q_DECLARE_PUBLIC(QRvtHToolWindow)
	private:
		Q_DISABLE_COPY(QRvtHToolWindowPrivate)

	public:
		Ui::QRvtHToolWindow ui;

		// RVT-H Reader disk image
		RvtH *rvth;
		RvtHModel *model;
		RvtHSortFilterProxyModel *proxyModel;

		// Filename.
		QString filename;
		QString displayFilename;	// filename without subdirectories

		// TODO: Config class like mcrecover?
		QString lastPath;

		// Initialized columns?
		bool cols_init;

		// Last icon ID.
		RvtHModel::IconID lastIconID;

		/**
		 * Update the RVT-H Reader disk image's QTreeView.
		 */
		void updateLstBankList(void);

		/**
		 * Update the window title.
		 */
		void updateWindowTitle(void);
};

QRvtHToolWindowPrivate::QRvtHToolWindowPrivate(QRvtHToolWindow *q)
	: q_ptr(q)
	, rvth(nullptr)
	, model(new RvtHModel(q))
	, proxyModel(new RvtHSortFilterProxyModel(q))
	, cols_init(false)
	, lastIconID(RvtHModel::ICON_MAX)
{
	// Connect the RvtHModel slots.
	QObject::connect(model, &RvtHModel::layoutChanged,
			 q, &QRvtHToolWindow::rvthModel_layoutChanged);
	QObject::connect(model, &RvtHModel::rowsInserted,
			 q, &QRvtHToolWindow::rvthModel_rowsInserted);
}

QRvtHToolWindowPrivate::~QRvtHToolWindowPrivate()
{
	// NOTE: Delete the MemCardModel first to prevent issues later.
	delete model;
	if (rvth) {
		delete rvth;
	}
}

/**
 * Update the RVT-H Reader disk image's QTreeView.
 */
void QRvtHToolWindowPrivate::updateLstBankList(void)
{
	if (!rvth) {
		// Set the group box's title.
		ui.grpBankList->setTitle(QRvtHToolWindow::tr("No RVT-H Reader disk image loaded."));
	} else {
		// Show the filename.
		ui.grpBankList->setTitle(displayFilename);
	}

	// Show the QTreeView headers if an RVT-H Reader disk image is loaded.
	ui.lstBankList->setHeaderHidden(!rvth);

	// Resize the columns to fit the contents.
	int num_sections = model->columnCount();
	for (int i = 0; i < num_sections; i++)
		ui.lstBankList->resizeColumnToContents(i);
	ui.lstBankList->resizeColumnToContents(num_sections);
}

/**
 * Update the window title.
 */
void QRvtHToolWindowPrivate::updateWindowTitle(void)
{
	QString windowTitle;
	RvtHModel::IconID iconID;
	if (rvth) {
		windowTitle += displayFilename;
		windowTitle += QLatin1String(" - ");
		// If it's an RVT-H HDD image, use the RVT-H icon.
		// Otherwise, get the icon for the first bank.
		if (rvth->isHDD()) {
			iconID = RvtHModel::ICON_RVTH;
		} else {
			// Get the icon for the first bank.
			iconID = model->iconIDForBank1();
			if (iconID < RvtHModel::ICON_GCN || iconID >= RvtHModel::ICON_MAX) {
				// Invalid icon ID. Default to RVT-H.
				iconID = RvtHModel::ICON_RVTH;
			}
		}
	} else {
		// Use the RVT-H icon as the default.
		iconID = RvtHModel::ICON_RVTH;
	}
	windowTitle += QApplication::applicationName();

	Q_Q(QRvtHToolWindow);
	q->setWindowTitle(windowTitle);

#ifdef Q_OS_MAC
	// If there's no image loaded, remove the window icon.
	// This is a "proxy icon" on Mac OS X.
	// TODO: Associate with the image file if the file is loaded?
	if (!rvth) {
		q->setWindowIcon(QIcon());
		return;
	} else if (q->windowIcon().isNull()) {
		// Force an icon update.
		lastIconID = RvtHModel::ICON_MAX;
	}
#endif /* Q_OS_MAC */

	if (iconID != lastIconID) {
		q->setWindowIcon(model->getIcon(iconID));
		lastIconID = iconID;
	}
}

/** QRvtHToolWindow **/

QRvtHToolWindow::QRvtHToolWindow(QWidget *parent)
	: super(parent)
	, d_ptr(new QRvtHToolWindowPrivate(this))
{
	Q_D(QRvtHToolWindow);
	d->ui.setupUi(this);

	// Make sure the window is deleted on close.
	this->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_OS_MAC
	// Remove the window icon. (Mac "proxy icon")
	// TODO: Use the memory card file?
	this->setWindowIcon(QIcon());
#endif /* Q_OS_MAC */

#ifdef Q_OS_WIN
	// Hide the QMenuBar border on Win32.
	// FIXME: This causes the menu bar to be "truncated" when using
	// the Aero theme on Windows Vista and 7.
#if 0
	this->Ui_QRvtHToolWindow::menuBar->setStyleSheet(
		QLatin1String("QMenuBar { border: none }"));
#endif
#endif

	// Initialize the Language Menu.
	// TODO: Load/save the language setting somewhere?
	d->ui.menuLanguage->setLanguage(QString());

	// Set up the main splitter sizes.
	// We want the card info panel to be 160px wide at startup.
	// TODO: Save positioning settings somewhere?
	static const int BankInfoPanelWidth = 256;
	QList<int> sizes;
	sizes.append(this->width() - BankInfoPanelWidth);
	sizes.append(BankInfoPanelWidth);
	d->ui.splitterMain->setSizes(sizes);

	// Set the main splitter stretch factors.
	// We want the QTreeView to stretch, but not the card info panel.
	d->ui.splitterMain->setStretchFactor(0, 1);
	d->ui.splitterMain->setStretchFactor(1, 0);

	// Set the models.
	d->proxyModel->setSourceModel(d->model);
	d->ui.lstBankList->setModel(d->proxyModel);

	// Sort by COL_BANKNUM by default.
	// TODO: Disable sorting on specific columns.
	//d->proxyModel->setDynamicSortFilter(true);
	//d->ui.lstBankList->sortByColumn(RvtHModel::COL_BANKNUM, Qt::AscendingOrder);

	// Connect the lstBankList slots.
	connect(d->ui.lstBankList->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &QRvtHToolWindow::lstBankList_selectionModel_selectionChanged);

	// Initialize the UI.
	d->updateLstBankList();
	d->updateWindowTitle();
}

QRvtHToolWindow::~QRvtHToolWindow()
{
	delete d_ptr;
}

/**
 * Open an RVT-H Reader disk image.
 * @param filename Filename.
 */
void QRvtHToolWindow::openRvtH(const QString &filename)
{
	Q_D(QRvtHToolWindow);

	if (d->rvth) {
		d->model->setRvtH(nullptr);
		delete d->rvth;
	}

	// Open the specified RVT-H Reader disk image.
#ifdef _WIN32
	d->rvth = new RvtH(reinterpret_cast<const wchar_t*>(filename.utf16()), nullptr);
#else /* !_WIN32 */
	d->rvth = new RvtH(filename.toUtf8().constData(), nullptr);
#endif
	if (!d->rvth) {
		// FIXME: Show an error message?
		return;
	}

	d->filename = filename;
	d->model->setRvtH(d->rvth);

	// Extract the filename from the path.
	// TODO: Use QFileInfo or similar?
	d->displayFilename = filename;
	bool removeDir = true;
#ifdef _WIN32
	// Does the path start with "\\\\.\\PhysicalDriveN"?
	// FIXME: How does Qt's native slashes interact with this?
	if (d->displayFilename.startsWith(QLatin1String("\\\\.\\PhysicalDrive")) ||
	    d->displayFilename.startsWith(QLatin1String("//./PhysicalDrive")))
	{
		// Physical drive.
		// TODO: Make sure it's all backslashes.
		removeDir = false;
	}
#else /* !_WIN32 */
	// Does the path start with "/dev/"?
	if (d->displayFilename.startsWith(QLatin1String("/dev/"))) {
		// Physical drive.
		removeDir = false;
	}
#endif

	if (removeDir) {
		int lastSlash = d->displayFilename.lastIndexOf(QChar(L'/'));
		if (lastSlash >= 0) {
			d->displayFilename.remove(0, lastSlash + 1);
		}
	}

	// Update the UI.
	d->updateLstBankList();
	d->updateWindowTitle();

	// FIXME: If a file is opened from the command line,
	// QTreeView sort-of selects the first file.
	// (Signal is emitted, but nothing is highlighted.)
}

/**
 * Close the currently-opened RVT-H Reader disk image.
 */
void QRvtHToolWindow::closeRvtH(void)
{
	Q_D(QRvtHToolWindow);
	if (!d->rvth) {
		// Not open...
		return;
	}

	d->model->setRvtH(nullptr);
	delete d->rvth;
	d->rvth = nullptr;

	// Clear the filenames.
	d->filename.clear();
	d->displayFilename.clear();

	// Update the UI.
	d->updateLstBankList();
	d->updateWindowTitle();
}

/**
 * Widget state has changed.
 * @param event State change event.
 */
void QRvtHToolWindow::changeEvent(QEvent *event)
{
	Q_D(QRvtHToolWindow);

	switch (event->type()) {
		case QEvent::LanguageChange:
			// Retranslate the UI.
			d->ui.retranslateUi(this);
			d->updateLstBankList();
			d->updateWindowTitle();
			break;

		default:
			break;
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}

/**
 * Window show event.
 * @param event Window show event.
 */
void QRvtHToolWindow::showEvent(QShowEvent *event)
{
	Q_UNUSED(event);
	Q_D(QRvtHToolWindow);

	// Show all columns except signature status by default.
	// TODO: Allow the user to customize the columns, and save the
	// customized columns somewhere.
	if (!d->cols_init) {
		d->cols_init = true;
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_BANKNUM, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_TYPE, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_TITLE, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_DISCNUM, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_REVISION, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_REGION, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_IOS_VERSION, false);
		static_assert(RvtHModel::COL_IOS_VERSION + 1 == RvtHModel::COL_MAX,
			"Default column visibility status needs to be updated!");
	}
}

/** UI widget slots. **/

/**
 * Open a memory card image.
 */
void QRvtHToolWindow::on_actionOpen_triggered(void)
{
	Q_D(QRvtHToolWindow);

	// TODO: Remove the space before the "*.raw"?
	// On Linux, Qt shows an extra space after the filter name, since
	// it doesn't show the extension. Not sure about Windows...
	const QString allSupportedFilter = tr("All Supported Files") +
		QLatin1String(" (*.img *.bin *.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString hddFilter = tr("RVT-H Reader Disk Image Files") +
		QLatin1String(" (*.img *.bin)");
	const QString gcmFilter = tr("GameCube/Wii Disc Image Files") +
		QLatin1String(" (*.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString allFilter = tr("All Files") + QLatin1String(" (*)");

	// NOTE: Using a QFileDialog instead of QFileDialog::getOpenFileName()
	// causes a non-native appearance on Windows. Hence, we should use
	// QFileDialog::getOpenFileName().
	const QString filters =
		allSupportedFilter + QLatin1String(";;") +
		hddFilter + QLatin1String(";;") +
		gcmFilter + QLatin1String(";;") +
		allFilter;

	// Get the filename.
	// TODO: d->lastPath()
	QString filename = QFileDialog::getOpenFileName(this,
			tr("Open RVT-H Reader Disk Image"),	// Dialog title
			d->lastPath,				// Default filename
			filters);				// Filters

	if (!filename.isEmpty()) {
		// Filename is selected.

		// Save the last path.
		d->lastPath = QFileInfo(filename).absolutePath();

		// Open the RVT-H Reader disk image.
		openRvtH(filename);
	}
}

/**
 * Close the currently-opened memory card image.
 */
void QRvtHToolWindow::on_actionClose_triggered(void)
{
	Q_D(QRvtHToolWindow);
	if (!d->rvth)
		return;

	closeRvtH();
}

/**
 * Exit the program.
 * TODO: Separate close/exit for Mac OS X?
 */
void QRvtHToolWindow::on_actionExit_triggered(void)
{
	this->closeRvtH();
	this->close();
}

/**
 * Show the About dialog.
 */
void QRvtHToolWindow::on_actionAbout_triggered(void)
{
	// TODO
	//AboutDialog::ShowSingle(this);
}

/** RvtHModel slots. **/

void QRvtHToolWindow::rvthModel_layoutChanged(void)
{
	// Update the QTreeView columns, etc.
	// FIXME: This doesn't work the first time a file is added...
	// (possibly needs a dataChanged() signal)
	Q_D(QRvtHToolWindow);
	d->updateLstBankList();
}

void QRvtHToolWindow::rvthModel_rowsInserted(void)
{
	// A new file entry was added to the GcnCard.
	// Update the QTreeView columns.
	// FIXME: This doesn't work the first time a file is added...
	Q_D(QRvtHToolWindow);
	d->updateLstBankList();
}

/** lstBankList slots. **/
void QRvtHToolWindow::lstBankList_selectionModel_selectionChanged(
	const QItemSelection& selected, const QItemSelection& deselected)
{
	Q_UNUSED(selected)
	Q_UNUSED(deselected)
	Q_D(QRvtHToolWindow);

	if (!d->rvth) {
		// No RVT-H Reader disk image.
		d->ui.bevBankEntryView->setBankEntry(nullptr);
		return;
	}

	// FIXME: QItemSelection::indexes() *crashes* in MSVC debug builds. (Qt 4.8.6)
	// References: (search for "QModelIndexList assertion", no quotes)
	// - http://www.qtforum.org/article/13355/qt4-qtableview-assertion-failure.html#post66572
	// - http://www.qtcentre.org/threads/55614-QTableView-gt-selectionModel%20%20-gt-selection%20%20-indexes%20%20-crash#8766774666573257762
	// - https://forum.qt.io/topic/24664/crash-with-qitemselectionmodel-selectedindexes
	//QModelIndexList indexes = selected.indexes();
	int bank = -1;
	const RvtH_BankEntry *entry = nullptr;
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (selectionModel->hasSelection()) {
		// TODO: If multiple banks are selected, and one of the
		// banks was just now unselected, this will still be the
		// unselected bank.
		QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
		if (index.isValid()) {
			// TODO: Sort proxy model like in mcrecover.
			bank = d->proxyModel->mapToSource(index).row();
			// TODO: Check for errors?
			entry = d->rvth->bankEntry(bank, nullptr);
		}
	}

	// Set the BankView's BankEntry to the selected bank.
	// NOTE: Only handles the first selected bank.
	d->ui.bevBankEntryView->setBankEntry(entry);
}