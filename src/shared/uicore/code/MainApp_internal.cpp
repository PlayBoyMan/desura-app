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
#include "MainApp.h"

#include <wx/wx.h>
#include "wx/window.h"

#include "TaskBarIcon.h"
#include "Managers.h"
#include <branding/branding.h>
#include <branding/uicore_version.h>
#include "mcfcore/MCFMain.h"

#include "InternalLink.h"

#include "Log.h"

#include "managers/CVar.h"
#include "MainForm.h"

CVar gc_firsttime("gc_firsttime", "1", CFLAG_USER);
CVar gc_enable_news_popups("gc_enable_news_popups", "1", CFLAG_USER);

bool admin_cb(CVar* var, const char* val)
{
	return (GetUserCore() && GetUserCore()->isAdmin());
}

CVar admin_autoupdate("admin_autoupdate", "0", CFLAG_ADMIN, (CVarCallBackFn)&admin_cb);

CONCOMMAND(shownews, "shownews")
{
	g_pMainApp->showNews();
}


void MainApp::handleInternalLink(const char* link)
{
	gcTrace("Link: {0}", link);

	gcString strLink(link);
	onInternalLinkStrEvent(strLink);
}

void MainApp::handleInternalLink(DesuraId id, uint8 action, const LinkArgs& args)
{
	gcTrace("Id: {0}, Action: {1}", id, action);

	InternalLinkInfo ili;

	ili.action = action;
	ili.id = id;
	ili.args = args;

	onInternalLinkEvent(ili);
}

void MainApp::onInternalStrLink(gcString &link)
{
	if (isOffline())
	{
		gcMessageBox(this->getMainWindow(), Managers::GetString(L"#MF_OFFLINEWARN"), PRODUCT_NAME_CATW(L" Error"));
		return;
	}

	if (this->isLoggedIn())
		m_pInternalLink->handleInternalLink(link.c_str());
	else
		m_szDesuraCache = link;
}

void MainApp::onInternalLink(InternalLinkInfo& info)
{
	DesuraId id = info.id;
	uint8 action = info.action;

	switch (action)
	{
	case ACTION_PROFILE		: showProfile( id, info.args );			break;
	case ACTION_DEVPROFILE	: showDevProfile( id );					break;
	case ACTION_DEVPAGE		: showDevPage( id );					break;
	case ACTION_ACCOUNTSTAT	: changeAccountState(id);				break;
	case ACTION_PLAY		: showPlay();							break;
	case ACTION_SHOWCONSOLE	: showConsole();						break;

	default:
		m_pInternalLink->handleInternalLink(id, action, info.args);
		break;
	}

	showNews();
}

void MainApp::showConsole()
{
#ifdef WIN32
	ShowLogForm(true);
#endif // LINUX TODO
}

void MainApp::closeForm(int32 wxId)
{
	if (m_pInternalLink)
		m_pInternalLink->closeForm(wxId);
}

void MainApp::showPlay()
{
	if (!m_wxMainForm)
		return;

	if (m_wxMainForm->IsIconized())
		m_wxMainForm->Iconize(false);

	m_wxMainForm->showPlay();
	m_wxMainForm->Raise();
}

void MainApp::showPage(PAGE page)
{
	if (!m_wxMainForm)
		return;

	if (m_wxMainForm->IsIconized())
		m_wxMainForm->Iconize(false);

	m_wxMainForm->showPage(page);
	m_wxMainForm->Raise();
}

void MainApp::loadUrl(const char* url, PAGE page)
{
	if (!m_wxMainForm)
	{
		gcLaunchDefaultBrowser(url);
		return;
	}

	if (m_wxMainForm->IsIconized())
		m_wxMainForm->Iconize(false);

	m_wxMainForm->loadUrl(url, page);
	m_wxMainForm->Raise();
}



void MainApp::showProfile(DesuraId id, const LinkArgs &args)
{
	gcRefPtr<UserCore::Item::ItemInfoI> item = GetUserCore()->getItemManager()->findItemInfo( id );

	if (item && item->getProfile())
	{
		gcString url(item->getProfile());

		if (!args.empty())
			url = gcString("{0}{1}", item->getProfile(), args.first());

		PAGE page = GAMES;

#ifndef UI_HIDE_MODS
		if (id.getType() != DesuraId::TYPE_GAME)
			page = MODS;
#endif

		loadUrl(url.c_str(), page);
	}
	else
	{
		Warning("Failed to locate item {0} to show profile.\n", id.toInt64());
	}
}

void MainApp::showDevProfile(DesuraId id)
{
	gcRefPtr<UserCore::Item::ItemInfoI> item = GetUserCore()->getItemManager()->findItemInfo( id );

	if (item && item->getDevProfile())
	{
		loadUrl(item->getDevProfile(), COMMUNITY);
	}
	else
	{
		Warning("Failed to locate item {0} to show dev profile.\n", id.toInt64());
	}
}

void MainApp::showDevPage(DesuraId id)
{
	gcRefPtr<UserCore::Item::ItemInfoI> item = GetUserCore()->getItemManager()->findItemInfo( id );

	if (!item)
		return;

	gcString url("{0}/{1}/publish/{2}", GetWebCore()->getUrl(WebCore::Root), id.getTypeString(), item->getShortName());
	loadUrl(url.c_str(), DEVELOPMENT);
}

void MainApp::showNews()
{
	if (gc_firsttime.getBool())
	{
		gc_firsttime.setValue(false);
		return;
	}

	std::vector<gcRefPtr<UserCore::Misc::NewsItem>> news_items_vec;
	std::lock_guard<std::mutex> guard(m_NewsLock);

	if (gc_enable_news_popups.getBool())
		news_items_vec = m_vNewsItems;

	m_pInternalLink->showNews(news_items_vec, m_vGiftItems);

	m_vNewsItems.clear();
	m_vGiftItems.clear();
}

void MainApp::changeAccountState(DesuraId id)
{
	gcRefPtr<UserCore::Item::ItemInfoI> item = GetUserCore()->getItemManager()->findItemInfo( id );

	if (!item)
	{
		Warning("Could not find item {0} for account status change!", id.toInt64());
		return;
	}

	if (item->getStatus() & UserCore::Item::ItemInfoI::STATUS_ONACCOUNT)
		item->removeFromAccount();
	else
		item->addToAccount();
}









