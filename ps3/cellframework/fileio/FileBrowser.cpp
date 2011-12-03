/******************************************************************************* 
 *  -- Cellframework -  Open framework to abstract the common tasks related to
 *                      PS3 application development.
 *
 *  Copyright (C) 2010-2011
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ********************************************************************************/

/******************************************************************************* 
 * FileBrowser.cpp - Cellframework
 *
 *  Created on:		Oct 29, 2010
 *  Last updated:	 
 ********************************************************************************/

#include <stdlib.h>
#include <algorithm>
#include "FileBrowser.hpp"

static bool less_than_key(CellFsDirent* a, CellFsDirent* b)
{
	// compare a directory to a file
	// directory is always lesser than
	if ((a->d_type == CELL_FS_TYPE_DIRECTORY && b->d_type == CELL_FS_TYPE_REGULAR))
		return true;
	else if (a->d_type == CELL_FS_TYPE_REGULAR && b->d_type == CELL_FS_TYPE_DIRECTORY)
		return false;

	return strcasecmp(a->d_name, b->d_name) < 0;
}

static const char * filebrowser_get_extension(const char * filename)
{
	const char * ext = strrchr(filename, '.');
	if (ext)
		return ext+1;
	else
		return "";
}

static char * strndup (const char *s, size_t n)
{
	char *result;
	size_t len = strlen (s);

	if (n < len)
		len = n;

	result = (char *) malloc (len + 1);
	if (!result)
		return 0;

	result[len] = '\0';
	return (char *) memcpy (result, s, len);
}

static char * substr(const char * str, size_t begin, size_t len)
{
	if (str == 0 || strlen(str) == 0 || strlen(str) < begin || strlen(str) < (begin+len))
		return 0;

	return strndup(str + begin, len);
}

static bool filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, std::string extensions)
{
	int fd;
	// for extension parsing
	uint32_t index = 0;
	uint32_t lastIndex = 0;

	// bad path
	if (strcmp(path,"") == 0)
		return false;

	// delete old path
	if (!filebrowser->cur.empty())
	{
		std::vector<CellFsDirent*>::const_iterator iter;
		for(iter = filebrowser->cur.begin(); iter != filebrowser->cur.end(); ++iter)
			delete (*iter);
		filebrowser->cur.clear();
	}

	// FIXME: add FsStat calls or use cellFsDirectoryEntry
	if (cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		uint64_t nread = 0;

		// set new dir
		strcpy(filebrowser->dir[filebrowser->directory_stack_size].dir, path);
		filebrowser->dir[filebrowser->directory_stack_size].extensions = extensions;

		// reset num entries
		filebrowser->file_count = 0;

		// reset currently selected variable for safety
		filebrowser->currently_selected = 0;

		// read the directory
		CellFsDirent dirent;
		while (cellFsReaddir(fd, &dirent, &nread) == CELL_FS_SUCCEEDED)
		{
			// no data read, something is wrong
			if (nread == 0)
				break;

			// check for valid types
			if ((dirent.d_type != CELL_FS_TYPE_REGULAR) && (dirent.d_type != CELL_FS_TYPE_DIRECTORY))
				continue;

			// skip cur dir
			if (dirent.d_type == CELL_FS_TYPE_DIRECTORY && !(strcmp(dirent.d_name, ".")))
				continue;

			// validate extensions
			if (dirent.d_type == CELL_FS_TYPE_REGULAR)
			{
				// reset indices from last search
				index = 0;
				lastIndex = 0;

				// get this file extension
				const char * ext = filebrowser_get_extension(dirent.d_name);

				// assume to skip it, prove otherwise
				bool bSkip = true;

				size_t ext_len = strlen(extensions.c_str());
				// build the extensions to compare against
				if (ext_len > 0)
				{
					index = extensions.find('|', 0);

					// only 1 extension
					if (index == std::string::npos)
					{
						if (strcmp(ext, extensions.c_str()) == 0)
							bSkip = false;
					}
					else
					{
						lastIndex = 0;
						index = extensions.find('|', 0);
						while (index != std::string::npos)
						{
							char * tmp = substr(extensions.c_str(), lastIndex, (index-lastIndex));
							if (strcmp(ext, tmp) == 0)
							{
								bSkip = false;
								free(tmp);
								break;
							}
							free(tmp);

							lastIndex = index + 1;
							index = extensions.find('|', index+1);
						}

						// grab the final extension
						const char * tmp = strrchr(extensions.c_str(), '|');
						if (strcmp(ext, tmp+1) == 0)
							bSkip = false;
					}
				}
				else
					bSkip = false; // no extensions we'll take as all extensions

				if (bSkip)
					continue;
			}

			// AT THIS POINT WE HAVE A VALID ENTRY

			// allocate an entry
			CellFsDirent *entry = new CellFsDirent();
			memcpy(entry, &dirent, sizeof(CellFsDirent));

			filebrowser->cur.push_back(entry);

			// next file
			++filebrowser->file_count;
		}

		cellFsClosedir(fd);
	}
	else
		return false;

	std::sort(++filebrowser->cur.begin(), filebrowser->cur.end(), less_than_key);

	return true;
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, std::string extensions)
{
	filebrowser->currently_selected = 0;
	filebrowser->directory_stack_size = 0;

	filebrowser_parse_directory(filebrowser, start_dir, extensions);
}

void filebrowser_reset_start_directory(filebrowser_t * filebrowser, const char * start_dir, std::string extensions)
{
	std::vector<CellFsDirent*>::const_iterator iter;
	for(iter = filebrowser->cur.begin(); iter != filebrowser->cur.end(); ++iter)
		delete (*iter);
	filebrowser->cur.clear();
   
	filebrowser->currently_selected = 0;
	filebrowser->directory_stack_size = 0;

	filebrowser_parse_directory(filebrowser, start_dir, extensions);
}

void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, std::string extensions)
{
	filebrowser->directory_stack_size++;
	filebrowser_parse_directory(filebrowser, path, extensions);
}


void filebrowser_pop_directory (filebrowser_t * filebrowser)
{
	if (filebrowser->directory_stack_size > 0)
		filebrowser->directory_stack_size--;

	filebrowser_parse_directory(filebrowser, filebrowser->dir[filebrowser->directory_stack_size].dir, filebrowser->dir[filebrowser->directory_stack_size].extensions);
}
