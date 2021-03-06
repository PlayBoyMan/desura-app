/*
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)
Copyright (C) 2014 Bad Juju Games, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.

Contact us at legal@badjuju.com.

*/

#include "Common.h"
#include "CIPManager.h"

#include "sqlite3x.hpp"
#include "sql/CustomInstallPathSql.h"

#include "User.h"
#include "GetItemListThread.h"

using namespace UserCore;


CIPManager::CIPManager(gcRefPtr<User> user)
	: m_pUser(user)
{
	createCIPDbTables(user->getAppDataPath());
	m_szDBName = getCIBDb(user->getAppDataPath());
}

CIPManager::~CIPManager()
{
	safe_delete(m_pThread);
}

void CIPManager::getCIPList(std::vector<UserCore::Misc::CIPItem> &list)
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szDBName.c_str());
		sqlite3x::sqlite3_command cmd(db, "SELECT c.internalid, c.path, i.name FROM cip as c INNER JOIN cipiteminfo as i USING(internalid);");
		sqlite3x::sqlite3_reader reader = cmd.executereader();

		while (reader.read())
		{
			UserCore::Misc::CIPItem cip;

			cip.id = DesuraId(reader.getint64(0));
			cip.path = reader.getstring(1);
			cip.name = reader.getstring(2);

			if (cip.path.size() == 0)
				continue;

			list.push_back(cip);
		}
	}
	catch(std::exception &ex)
	{
		Warning("Failed to update cip (loadItems) {0}\n", ex.what());
	}
}

void CIPManager::getItemList(std::vector<UserCore::Misc::CIPItem> &list)
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szDBName.c_str());
		sqlite3x::sqlite3_command cmd(db, "select * from cipiteminfo;");
		sqlite3x::sqlite3_reader reader = cmd.executereader();

		while (reader.read())
		{
			UserCore::Misc::CIPItem cip;

			cip.id = DesuraId(reader.getint64(0));
			cip.name =  reader.getstring(1);

			list.push_back(cip);
		}
	}
	catch(std::exception &ex)
	{
		Warning("Failed to update cip (loadItems) {0}\n", ex.what());
	}

	std::vector<gcRefPtr<UserCore::Item::ItemInfoI>> items;
	m_pUser->getItemManager()->getGameList(items);

	for (auto i : items)
	{
		bool found = false;

		for (auto t : list)
		{
			if (t.id == i->getId())
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			UserCore::Misc::CIPItem cip;

			cip.id = i->getId();
			cip.name = i->getName();

			list.push_back(cip);
		}
	}
}

void CIPManager::updateItem(DesuraId id, gcString path)
{
	if (path.size() == 0)
	{
		deleteItem(id);
		return;
	}

	try
	{
		sqlite3x::sqlite3_connection db(m_szDBName.c_str());
		sqlite3x::sqlite3_command cmd(db, "REPLACE INTO cip (internalid, path) VALUES (?,?);");

		cmd.bind(1, (long long int)id.toInt64());
		cmd.bind(2, path);
		cmd.executenonquery();
	}
	catch (std::exception &e)
	{
		Warning("Failed to save cip: {0}\n", e.what());
	}
}

void CIPManager::deleteItem(DesuraId id)
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szDBName.c_str());
		sqlite3x::sqlite3_command cmd(db, "DELETE FROM cip WHERE internalid=?;");
		cmd.bind(1, (long long int)id.toInt64());
		cmd.executenonquery();
	}
	catch (...)
	{
	}
}

void CIPManager::refreshList()
{
	if (m_bRefreshComplete)
		safe_delete(m_pThread);

	m_bRefreshComplete = false;

	if (m_pThread)
		return;

	m_pThread = m_pUser->getThreadManager()->newGetItemListThread();

	m_pThread->getErrorEvent() += delegate(this, &CIPManager::onRefreshError);
	m_pThread->getCompleteEvent() += delegate(this, &CIPManager::onRefreshComplete);

	m_pThread->start();
}

EventV& CIPManager::getItemsUpdatedEvent()
{
	return onItemsUpdatedEvent;
}

void CIPManager::onRefreshComplete(uint32&)
{
	onItemsUpdatedEvent();
	m_bRefreshComplete = true;
}

void CIPManager::onRefreshError(gcException& e)
{
	Warning("Refresh CIP had critical error: {0}\n", e);
	onItemsUpdatedEvent();
	m_bRefreshComplete = true;
}

bool CIPManager::getCIP(UserCore::Misc::CIPItem& info)
{
	try
	{
		sqlite3x::sqlite3_connection db(m_szDBName.c_str());

		gcString q("SELECT path from cip WHERE internalid='{0}';", info.id.toInt64());
		std::string res = db.executestring(q.c_str());

		if (res.size() > 0)
		{
			info.path = gcString(res);
			return true;
		}
	}
	catch(std::exception &)
	{
	}

	return false;
}
