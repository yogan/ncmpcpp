bin_PROGRAMS = ncmpcpp
ncmpcpp_SOURCES = browser.cpp charset.cpp clock.cpp conv.cpp display.cpp \
	error.cpp help.cpp helpers.cpp info.cpp lyrics.cpp media_library.cpp menu.cpp \
	mpdpp.cpp ncmpcpp.cpp outputs.cpp playlist.cpp playlist_editor.cpp scrollpad.cpp \
	search_engine.cpp sel_items_adder.cpp server_info.cpp settings.cpp song.cpp status.cpp \
	tag_editor.cpp tiny_tag_editor.cpp tolower.cpp visualizer.cpp window.cpp

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
ncmpcpp_LDFLAGS = $(all_libraries)
noinst_HEADERS = browser.h charset.h clock.h conv.h display.h error.h global.h \
	help.h helpers.h home.h info.h lyrics.h media_library.h menu.h mpdpp.h \
	outputs.h playlist_editor.h screen.h scrollpad.h search_engine.h \
	sel_items_adder.h server_info.h settings.h song.h tag_editor.h tiny_tag_editor.h \
	tolower.h visualizer.h window.h
