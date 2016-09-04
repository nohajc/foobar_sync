#include "stdafx.h"
#include "resource.h"

class COpenTorrentDiag : public CDialogImpl<COpenTorrentDiag> {
public:
	enum { IDD = IDD_OPEN_TORRENT };

	BEGIN_MSG_MAP(COpenTorrentDiag)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnOk);
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel);
	END_MSG_MAP()

	void OnOk(UINT, int, CWindow) {
		pfc::string8 path;
		uGetDlgItemText(*this, IDC_TORRENT_PATH, path);
		console::print(path);
	}

	void OnCancel(UINT, int, CWindow) {
		DestroyWindow();
	}

	BOOL OnInitDialog(CWindow, LPARAM) {
		::SetFocus(GetDlgItem(IDC_TORRENT_PATH));
		::ShowWindowCentered(*this, GetParent());
		return FALSE;
	}
};

void showOpenTorrentDiag() {
	try {
		new CWindowAutoLifetime<ImplementModelessTracking<COpenTorrentDiag> >(core_api::get_main_window());
	}
	catch (std::exception const & e) {
		popup_message::g_complain("Dialog creation failure", e);
	}
}