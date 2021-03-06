/***************************************************************************
 *   Copyright (C) 2008-2010 by Andrzej Rybczak                            *
 *   electricityispower@gmail.com                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include <cerrno>
#include <cstring>
#ifdef WIN32
# include <io.h>
#else
# include <sys/stat.h>
#endif // WIN32
#include <fstream>

#include "lyrics.h"

#include "browser.h"
#include "charset.h"
#include "global.h"
#include "helpers.h"

#include "media_library.h"
#include "playlist.h"
#include "playlist_editor.h"
#include "search_engine.h"
#include "settings.h"
#include "song.h"
#include "status.h"
#include "tag_editor.h"

#ifdef WIN32
# define LYRICS_FOLDER HOME_FOLDER"\\lyrics\\"
#else
# define LYRICS_FOLDER "/.lyrics"
#endif // WIN32

using Global::MainHeight;
using Global::MainStartY;
using Global::myScreen;
using Global::myOldScreen;

const std::string Lyrics::Folder = home_path + LYRICS_FOLDER;

bool Lyrics::Reload = 0;

#ifdef HAVE_CURL_CURL_H
bool Lyrics::Ready = 0;

#ifdef HAVE_PTHREAD_H
pthread_t *Lyrics::Downloader = 0;
pthread_mutex_t Global::CurlLock = PTHREAD_MUTEX_INITIALIZER;
#endif // HAVE_PTHREAD_H
#endif // HAVE_CURL_CURL_H

Lyrics *myLyrics = new Lyrics;

void Lyrics::Init()
{
	w = new Scrollpad(0, MainStartY, COLS, MainHeight, "", Config.main_color, brNone);
	isInitialized = 1;
}

void Lyrics::Resize()
{
	w->Resize(COLS, MainHeight);
	w->MoveTo(0, MainStartY);
	hasToBeResized = 0;
}

void Lyrics::Update()
{
#	if defined(HAVE_CURL_CURL_H) && defined(HAVE_PTHREAD_H)
	if (myLyrics->Ready)
		myLyrics->Take();
#	endif // HAVE_CURL_CURL_H && HAVE_PTHREAD_H
	
	if (Reload)
		SwitchTo();
}

void Lyrics::SwitchTo()
{
	if (myScreen == this && !Reload)
	{
		myOldScreen->SwitchTo();
	}
	else
	{
		if (!isInitialized)
			Init();
		
#		if defined(HAVE_CURL_CURL_H) && defined(HAVE_PTHREAD_H)
		if (Downloader && !Ready)
		{
			ShowMessage("Lyrics are being downloaded...");
			return;
		}
		else if (Ready)
			Take();
#		endif // HAVE_CURL_CURL_H && HAVE_PTHREAD_H
		
		const MPD::Song *s = Reload ? myPlaylist->NowPlayingSong() : myScreen->CurrentSong();
		
		if (!s)
			return;
		
		if (!s->GetArtist().empty() && !s->GetTitle().empty())
		{
			if (hasToBeResized)
				Resize();
			itsScrollBegin = 0;
			itsSong = *s;
			itsSong.Localize();
			if (!Reload)
			{
				myOldScreen = myScreen;
				myScreen = this;
			}
			Global::RedrawHeader = 1;
			w->Clear();
			w->Reset();
#			ifdef HAVE_CURL_CURL_H
			static_cast<Window &>(*w) << "Fetching lyrics...";
			w->Window::Refresh();
#			ifdef HAVE_PTHREAD_H
			if (!Downloader)
#			endif // HAVE_PTHREAD_H
#			endif // HAVE_CURL_CURL_H
			{
				std::string file = locale_to_utf_cpy(itsSong.GetArtist()) + " - " + locale_to_utf_cpy(itsSong.GetTitle()) + ".txt";
				EscapeUnallowedChars(file);
				itsFilenamePath = Folder + "/" + file;
				
				mkdir(Folder.c_str()
#				ifndef WIN32
				, 0755
#				endif // !WIN32
				);
				
				std::ifstream input(itsFilenamePath.c_str());
				if (input.is_open())
				{
					bool first = 1;
					std::string line;
					while (getline(input, line))
					{
						if (!first)
							*w << "\n";
						utf_to_locale(line);
						*w << line;
						first = 0;
					}
					w->Flush();
					if (Reload)
						w->Refresh();
				}
				else
				{
#					if defined(HAVE_CURL_CURL_H) && defined(HAVE_PTHREAD_H)
					Downloader = new pthread_t;
					pthread_create(Downloader, 0, Get, this);
#					elif defined(HAVE_CURL_CURL_H)
					Get(this);
					w->Flush();
#					else
					*w << "Local lyrics not found. As ncmpcpp has been compiled without curl support, you can put appropriate lyrics into " << Folder << " directory (file syntax is \"$ARTIST - $TITLE.txt\") or recompile ncmpcpp with curl support.";
					w->Flush();
#					endif
				}
			}
		}
		Reload = 0;
	}
}

std::basic_string<my_char_t> Lyrics::Title()
{
	std::basic_string<my_char_t> result = U("Lyrics: ");
	result += Scroller(TO_WSTRING(itsSong.toString("{%a - %t}")), itsScrollBegin, w->GetWidth()-result.length()-(Config.new_design ? 2 : Global::VolumeState.length()));
	return result;
}

void Lyrics::SpacePressed()
{
	Config.now_playing_lyrics = !Config.now_playing_lyrics;
	ShowMessage("Reload lyrics if song changes: %s", Config.now_playing_lyrics ? "On" : "Off");
}

#ifdef HAVE_CURL_CURL_H
void *Lyrics::Get(void *screen_void_ptr)
{
	Lyrics *screen = static_cast<Lyrics *>(screen_void_ptr);
	const Plugin *my_lyrics = ChoosePlugin(Config.lyrics_db);
	
	std::string result;
	std::string artist = locale_to_utf_cpy(screen->itsSong.GetArtist());
	std::string title = locale_to_utf_cpy(screen->itsSong.GetTitle());
	
	char *c_artist = curl_easy_escape(0, artist.c_str(), artist.length());
	char *c_title = curl_easy_escape(0, title.c_str(), title.length());
	
	std::string url = my_lyrics->url;
	Replace(url, "%artist%", c_artist);
	Replace(url, "%title%", c_title);
	
	CURLcode code;
	pthread_mutex_lock(&Global::CurlLock);
	CURL *lyrics = curl_easy_init();
	curl_easy_setopt(lyrics, CURLOPT_URL, url.c_str());
	curl_easy_setopt(lyrics, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(lyrics, CURLOPT_WRITEDATA, &result);
	curl_easy_setopt(lyrics, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(lyrics, CURLOPT_NOSIGNAL, 1);
	code = curl_easy_perform(lyrics);
	curl_easy_cleanup(lyrics);
	pthread_mutex_unlock(&Global::CurlLock);
	
	curl_free(c_artist);
	curl_free(c_title);
	
	if (code != CURLE_OK)
	{
		*screen->w << "Error while fetching lyrics: " << curl_easy_strerror(code);
		Ready = 1;
		pthread_exit(0);
	}
	
	size_t a, b;
	bool parse_failed = 0;
	
	if ((a = result.find(my_lyrics->tag_open)) != std::string::npos)
	{
		a += strlen(my_lyrics->tag_open);
		if ((b = result.find(my_lyrics->tag_close, a)) != std::string::npos)
			result = result.substr(a, b-a);
		else
			parse_failed = 1;
	}
	else
		parse_failed = 1;
	
	if (parse_failed || my_lyrics->not_found(result))
	{
		*screen->w << "Not found";
		Ready = 1;
		pthread_exit(0);
	}
	
	Replace(result, "&lt;", "<");
	Replace(result, "&gt;", ">");
	
	EscapeHtml(result);
	Trim(result);
	
	*screen->w << utf_to_locale_cpy(result);
	
	std::ofstream output(screen->itsFilenamePath.c_str());
	if (output.is_open())
	{
		output << result;
		output.close();
	}
	Ready = 1;
	pthread_exit(0);
}
#endif // HAVE_CURL_CURL_H

void Lyrics::Edit()
{
	if (myScreen != this)
		return;
	
	if (Config.external_editor.empty())
	{
		ShowMessage("External editor is not set!");
		return;
	}
	
	ShowMessage("Opening lyrics in external editor...");
	
	if (Config.use_console_editor)
	{
		system(("/bin/sh -c \"" + Config.external_editor + " \\\"" + itsFilenamePath + "\\\"\"").c_str());
		// below is needed as screen gets cleared, but apparently
		// ncurses doesn't know about it, so we need to reload main screen
		endwin();
		initscr();
		curs_set(0);
	}
	else
		system(("nohup " + Config.external_editor + " \"" + itsFilenamePath + "\" > /dev/null 2>&1 &").c_str());
}

void Lyrics::FetchAgain()
{
	std::string path = Folder + "/" + locale_to_utf_cpy(itsSong.GetArtist()) + " - " + locale_to_utf_cpy(itsSong.GetTitle()) + ".txt";
	if (!remove(path.c_str()))
	{
		myScreen = myOldScreen;
		SwitchTo();
	}
	else
	{
		static const char msg[] = "Couldn't remove \"%s\": %s";
		ShowMessage(msg, Shorten(TO_WSTRING(path), COLS-static_strlen(msg)-25).c_str(), strerror(errno));
	}
}

#ifdef HAVE_CURL_CURL_H

#ifdef HAVE_PTHREAD_H
void Lyrics::Take()
{
	if (!Ready)
		return;
	pthread_join(*Downloader, 0);
	w->Flush();
	w->Refresh();
	delete Downloader;
	Downloader = 0;
	Ready = 0;
}
#endif // HAVE_PTHREAD_H

const unsigned Lyrics::DBs = 1; // number of currently supported lyrics databases

const char *Lyrics::PluginsList[] =
{
	"lyricsplugin.com",
	0
};

const char *Lyrics::GetPluginName(int offset)
{
	return PluginsList[offset];
}

bool Lyrics::LyricsPlugin_NotFound(const std::string &s)
{
	if  (s.empty())
		return true;
	for (std::string::const_iterator it = s.begin(); it != s.end(); ++it)
		if (isprint(*it))
			return false;
	return true;
}

const Lyrics::Plugin Lyrics::LyricsPlugin =
{
	"http://www.lyricsplugin.com/winamp03/plugin/?artist=%artist%&title=%title%",
	"<div id=\"lyrics\">",
	"</div>",
	LyricsPlugin_NotFound
};

const Lyrics::Plugin *Lyrics::ChoosePlugin(int i)
{
	switch (i)
	{
		case 0:
			return &LyricsPlugin;
		default:
			return &LyricsPlugin;
	}
}

#endif // HAVE_CURL_CURL_H

