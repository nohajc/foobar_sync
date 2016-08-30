#include "stdafx.h"
#include <Poco/Net/HTTPRequest.h>

static const GUID guid_ctxmenugroup = { 0xa20a673e, 0x38fd, 0x4b89,{ 0xaf, 0xa4, 0x49, 0x7d, 0x89, 0x9a, 0xb4, 0xdf } };

static contextmenu_group_factory g_ctxmenugroup(guid_ctxmenugroup, contextmenu_groups::root, 0);

void SharePlaylist(metadb_handle_list_cref p_data) {
	pfc::list_t<metadb_handle_ptr> complete_plist;
	static_api_ptr_t<playlist_manager>()->activeplaylist_get_all_items(complete_plist);

	console::print("Playlist (full):");
	for (t_size i = 0; i < complete_plist.get_count(); i++) {
		console::print(complete_plist[i]->get_path());
	}
}

class sharepl_item : public contextmenu_item_simple {
public:
	enum {
		cmd_share = 0,
		cmd_total
	};
	GUID get_parent() { return guid_ctxmenugroup; }
	unsigned get_num_items() { return cmd_total; }
	void get_item_name(unsigned p_index, pfc::string_base & p_out) {
		switch (p_index) {
		case cmd_share: p_out = "Share playlist"; break;
		default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}
	}
	void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) {
		switch (p_index) {
		case cmd_share:
			SharePlaylist(p_data);
			break;
		default:
			uBugCheck();
		}
	}
	GUID get_item_guid(unsigned p_index) {
		static const GUID guid_share = { 0x29a25ca8, 0xb7b4, 0x4722,{ 0xa4, 0x71, 0xa9, 0x36, 0x53, 0x6b, 0x9b, 0xdc } };
		switch (p_index) {
		case cmd_share: return guid_share;
		default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}

	}
	bool get_item_description(unsigned p_index, pfc::string_base & p_out) {
		switch (p_index) {
		case cmd_share:
			p_out = "Share this playlist online.";
			return true;
		default:
			uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}
	}
};

static contextmenu_item_factory_t<sharepl_item> g_myitem_factory;


class playlist_uploader : private playlist_callback_single_impl_base {
	void on_items_added(t_size p_base, const pfc::list_base_const_t<metadb_handle_ptr> & p_data, const bit_array & p_selection) {
		console::print("Playlist (added):");
		for (t_size i = 0; i < p_data.get_count(); i++) {
			console::print(p_data[i]->get_path());
		}
	}
};

class initquit_pl_uploader : public initquit {
	playlist_uploader * pu;
public:
	void on_init() {
		console::print("Sample component: Init playlist uploader.");
		pu = new playlist_uploader();
	}
	void on_quit() {
		delete pu;
	}
};

static initquit_factory_t<initquit_pl_uploader> g_initquit_pl_uploader_factory;