#include "stdafx.h"
#include "resource.h"
#include "sync_manager.h"
#include "string_helpers.h"

#include "create_room_dialog.h"

#include <functional>
#include <unordered_map>

/*class sync_room_frame : public CDialogImpl<sync_room_frame> {
public:
	enum { IDD = IDD_SYNC_ROOM };

	BEGIN_MSG_MAP(COpenTorrentDiag)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow, LPARAM) {
		//::ShowWindowCentered(m_parent, GetParent().GetParent());
		return FALSE;
	}
};*/

class sync_room_window : public ui_element_instance, public CWindowImpl<sync_room_window>, public CMessageFilter {
public:
	BEGIN_MSG_MAP(sync_room_window)
		MSG_WM_ERASEBKGND(OnEraseBkgnd)
		MSG_WM_PAINT(OnPaint)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_COMMAND(OnCommand) // TODO: use COMMAND_HANDLER_EX instead
	END_MSG_MAP()

	DECLARE_WND_CLASS_EX(TEXT("{25821DB8-4BFE-42E8-9D1C-DB284C367102}"), CS_VREDRAW | CS_HREDRAW, (-1));

	sync_room_window(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback) : m_callback(p_callback), m_config(config), m_manager(sync_manager::get_instance()) {
		m_manager.register_update_window_callback(std::bind(&sync_room_window::update_sync_room_list, this));
	}

	void initialize_window(HWND parent) {
		WIN32_OP(Create(parent, 0, 0, 0) != NULL);
		// WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU
	}

	HWND get_wnd() { return *this; }
	void set_configuration(ui_element_config::ptr config) { m_config = config; }
	ui_element_config::ptr get_configuration() { return m_config; }

	static GUID g_get_guid() {
		static const GUID guid_myelem = { 0x0e951b38, 0x85df, 0x45d5,{ 0xb7, 0x62, 0x35, 0xa2, 0x74, 0x54, 0x55, 0x26 } };
		return guid_myelem;
	}
	static GUID g_get_subclass() { return ui_element_subclass_utility; }
	static void g_get_name(pfc::string_base & out) { out = "Sync rooms"; }
	static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
	static const char * g_get_description() { return "Sync room manager."; }

	void notify(const GUID & p_what, t_size p_param1, const void * p_param2, t_size p_param2size) {
		if (p_what == ui_element_notify_colors_changed || p_what == ui_element_notify_font_changed) {
			// we use global colors and fonts - trigger a repaint whenever these change.
			Invalidate();
		}
	}

	BOOL PreTranslateMessage(MSG *msg) override {
		return IsDialogMessage(msg);
	}

	void OnCommand(UINT wParamHi, UINT wParamLo, HWND hWnd) {
		if (wParamHi == BN_CLICKED) {
			switch (wParamLo) {
			case CREATE_ROOM_BUTTON:
				console::print("Clicked CREATE ROOM");
				m_create_room_diag.DoModal();
				break;
			case JOIN_ROOM_BUTTON: {
				console::print("Clicked JOIN ROOM");
				int idx = m_sync_room_listbox.GetCurSel();
				auto & room_name = m_sync_room_name_table[idx];
				m_manager.join_sync_room(room_name);
				update_sync_room_list();
			}
					
				break;
			case SETTINGS_ROOM_BUTTON:
				console::print("Clicked SETTINGS");
				break;
			}
		}
	}

	BOOL OnEraseBkgnd(CDCHandle dc) {
		CRect rc; WIN32_OP_D(GetClientRect(&rc));
		CBrush brush;
		WIN32_OP_D(brush.CreateSolidBrush(m_callback->query_std_color(ui_color_background)) != NULL);
		WIN32_OP_D(dc.FillRect(&rc, brush));
		return TRUE;
	}
	void OnPaint(CDCHandle) {
		CPaintDC dc(*this);
		CRect rc; WIN32_OP_D(GetClientRect(&rc));
		CBrush brush;
		//dc.SetTextColor(m_callback->query_std_color(ui_color_text));
		//dc.SetBkMode(TRANSPARENT);
		WIN32_OP_D(brush.CreateSolidBrush(m_callback->query_std_color(ui_color_background)) != NULL);
		WIN32_OP_D(dc.FillRect(&rc, brush));
	}

	LRESULT OnCreate(LPCREATESTRUCT param) {
		t_ui_font font = m_callback->query_font_ex(ui_font_default);

		m_sync_room_listbox.Create(*this, CRect(0, 0, 380, 250), 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_GROUP);
		update_sync_room_list();

		m_create_room_button.Create(*this, CRect(0, 270, 120, 300), TEXT("Create room..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP, 0, CREATE_ROOM_BUTTON);
		m_join_room_button.Create(*this, CRect(130, 270, 250, 300), TEXT("Join room"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, JOIN_ROOM_BUTTON);
		m_settings_room_button.Create(*this, CRect(260, 270, 380, 300), TEXT("Settings..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, SETTINGS_ROOM_BUTTON);

		SetFont(font);

		m_sync_room_listbox.SetFont(font);
		m_create_room_button.SetFont(font);
		m_join_room_button.SetFont(font);
		m_settings_room_button.SetFont(font);

		return 0;
	}

	void update_sync_room_list() {
		auto sr_list = m_manager.get_sync_room_list();

		m_sync_room_listbox.ResetContent();
		m_sync_room_name_table.clear();
		for (auto & r : sr_list) {
			std::string item;
			if (m_manager.sync_room_is_joined(r)) {
				item = r + " (joined)";
			}
			else {
				item = r;
			}
			int idx = m_sync_room_listbox.AddString(stringToWstring(item).c_str());
			m_sync_room_name_table[idx] = r;
		}
	}
private:
	ui_element_config::ptr m_config;
	
	sync_manager & m_manager;
	
	CListBox m_sync_room_listbox;
	CButton m_create_room_button;
	CButton m_join_room_button;
	CButton m_settings_room_button;
	CCreateRoomDiag m_create_room_diag;

	std::unordered_map<int, std::string> m_sync_room_name_table;

	enum {CREATE_ROOM_BUTTON = 7, JOIN_ROOM_BUTTON, SETTINGS_ROOM_BUTTON};
protected:
	const ui_element_instance_callback_ptr m_callback;
};

// ui_element_impl_withpopup autogenerates standalone version of our component and proper menu commands. Use ui_element_impl instead if you don't want that.
class ui_element_sync_room_window : public ui_element_impl_withpopup<sync_room_window> {};

static service_factory_single_t<ui_element_sync_room_window> g_ui_element_sync_room_window_factory;
