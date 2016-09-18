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
		//MSG_WM_COMMAND(OnCommand) // TODO: use COMMAND_HANDLER_EX instead
		COMMAND_CODE_HANDLER(BN_CLICKED, OnButtonClick)
		COMMAND_HANDLER_EX(ID_CREATE_ROOM_BUTTON, BN_CLICKED, OnCreateRoom)
		COMMAND_HANDLER_EX(ID_JOIN_ROOM_BUTTON, BN_CLICKED, OnJoinRoom)
		COMMAND_HANDLER_EX(ID_SYNC_PL_BUTTON, BN_CLICKED, OnSyncPlaylist)
		COMMAND_HANDLER_EX(ID_SETTINGS_BUTTON, BN_CLICKED, OnSettings)
	END_MSG_MAP()

	DECLARE_WND_CLASS_EX(TEXT("{25821DB8-4BFE-42E8-9D1C-DB284C367102}"), CS_VREDRAW | CS_HREDRAW, (-1));

	sync_room_window(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback) : m_callback(p_callback), m_config(config), m_manager(sync_manager::get_instance()) {
		m_manager.register_update_window_callback(std::bind(&sync_room_window::update_sync_room_list, this));
		last_clicked = nullptr;
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

	void OnCreateRoom(UINT, int, CWindow) {
		console::print("Clicked CREATE ROOM");
		last_clicked = &m_create_room_button;
		m_create_room_diag.DoModal();
	}

	void OnJoinRoom(UINT, int, CWindow) {
		console::print("Clicked JOIN ROOM");
		last_clicked = &m_join_room_button;
		int idx = m_sync_room_listbox.GetCurSel();
		if (idx == LB_ERR) return;
		auto & room_name = m_sync_room_name_table[idx];
		m_manager.join_sync_room(room_name);
		update_sync_room_list();
	}

	void OnSyncPlaylist(UINT, int, CWindow) {
		pfc::list_t<metadb_handle_ptr> complete_plist;
		static_api_ptr_t<playlist_manager>()->activeplaylist_get_all_items(complete_plist);
		m_manager.share_playlist_as_torrent_async(complete_plist);
	}

	void OnSettings(UINT, int, CWindow) {
		last_clicked = &m_settings_button;
	}

	LRESULT OnButtonClick(UINT, int, CWindow, BOOL & bHandled) {
		if (last_clicked) {
			last_clicked->SetButtonStyle(BS_PUSHBUTTON, 1);
			last_clicked->SetState(0);
		}
		bHandled = false;
		return 0;
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

		m_sync_room_listbox.Create(*this, CRect(0, 0, 372, 250), 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_GROUP);
		update_sync_room_list();

		int x = 0, y = 270;
		CRect button_rect(x, y, x + BUTTON_WIDTH, y + BUTTON_HEIGHT);
		auto rect_next = [&button_rect]() {
			button_rect.left += BUTTON_WIDTH + BUTTON_GAP;
			button_rect.right += BUTTON_WIDTH + BUTTON_GAP;
		};

		m_create_room_button.Create(*this, button_rect, TEXT("Create room..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP, 0, ID_CREATE_ROOM_BUTTON);
		rect_next();
		m_join_room_button.Create(*this, button_rect, TEXT("Join room"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, ID_JOIN_ROOM_BUTTON);
		rect_next();
		m_sync_pl_button.Create(*this, button_rect, TEXT("Sync playlist"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, ID_SYNC_PL_BUTTON);
		rect_next();
		m_settings_button.Create(*this, button_rect, TEXT("Settings..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, ID_SETTINGS_BUTTON);

		SetFont(font);

		m_sync_room_listbox.SetFont(font);
		m_create_room_button.SetFont(font);
		m_join_room_button.SetFont(font);
		m_sync_pl_button.SetFont(font);
		m_settings_button.SetFont(font);

		//SetWindowPos(HWND_TOPMOST, 0, 0, 380, 300, SWP_NOMOVE); // Has no effect

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
	CButton m_sync_pl_button;
	CButton m_settings_button;
	CButton * last_clicked;
	CCreateRoomDiag m_create_room_diag;

	std::unordered_map<int, std::string> m_sync_room_name_table;

	enum {ID_CREATE_ROOM_BUTTON = 7, ID_JOIN_ROOM_BUTTON, ID_SYNC_PL_BUTTON, ID_SETTINGS_BUTTON};
	static const int BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_GAP;
protected:
	const ui_element_instance_callback_ptr m_callback;
};

const int sync_room_window::BUTTON_WIDTH = 90;
const int sync_room_window::BUTTON_HEIGHT = 30;
const int sync_room_window::BUTTON_GAP = 4;

// ui_element_impl_withpopup autogenerates standalone version of our component and proper menu commands. Use ui_element_impl instead if you don't want that.
class ui_element_sync_room_window : public ui_element_impl_withpopup<sync_room_window> {};

static service_factory_single_t<ui_element_sync_room_window> g_ui_element_sync_room_window_factory;
