#pragma once

#include "stdafx.h"
#include "resource.h"

class CCreateRoomDiag : public CDialogImpl<CCreateRoomDiag> {
public:
	enum { IDD = IDD_CREATE_ROOM };

	BEGIN_MSG_MAP(CCreateRoomDiag)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnOk);
	COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel);
	END_MSG_MAP()

	void OnOk(UINT, int, CWindow);
	void OnCancel(UINT, int, CWindow);
	BOOL OnInitDialog(CWindow, LPARAM);
};