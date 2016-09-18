#include "stdafx.h"
#include "resource.h"
#include "sync_manager.h"

#include "create_room_dialog.h"


void CCreateRoomDiag::OnOk(UINT, int, CWindow) {
	pfc::string8 room_name;
	uGetDlgItemText(*this, IDC_ROOM_NAME, room_name);
	console::print(room_name);

	sync_manager & sm = sync_manager::get_instance();
	// Create room, join it
	sm.create_sync_room(room_name.c_str());
	EndDialog(0);
}

void CCreateRoomDiag::OnCancel(UINT, int, CWindow) {
	EndDialog(1);
}

BOOL CCreateRoomDiag::OnInitDialog(CWindow, LPARAM) {
	::SetFocus(GetDlgItem(IDC_ROOM_NAME));
	::ShowWindowCentered(*this, GetParent());
	return FALSE;
}